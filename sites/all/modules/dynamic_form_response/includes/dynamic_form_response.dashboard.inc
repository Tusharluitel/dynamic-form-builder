<?php

/**
 * @file
 * Dashboard pages for viewing and managing form responses.
 */

/* =====================================================================
   RESPONSE LIST
   ===================================================================== */

/**
 * Page callback: dashboard/forms/%/responses
 *
 * Paginated table of submitted responses for one form.
 *
 * @param int $form_id  Form primary key from the URL.
 */
function dynamic_form_response_list_page($form_id) {
  $form_id = (int) $form_id;

  $form = db_select('dynamic_form_forms', 'f')
    ->fields('f')
    ->condition('id', $form_id)
    ->isNull('deleted_at')
    ->execute()
    ->fetchObject();

  if (!$form) {
    drupal_set_message(t('Form not found.'), 'error');
    drupal_goto('dashboard/forms');
  }

  drupal_set_title(t('Responses: @title', array('@title' => $form->title)));

  $mp     = drupal_get_path('module', 'dynamic_form_response');
  $mp_dfm = drupal_get_path('module', 'dynamic_form');
  drupal_add_css($mp . '/css/response-list.css');
  drupal_add_css($mp_dfm . '/css/dashboard-delete.css');
  drupal_add_js($mp_dfm . '/js/dashboard-delete.js');

  $base = db_select('dynamic_form_responses', 'r')
    ->fields('r')
    ->condition('form_id', $form_id)
    ->condition('status', 1)
    ->isNull('deleted_at')
    ->orderBy('submitted_at', 'DESC');

  $total     = $base->countQuery()->execute()->fetchField();
  $responses = $base->extend('PagerDefault')->limit(25)->execute()->fetchAll();

  // Batch-load respondent names.
  $uid_map = array();
  foreach ($responses as $r) {
    if ($r->user_id) {
      $uid_map[(int) $r->user_id] = TRUE;
    }
  }
  $users = array();
  if (!empty($uid_map)) {
    $rows = db_select('users', 'u')
      ->fields('u', array('uid', 'name', 'mail'))
      ->condition('uid', array_keys($uid_map), 'IN')
      ->execute()
      ->fetchAll();
    foreach ($rows as $u) {
      $users[(int) $u->uid] = $u;
    }
  }

  $out  = '<div class="dfr-breadcrumb">';
  $out .= l(t('&larr; Forms'), 'dashboard/forms',
    array('html' => TRUE, 'attributes' => array('class' => array('dfr-back-link'))));
  $out .= '<span class="dfr-breadcrumb-sep">/</span>';
  $out .= l(check_plain($form->title), 'dashboard/forms/' . $form_id . '/builder',
    array('attributes' => array('class' => array('dfr-breadcrumb-link'))));
  $out .= '</div>';

  $out .= '<div class="dfr-responses-header">';
  $out .= '<h2>' . t('Responses &mdash; @title', array('@title' => check_plain($form->title))) . '</h2>';
  $out .= '<span class="dfr-response-count">'
    . format_plural($total, '1 submitted response', '@count submitted responses')
    . '</span>';
  $out .= '</div>';

  if (empty($responses)) {
    $out .= '<p class="dfr-empty-note">' . t('No responses have been submitted yet.') . '</p>';
  } else {
    $out .= '<table class="dfr-response-table">';
    $out .= '<thead><tr>'
      . '<th>#</th>'
      . '<th>' . t('Respondent') . '</th>'
      . '<th>' . t('Submitted') . '</th>'
      . '<th>' . t('Actions') . '</th>'
      . '</tr></thead><tbody>';

    $i = 1;
    foreach ($responses as $r) {
      $uid = (int) $r->user_id;
      if ($uid && isset($users[$uid])) {
        $name = check_plain($users[$uid]->name)
          . ' &lt;' . check_plain($users[$uid]->mail) . '&gt;';
      } else {
        $name = t('Anonymous');
      }
      $date         = $r->submitted_at
        ? format_date($r->submitted_at, 'custom', 'M j, Y H:i')
        : '—';
      $modal_name   = ($uid && isset($users[$uid]))
        ? '#' . (int) $r->id . ' – ' . check_plain($users[$uid]->name)
        : '#' . (int) $r->id . ' – ' . t('Anonymous');
      $view_link    = l(t('View'), 'dashboard/forms/' . $form_id . '/responses/' . (int) $r->id,
        array('attributes' => array('class' => array('dfr-resp-action-view'))));
      $delete_btn   = '<button type="button"'
        . ' class="dfr-resp-action-delete dfb-delete-trigger"'
        . ' data-entity-type="response"'
        . ' data-entity-id="' . (int) $r->id . '"'
        . ' data-entity-name="' . check_plain($modal_name) . '">'
        . t('Delete') . '</button>';
      $actions      = '<div class="dfr-resp-actions">' . $view_link . $delete_btn . '</div>';

      $out .= '<tr>'
        . '<td>' . $i . '</td>'
        . '<td>' . $name . '</td>'
        . '<td>' . $date . '</td>'
        . '<td>' . $actions . '</td>'
        . '</tr>';
      $i++;
    }

    $out .= '</tbody></table>';
    $out .= theme('pager');
  }

  module_load_include('inc', 'dynamic_form', 'includes/dynamic_form.dashboard');
  $sidebar = _dynamic_form_dashboard_sidebar('forms');
  $out    .= _dynamic_form_delete_confirm_modal();

  return theme('dynamic_form_dashboard', array(
    'sidebar' => $sidebar,
    'content' => '<div class="dfr-response-detail">' . $out . '</div>',
  ));
}

