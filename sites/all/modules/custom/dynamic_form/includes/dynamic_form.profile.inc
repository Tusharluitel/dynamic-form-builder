<?php

/**
 * @file
 * User profile page callback for the Dynamic Form Builder dashboard.
 */

/**
 * Page callback: dashboard/profile
 */
function dynamic_form_profile_page() {
  global $user;

  drupal_set_title(t('My Profile'));

  module_load_include('inc', 'dynamic_form', 'includes/dynamic_form.dashboard');

  $module_path = drupal_get_path('module', 'dynamic_form');
  drupal_add_css($module_path . '/css/profile.css');

  $account = user_load($user->uid);

  // Count forms created by this user.
  $forms_count = (int) db_select('dynamic_form_forms', 'f')
    ->condition('f.created_by', $user->uid)
    ->isNull('f.deleted_at')
    ->countQuery()
    ->execute()
    ->fetchField();

  // Count responses submitted by this user.
  $responses_count = (int) db_select('dynamic_form_responses', 'r')
    ->condition('r.user_id', $user->uid)
    ->isNull('r.deleted_at')
    ->countQuery()
    ->execute()
    ->fetchField();

  // Derive display role label (highest-priority custom role).
  $role_priority = array('administrator', 'editor', 'form_applicant', 'authenticated user');
  $display_role  = t('Member');
  foreach ($role_priority as $role_name) {
    foreach ($account->roles as $rid => $name) {
      if ($name === $role_name) {
        $display_role = check_plain(ucwords(str_replace('_', ' ', $name)));
        break 2;
      }
    }
  }

  $profile_content = theme('dynamic_form_profile_page', array(
    'initials'   => drupal_strtoupper(drupal_substr($account->name, 0, 1)),
    'name'       => check_plain($account->name),
    'mail'       => check_plain($account->mail),
    'role_label' => $display_role,
    'edit_url'   => url('user/' . $user->uid . '/edit'),
    'stats'      => array(
      array(
        'icon'  => '<i class="fa-regular fa-calendar"></i>',
        'value' => format_date($account->created, 'custom', 'M j, Y'),
        'label' => t('Member Since'),
      ),
      array(
        'icon'  => '<i class="fa-regular fa-file-lines"></i>',
        'value' => $forms_count,
        'label' => t('Forms Created'),
      ),
      array(
        'icon'  => '<i class="fa-regular fa-pen-to-square"></i>',
        'value' => $responses_count,
        'label' => t('Responses Submitted'),
      ),
    ),
  ));

  return theme('dynamic_form_dashboard', array(
    'sidebar' => _dynamic_form_dashboard_sidebar('profile'),
    'content' => $profile_content,
  ));
}
