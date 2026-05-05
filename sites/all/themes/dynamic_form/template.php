<?php

/**
 * @file
 * Theme preprocess functions for the Dynamic Form Builder theme.
 */

/**
 * Implements hook_preprocess_page().
 *
 * Adds body classes for page-specific styling (login, register, front).
 */
function dynamic_form_preprocess_page(&$variables) {
  $path = current_path();

  if ($path === 'login' || $path === 'user/login') {
    $variables['classes_array'][] = 'page-login';
  }
  elseif ($path === 'register' || $path === 'user/register') {
    $variables['classes_array'][] = 'page-register';
  }

  if (strpos($path, 'dashboard') === 0) {
    $variables['classes_array'][] = 'page-dashboard';
  }

  if (drupal_is_front_page()) {
    $variables['classes_array'][] = 'page-front';
    _dynamic_form_preprocess_front_forms($variables);
  }
}

/**
 * Populates front_forms variables used by page.tpl.php on the front page.
 */
function _dynamic_form_preprocess_front_forms(&$variables) {
  global $user;

  module_load_include('inc', 'dynamic_form', 'includes/dynamic_form.front');

  if (!$user->uid) {
    // Anonymous: latest 3 currently active public forms.
    $q = db_select('dynamic_form_forms', 'f')
      ->fields('f', array('id', 'title', 'description', 'slug', 'created_at', 'closes_at'))
      ->condition('f.status', 1)
      ->condition('f.visibility', 'p')
      ->isNull('f.deleted_at')
      ->orderBy('f.created_at', 'DESC')
      ->range(0, 3);
    _dynamic_form_apply_active_conditions($q);
    $q->addExpression(
      '(SELECT COUNT(*) FROM {dynamic_form_responses} r WHERE r.form_id = f.id AND r.status = 1 AND r.deleted_at IS NULL)',
      'response_count'
    );
    $variables['front_forms']           = $q->execute()->fetchAllAssoc('id');
    $variables['front_forms_title']     = t('Public Forms');
    $variables['front_forms_subtitle']  = t('Browse publicly available forms and share your response.');
    $variables['front_forms_see_more']  = url('forms');
    $variables['front_forms_see_label'] = t('See All Public Forms');

    // Count currently active public forms (same conditions, no range).
    $cq = db_select('dynamic_form_forms', 'f')
      ->condition('f.status', 1)
      ->condition('f.visibility', 'p')
      ->isNull('f.deleted_at');
    _dynamic_form_apply_active_conditions($cq);
    $variables['front_forms_total'] = $cq->countQuery()->execute()->fetchField();
  }
  else {
    // Logged-in: latest 3 currently active forms this user has access to.
    $member_ids = db_select('dynamic_form_members', 'm')
      ->fields('m', array('form_id'))
      ->condition('m.user_id', $user->uid)
      ->isNull('m.deleted_at')
      ->execute()->fetchCol();

    $invite_ids = db_select('dynamic_form_invitations', 'inv')
      ->fields('inv', array('form_id'))
      ->condition('inv.user_id', $user->uid)
      ->condition('inv.status', 'accepted')
      ->execute()->fetchCol();

    $q = db_select('dynamic_form_forms', 'f')
      ->fields('f', array('id', 'title', 'description', 'slug', 'created_at', 'closes_at'))
      ->condition('f.status', 1)
      ->isNull('f.deleted_at')
      ->orderBy('f.created_at', 'DESC')
      ->range(0, 3);
    _dynamic_form_apply_active_conditions($q);
    $q->addExpression(
      '(SELECT COUNT(*) FROM {dynamic_form_responses} r WHERE r.form_id = f.id AND r.status = 1 AND r.deleted_at IS NULL)',
      'response_count'
    );

    $or = db_or()
      ->condition('f.visibility', 'p')
      ->condition('f.created_by', $user->uid);
    if (!empty($member_ids)) {
      $or->condition('f.id', $member_ids, 'IN');
    }
    if (!empty($invite_ids)) {
      $or->condition('f.id', $invite_ids, 'IN');
    }
    $q->condition($or);

    $variables['front_forms']           = $q->execute()->fetchAllAssoc('id');
    $variables['front_forms_title']     = t('Your Forms');
    $variables['front_forms_subtitle']  = t('The latest forms you have access to.');
    $variables['front_forms_see_more']  = url('dashboard/forms');
    $variables['front_forms_see_label'] = t('See All Forms');
    $variables['front_forms_total']     = 999; // always show for logged-in users
  }
}

/**
 * Implements hook_preprocess_html().
 *
 * Adds page-specific body classes to the <html> element.
 */
function dynamic_form_preprocess_html(&$variables) {
  $path = current_path();

  if ($path === 'login' || $path === 'user/login') {
    $variables['classes_array'][] = 'page-login';
  }
  elseif ($path === 'register' || $path === 'user/register') {
    $variables['classes_array'][] = 'page-register';
  }

  if (strpos($path, 'dashboard') === 0) {
    $variables['classes_array'][] = 'page-dashboard';
  }
}