/* =====================================================================
   SINGLE RESPONSE DETAIL
   ===================================================================== */

/**
 * Page callback: dashboard/forms/%/responses/%
 *
 * Displays every answer for one submitted response, organised by section.
 *
 * @param int $form_id      Form primary key.
 * @param int $response_id  Response primary key.
 */
function dynamic_form_response_view_page($form_id, $response_id) {
  $form_id     = (int) $form_id;
  $response_id = (int) $response_id;

  $form = db_select('dynamic_form_forms', 'f')
    ->fields('f')
    ->condition('id', $form_id)
    ->isNull('deleted_at')
    ->execute()
    ->fetchObject();

  if (!$form) {
    return drupal_not_found();
  }

  $response = db_select('dynamic_form_responses', 'r')
    ->fields('r')
    ->condition('id', $response_id)
    ->condition('form_id', $form_id)
    ->isNull('deleted_at')
    ->execute()
    ->fetchObject();

  if (!$response) {
    return drupal_not_found();
  }

  drupal_set_title(t('Response #@id', array('@id' => $response_id)));

  $mp = drupal_get_path('module', 'dynamic_form_response');
  drupal_add_css($mp . '/css/response-list.css');

  // Respondent info.
  $resp_name = t('Anonymous');
  $resp_mail = '—';
  if ($response->user_id) {
    $u = db_select('users', 'u')
      ->fields('u', array('name', 'mail'))
      ->condition('uid', $response->user_id)
      ->execute()
      ->fetchObject();
    if ($u) {
      $resp_name = check_plain($u->name);
      $resp_mail = check_plain($u->mail);
    }
  }

  $submitted_at = $response->submitted_at
    ? format_date($response->submitted_at, 'custom', 'M j, Y H:i')
    : t('Pending');

  // Sections and questions.
  $sections = db_select('dynamic_form_sections', 's')
    ->fields('s')
    ->condition('form_id', $form_id)
    ->isNull('deleted_at')
    ->orderBy('position', 'ASC')
    ->execute()
    ->fetchAll();

  $questions = db_select('dynamic_form_questions', 'q')
    ->fields('q')
    ->condition('form_id', $form_id)
    ->isNull('deleted_at')
    ->orderBy('position', 'ASC')
    ->execute()
    ->fetchAll();

  $q_by_section = array();
  foreach ($questions as $q) {
    $sid = $q->section_id ? (int) $q->section_id : 0;
    $q_by_section[$sid][] = $q;
  }

  // Stored answers keyed by question_id.
  $answers = db_select('dynamic_form_answers', 'a')
    ->fields('a', array('question_id', 'value'))
    ->condition('response_id', $response_id)
    ->execute()
    ->fetchAllKeyed(0, 1);

  // Completion stats.
  $total_q    = count($questions);
  $answered_q = 0;
  foreach ($questions as $q) {
    $val = isset($answers[(int) $q->id]) ? $answers[(int) $q->id] : NULL;
    if ($val !== NULL && $val !== '') {
      $answered_q++;
    }
  }
  $pct = $total_q > 0 ? round($answered_q / $total_q * 100) : 0;

  // Prev / next submitted responses for this form (ordered by submitted_at ASC).
  $all_ids = db_select('dynamic_form_responses', 'r')
    ->fields('r', array('id'))
    ->condition('form_id', $form_id)
    ->condition('status', 1)
    ->isNull('deleted_at')
    ->orderBy('submitted_at', 'ASC')
    ->execute()
    ->fetchCol();

  $current_pos = array_search($response_id, $all_ids);
  $prev_id = ($current_pos !== FALSE && $current_pos > 0)
    ? $all_ids[$current_pos - 1] : NULL;
  $next_id = ($current_pos !== FALSE && $current_pos < count($all_ids) - 1)
    ? $all_ids[$current_pos + 1] : NULL;

  // ---- LEFT COLUMN: back link + meta + answer cards ------------------
  $main  = l(t('&larr; Back to Responses'), 'dashboard/forms/' . $form_id . '/responses',
    array('html' => TRUE, 'attributes' => array('class' => array('dfr-back-link'))));

  $main .= '<div class="dfr-detail-meta"><dl>'
    . '<dt>' . t('Form')       . '</dt><dd>' . check_plain($form->title) . '</dd>'
    . '<dt>' . t('Respondent') . '</dt><dd>' . $resp_name . '</dd>'
    . '<dt>' . t('Email')      . '</dt><dd>' . $resp_mail . '</dd>'
    . '<dt>' . t('Submitted')  . '</dt><dd>' . $submitted_at . '</dd>'
    . '</dl></div>';

  foreach ($sections as $sec) {
    $sid = (int) $sec->id;
    $qs  = isset($q_by_section[$sid]) ? $q_by_section[$sid] : array();
    if (empty($qs)) {
      continue;
    }
    $main .= '<h3 class="dfr-section-heading">' . check_plain($sec->name) . '</h3>';
    foreach ($qs as $q) {
      $main .= _dynamic_form_response_render_answer_card($q, $answers);
    }
  }
  if (!empty($q_by_section[0])) {
    foreach ($q_by_section[0] as $q) {
      $main .= _dynamic_form_response_render_answer_card($q, $answers);
    }
  }

  // ---- RIGHT COLUMN: info panel --------------------------------------

  if (user_access('view response panel')) {

  $panel  = '<aside class="dfr-detail-panel">';

  // Completion card.
  $panel .= '<div class="dfr-panel-card">';
  $panel .= '<h4 class="dfr-panel-card-title">' . t('Completion') . '</h4>';
  $panel .= '<div class="dfr-panel-stat">';
  $panel .= '<span class="dfr-panel-stat-num">' . $answered_q . ' / ' . $total_q . '</span>';
  $panel .= '<span class="dfr-panel-stat-label">' . t('questions answered') . '</span>';
  $panel .= '</div>';
  $panel .= '<div class="dfr-completion-bar-wrap">';
  $panel .= '<div class="dfr-completion-bar" style="width:' . $pct . '%"></div>';
  $panel .= '</div>';
  $panel .= '<div class="dfr-completion-pct">' . $pct . '%</div>';
  $panel .= '</div>';

  // Navigation card.
  if ($prev_id !== NULL || $next_id !== NULL) {
    $base = 'dashboard/forms/' . $form_id . '/responses/';
    $panel .= '<div class="dfr-panel-card dfr-panel-nav">';
    $panel .= '<h4 class="dfr-panel-card-title">' . t('Navigate') . '</h4>';
    $panel .= '<div class="dfr-panel-nav-btns">';
    if ($prev_id !== NULL) {
      $panel .= l(t('&larr; Previous'), $base . $prev_id,
        array('html' => TRUE, 'attributes' => array('class' => array('dfr-nav-btn'))));
    }
    if ($next_id !== NULL) {
      $panel .= l(t('Next &rarr;'), $base . $next_id,
        array('html' => TRUE, 'attributes' => array('class' => array('dfr-nav-btn'))));
    }
    $panel .= '</div>';
    if ($current_pos !== FALSE) {
      $panel .= '<p class="dfr-nav-position">'
        . t('@num of @total', array('@num' => $current_pos + 1, '@total' => count($all_ids)))
        . '</p>';
    }
    $panel .= '</div>';
  }

  // Actions card.
  $panel .= '<div class="dfr-panel-card dfr-panel-actions">';
  $panel .= '<h4 class="dfr-panel-card-title">' . t('Actions') . '</h4>';
  $panel .= l(t('All Responses'), 'dashboard/forms/' . $form_id . '/responses',
    array('attributes' => array('class' => array('dfr-action-btn dfr-action-btn--secondary'))));
  $panel .= l(t('Form Builder'), 'dashboard/forms/' . $form_id . '/builder',
    array('attributes' => array('class' => array('dfr-action-btn dfr-action-btn--secondary'))));
  $panel .= '</div>';

  $panel .= '</aside>';
}

  // ---- Compose two-column layout ------------------------------------
  $out = '<div class="dfr-detail-layout">'
    . '<div class="dfr-detail-main">' . $main . '</div>'
    . $panel
    . '</div>';

  module_load_include('inc', 'dynamic_form', 'includes/dynamic_form.dashboard');
  $sidebar = _dynamic_form_dashboard_sidebar('forms');

  return theme('dynamic_form_dashboard', array(
    'sidebar' => $sidebar,
    'content' => '<div class="dfr-response-detail">' . $out . '</div>',
  ));
}

