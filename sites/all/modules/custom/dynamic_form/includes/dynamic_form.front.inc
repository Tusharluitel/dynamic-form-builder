<?php

/**
 * @file
 * Public-facing pages: public form listing and form viewer.
 */

/**
 * A form is active when:
 *   - It has been published (published_at IS NOT NULL)
 *   - opens_at has passed or is unset
 *   - closes_at is in the future or is unset
 */
function _dynamic_form_apply_active_conditions(&$query) {
  $now = REQUEST_TIME;
  $query->condition('f.published_at', NULL, 'IS NOT NULL');
  $query->where('(f.opens_at  IS NULL OR f.opens_at  <= :opens_now)',  array(':opens_now'  => $now));
  $query->where('(f.closes_at IS NULL OR f.closes_at >= :closes_now)', array(':closes_now' => $now));
}

/**
 * Page callback: /forms — paginated list of public published forms.
 */
function dynamic_form_public_forms_page() {
  drupal_set_title(t('Public Forms'));

  $base = db_select('dynamic_form_forms', 'f')
    ->fields('f', array('id', 'title', 'description', 'slug', 'created_at','closes_at'))
    ->condition('f.status', 1)
    ->condition('f.visibility', 'p')
    ->isNull('f.deleted_at')
    ->orderBy('f.created_at', 'DESC');
  _dynamic_form_apply_active_conditions($base);
  $base->addExpression(
    '(SELECT COUNT(*) FROM {dynamic_form_responses} r WHERE r.form_id = f.id AND r.status = 1 AND r.deleted_at IS NULL)',
    'response_count'
  );

  $forms = $base->extend('PagerDefault')->limit(5)->execute()->fetchAllAssoc('id');

  $cards = array();
  foreach ($forms as $form) {
    $cards[] = _dynamic_form_render_public_form_card($form);
  }

  return theme('dynamic_form_public_listing', array(
    'cards' => $cards,
    'pager' => empty($cards) ? '' : theme('pager'),
  ));
}

/**
 * Page callback: /forms/% — public form viewer identified by slug.
 */
function dynamic_form_public_form_view_page($slug) {
  // Fetch the form regardless of time window so we can give a useful message.
  $record = db_select('dynamic_form_forms', 'f')
    ->fields('f')
    ->condition('f.slug', check_plain($slug))
    ->condition('f.status', 1)
    ->condition('f.published_at', NULL, 'IS NOT NULL')
    ->isNull('f.deleted_at')
    ->execute()
    ->fetchObject();

  if (!$record) {
    return drupal_not_found();
  }

  $now = REQUEST_TIME;
  if ($record->opens_at && $record->opens_at > $now) {
    drupal_set_title(check_plain($record->title));
    return theme('dynamic_form_public_form_status', array(
      'message'  => t('This form is not open yet.'),
      'back_url' => url('forms'),
    ));
  }
  if ($record->closes_at && $record->closes_at < $now) {
    drupal_set_title(check_plain($record->title));
    return theme('dynamic_form_public_form_status', array(
      'message'  => t('This form is closed and no longer accepting responses.'),
      'back_url' => url('forms'),
    ));
  }

  global $user;

  if ($record->visibility === 'r') {
    if (!$user->uid) {
      drupal_set_message(t('You must be logged in to access this form.'), 'warning');
      drupal_goto('login', array('query' => drupal_get_destination()));
    }
    // Allow if user is an active member (covers direct-add and accepted invites).
    $is_authorized = db_select('dynamic_form_members', 'm')
      ->condition('m.form_id', $record->id)
      ->condition('m.user_id', $user->uid)
      ->isNull('m.deleted_at')
      ->countQuery()->execute()->fetchField();
    // Allow if user has a pending or accepted invitation by user_id.
    if (!$is_authorized) {
      $is_authorized = db_select('dynamic_form_invitations', 'inv')
        ->condition('inv.form_id', $record->id)
        ->condition('inv.user_id', $user->uid)
        ->condition('inv.status', array('pending', 'accepted'), 'IN')
        ->countQuery()->execute()->fetchField();
    }
    // Allow if user's registered email matches a pending or accepted invitation.
    if (!$is_authorized) {
      $user_mail = db_select('users', 'u')
        ->fields('u', array('mail'))
        ->condition('u.uid', $user->uid)
        ->execute()
        ->fetchField();
      if ($user_mail) {
        $is_authorized = db_select('dynamic_form_invitations', 'inv')
          ->condition('inv.form_id', $record->id)
          ->condition('inv.status', array('pending', 'accepted'), 'IN')
          ->where('LOWER(inv.email) = LOWER(:mail)', array(':mail' => $user_mail))
          ->countQuery()->execute()->fetchField();
      }
    }
    if (!$is_authorized && (int) $record->created_by !== (int) $user->uid) {
      return drupal_access_denied();
    }
  }
  elseif ($record->visibility === 'm') {
    if (!$user->uid) {
      drupal_set_message(t('You must be logged in to access this form.'), 'warning');
      drupal_goto('login', array('query' => drupal_get_destination()));
    }
  }

  drupal_set_title(check_plain($record->title));

  $question_count = db_select('dynamic_form_questions', 'q')
    ->condition('q.form_id', $record->id)
    ->isNull('q.deleted_at')
    ->countQuery()->execute()->fetchField();

  $response_count = db_select('dynamic_form_responses', 'r')
    ->condition('r.form_id', $record->id)
    ->condition('r.status', 1)
    ->isNull('r.deleted_at')
    ->countQuery()->execute()->fetchField();

  if (module_exists('dynamic_form_response')) {
    module_load_include('inc', 'dynamic_form_response', 'includes/dynamic_form_response.submit');
    $form_body = dynamic_form_response_render_form($record);
  }
  else {
    $form_body = '<p class="dfb-coming-soon-msg">' . t('Form responses are coming soon.') . '</p>';
  }

  return theme('dynamic_form_public_form_view', array(
    'title'          => check_plain($record->title),
    'description'    => $record->description ? nl2br(check_plain($record->description)) : '',
    'question_label' => format_plural($question_count, '1 question', '@count questions'),
    'response_label' => format_plural($response_count, '1 response', '@count responses'),
    'form_body'      => $form_body,
  ));
}

/**
 * Renders a single public form card using the template.
 */
function _dynamic_form_render_public_form_card($form) {
  $count = isset($form->response_count) ? (int) $form->response_count : 0;

  return theme('dynamic_form_public_form_card', array(
    'title'          => check_plain($form->title),
    'url'            => check_url(url('forms/' . $form->slug)),
    'desc'           => !empty($form->description)
                          ? check_plain(truncate_utf8($form->description, 120, TRUE, TRUE))
                          : '',
    'response_label' => format_plural($count, '1 response', '@count responses'),
    'closes_at'      => !empty($form->closes_at)
                          ? format_date($form->closes_at, 'custom', 'M j, Y')
                          : NULL,
  ));
}
