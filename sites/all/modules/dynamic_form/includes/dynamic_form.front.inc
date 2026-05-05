<?php

/**
 * @file
 * Public-facing pages: public form listing and form viewer.
 */

/**
 * Applies the "currently active" conditions to a query against dynamic_form_forms f.
 *
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

  $output  = '<div class="dfb-public-listing">';
  $output .= '<div class="dfb-public-listing-header">';
  $output .= '<h1>' . t('Public Forms') . '</h1>';
  $output .= '<p>' . t('Browse and respond to publicly available forms.') . '</p>';
  $output .= '</div>';

  if (empty($forms)) {
    $output .= '<p class="dfb-empty-note">' . t('No public forms are available at this time.') . '</p>';
  }
  else {
    $output .= '<div class="dfb-public-forms-grid dfb-public-forms-grid--list">';
    foreach ($forms as $form) {
      $output .= _dynamic_form_render_public_form_card($form);
    }
    $output .= '</div>';
    $output .= theme('pager');
  }

  $output .= '</div>';
  return $output;
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
    return '<div class="dfb-form-view"><div class="dfb-form-view-body">'
      . '<p class="dfb-coming-soon-msg">' . t('This form is not open yet.') . '</p>'
      . '<a href="' . url('forms') . '" class="dfb-btn-back">&larr; ' . t('Back to public forms') . '</a>'
      . '</div></div>';
  }
  if ($record->closes_at && $record->closes_at < $now) {
    drupal_set_title(check_plain($record->title));
    return '<div class="dfb-form-view"><div class="dfb-form-view-body">'
      . '<p class="dfb-coming-soon-msg">' . t('This form is closed and no longer accepting responses.') . '</p>'
      . '<a href="' . url('forms') . '" class="dfb-btn-back">&larr; ' . t('Back to public forms') . '</a>'
      . '</div></div>';
  }

  global $user;

  if ($record->visibility === 'r') {
    if (!$user->uid) {
      drupal_set_message(t('You must be logged in to access this form.'), 'warning');
      drupal_goto('login', array('query' => drupal_get_destination()));
    }
    $ok = db_select('dynamic_form_invitations', 'inv')
      ->condition('inv.form_id', $record->id)
      ->condition('inv.user_id', $user->uid)
      ->condition('inv.status', 'accepted')
      ->countQuery()->execute()->fetchField();
    if (!$ok) {
      $ok = db_select('dynamic_form_members', 'm')
        ->condition('m.form_id', $record->id)
        ->condition('m.user_id', $user->uid)
        ->isNull('m.deleted_at')
        ->countQuery()->execute()->fetchField();
    }
    if (!$ok && (int) $record->created_by !== (int) $user->uid) {
      return drupal_access_denied();
    }
  }
  elseif ($record->visibility === 'm') {
    if (!$user->uid) {
      drupal_set_message(t('You must be logged in to access this form.'), 'warning');
      drupal_goto('login', array('query' => drupal_get_destination()));
    }
    $ok = db_select('dynamic_form_members', 'm')
      ->condition('m.form_id', $record->id)
      ->condition('m.user_id', $user->uid)
      ->isNull('m.deleted_at')
      ->countQuery()->execute()->fetchField();
    if (!$ok && (int) $record->created_by !== (int) $user->uid) {
      return drupal_access_denied();
    }
  }

  drupal_set_title(check_plain($record->title));

  $q_count = db_select('dynamic_form_questions', 'q')
    ->condition('q.form_id', $record->id)
    ->isNull('q.deleted_at')
    ->countQuery()->execute()->fetchField();

  $r_count = db_select('dynamic_form_responses', 'r')
    ->condition('r.form_id', $record->id)
    ->condition('r.status', 1)
    ->isNull('r.deleted_at')
    ->countQuery()->execute()->fetchField();

  $output  = '<div class="dfb-form-view">';
  $output .= '<div class="dfb-form-view-header">';
  $output .= '<h1>' . check_plain($record->title) . '</h1>';
  if ($record->description) {
    $output .= '<p class="dfb-form-view-desc">' . nl2br(check_plain($record->description)) . '</p>';
  }
  $output .= '<div class="dfb-form-view-meta">';
  $output .= '<span>' . format_plural($q_count, '1 question', '@count questions') . '</span>';
  $output .= '<span class="dfb-dot">&middot;</span>';
  $output .= '<span>' . format_plural($r_count, '1 response', '@count responses') . '</span>';
  $output .= '</div>';
  $output .= '</div>';
  $output .= '<div class="dfb-form-view-body">';
  $output .= '<p class="dfb-coming-soon-msg">' . t('Form responses are coming soon.') . '</p>';
  $output .= '<a href="' . url('forms') . '" class="dfb-btn-back">&larr; ' . t('Back to public forms') . '</a>';
  $output .= '</div>';
  $output .= '</div>';

  return $output;
}

/**
 * Renders a single public form card as an HTML string.
 */
function _dynamic_form_render_public_form_card($form) {
  $title = check_plain($form->title);
  $desc  = !empty($form->description)
    ? check_plain(truncate_utf8($form->description, 120, TRUE, TRUE))
    : '';
  $url   = check_url(url('forms/' . $form->slug));
  $date  = format_date($form->created_at, 'custom', 'M j, Y');
  $closes_at = !empty($form->closes_at) ? format_date($form->closes_at, 'custom', 'M j, Y') : NULL;
  $count = isset($form->response_count) ? (int) $form->response_count : 0;

  $html  = '<div class="dfb-public-form-card">';
  $html .= '<div class="dfb-public-form-card-body">';
  $html .= '<h3><a href="' . $url . '">' . $title . '</a></h3>';
  if ($desc) {
    $html .= '<p class="dfb-public-form-card-desc">' . $desc . '</p>';
  }
  $html .= '<div class="dfb-public-form-card-meta">';
  $html .= '<span>' . format_plural($count, '1 response', '@count responses') . '</span>';
  if ($closes_at) {
    $html .= '<span class="dfb-dot">&middot;</span>';
    $html .= '<span class="dfb-closes-at">Closes at: ' . $closes_at . '</span>';
  }
  $html .= '</div>';
  $html .= '</div>';
  $html .= '<div class="dfb-public-form-card-footer">';
  $html .= '<a href="' . $url . '" class="dfb-public-form-btn">' . t('View Form') . '</a>';
  $html .= '</div>';
  $html .= '</div>';

  return $html;
}