/* =====================================================================
   MY SUBMISSIONS
   ===================================================================== */

/**
 * Page callback: dashboard/submissions
 *
 * Lists all responses (drafts and submitted) that the current user has
 * created, across any form.
 */
function dynamic_form_response_submissions_page() {
  global $user;

  drupal_set_title(t('My Submissions'));

  $mp = drupal_get_path('module', 'dynamic_form_response');
  drupal_add_css($mp . '/css/response-list.css');

  $query = db_select('dynamic_form_responses', 'r');
  $query->join('dynamic_form_forms', 'f', 'r.form_id = f.id');
  $query->fields('r', array('id', 'form_id', 'status', 'submitted_at', 'updated_at'));
  $query->addField('f', 'title', 'form_title');
  $query->addField('f', 'slug',  'form_slug');
  $query->condition('r.user_id', $user->uid);
  $query->isNull('r.deleted_at');
  $query->isNull('f.deleted_at');
  $query->orderBy('r.updated_at', 'DESC');

  $rows = $query->execute()->fetchAll();

  $out  = '<div class="dfb-dashboard-header">';
  $out .= '<h2>' . t('My Submissions') . '</h2>';
  $out .= '<p class="dfb-dashboard-subtitle">' . t('Forms you have filled out or started') . '</p>';
  $out .= '</div>';

  if (empty($rows)) {
    $out .= '<div class="dfb-empty-state">';
    $out .= '<div class="dfb-empty-icon">&#128221;</div>';
    $out .= '<p>' . t("You haven't submitted any forms yet. <a href=\"!url\">Browse available forms</a> to get started.", array('!url' => url('dashboard/browse'))) . '</p>';
    $out .= '</div>';
  }
  else {
    $status_labels = array(
      0 => '<span class="dfb-badge dfb-badge-draft">'     . t('Draft')     . '</span>',
      1 => '<span class="dfb-badge dfb-badge-published">' . t('Submitted') . '</span>',
    );

    $out .= '<table class="dfr-response-table">';
    $out .= '<thead><tr>'
      . '<th>' . t('Form')      . '</th>'
      . '<th>' . t('Status')    . '</th>'
      . '<th>' . t('Submitted') . '</th>'
      . '<th>' . t('Actions')   . '</th>'
      . '</tr></thead><tbody>';

    foreach ($rows as $r) {
      $status_badge = isset($status_labels[(int) $r->status])
        ? $status_labels[(int) $r->status]
        : check_plain($r->status);

      $date = ($r->submitted_at && (int) $r->status === 1)
        ? format_date($r->submitted_at, 'custom', 'M j, Y H:i')
        : '—';

      if ((int) $r->status === 1) {
        $action = l(t('View'), 'dashboard/forms/' . (int) $r->form_id . '/responses/' . (int) $r->id);
      }
      else {
        $action = l(t('Continue'), 'forms/' . check_plain($r->form_slug));
      }

      $out .= '<tr>'
        . '<td>' . check_plain($r->form_title) . '</td>'
        . '<td>' . $status_badge . '</td>'
        . '<td>' . $date . '</td>'
        . '<td>' . $action . '</td>'
        . '</tr>';
    }

    $out .= '</tbody></table>';
  }

  module_load_include('inc', 'dynamic_form', 'includes/dynamic_form.dashboard');
  $sidebar = _dynamic_form_dashboard_sidebar('submissions');

  return theme('dynamic_form_dashboard', array(
    'sidebar' => $sidebar,
    'content' => $out,
  ));
}

/* =====================================================================
   HELPERS
   ===================================================================== */

/**
 * Renders one answer card for the detail view.
 *
 * @param object $q        Question DB row.
 * @param array  $answers  All answers for this response, keyed by question_id.
 * @return string HTML.
 */
function _dynamic_form_response_render_answer_card($q, array $answers) {
  $qid = (int) $q->id;
  $val = isset($answers[$qid]) ? $answers[$qid] : NULL;

  $out  = '<div class="dfr-answer-card">';
  $out .= '<div class="dfr-answer-question">' . check_plain($q->label) . '</div>';

  if ($val === NULL || $val === '') {
    $out .= '<div class="dfr-answer-empty">' . t('(No answer provided)') . '</div>';
  } else {
    $decoded = @json_decode($val, TRUE);
    if (is_array($decoded) && isset($decoded['fid'], $decoded['filename'], $decoded['url'])) {
      // File upload answer — render as a download link.
      $out .= '<div class="dfr-answer-value">'
        . '<a href="' . check_url($decoded['url']) . '" target="_blank" class="dfr-file-link">'
        . '&#128206; ' . check_plain($decoded['filename'])
        . '</a>'
        . '</div>';
    } elseif (is_array($decoded)) {
      $out .= '<div class="dfr-answer-value">'
        . implode(', ', array_map('check_plain', $decoded)) . '</div>';
    } else {
      $out .= '<div class="dfr-answer-value">' . nl2br(check_plain($val)) . '</div>';
    }
  }

  $out .= '</div>';
  return $out;
}
