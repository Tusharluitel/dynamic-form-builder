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

  // Load Select2 for the filter question picker.
  $s2base = 'sites/all/libraries/select2';
  if (file_exists(DRUPAL_ROOT . '/' . $s2base . '/select2.min.js')) {
    drupal_add_css($s2base . '/select2.css');
    drupal_add_js($s2base . '/select2.min.js');
  }
  drupal_add_js($mp . '/js/response-filter.js');

  // ---- Collect and sanitize filter parameters from GET ----------------
  $filter_search     = isset($_GET['s'])          ? trim($_GET['s'])         : '';
  $filter_date_from  = isset($_GET['date_from'])  ? trim($_GET['date_from']) : '';
  $filter_date_to    = isset($_GET['date_to'])    ? trim($_GET['date_to'])   : '';
  $filter_respondent = isset($_GET['respondent']) ? trim($_GET['respondent']): '';
  $sort              = isset($_GET['sort'])        ? trim($_GET['sort'])      : 'date_desc';

  if (drupal_strlen($filter_search) > 200) {
    $filter_search = drupal_substr($filter_search, 0, 200);
  }
  if ($filter_date_from && !preg_match('/^\d{4}-\d{2}-\d{2}$/', $filter_date_from)) {
    $filter_date_from = '';
  }
  if ($filter_date_to && !preg_match('/^\d{4}-\d{2}-\d{2}$/', $filter_date_to)) {
    $filter_date_to = '';
  }
  if (!in_array($filter_respondent, array('', 'registered', 'anonymous'))) {
    $filter_respondent = '';
  }
  if (!in_array($sort, array('date_desc', 'date_asc'))) {
    $sort = 'date_desc';
  }

  // Parse advanced question filter rows from f[0][qid], f[0][val], f[0][op].
  $filter_rows = array();
  if (isset($_GET['f']) && is_array($_GET['f'])) {
    $raw_qids = array();
    $raw_rows = array();
    foreach ($_GET['f'] as $row) {
      $qid = isset($row['qid']) ? (int) $row['qid'] : 0;
      $val = isset($row['val']) ? trim($row['val']) : '';
      $op  = (isset($row['op']) && strtoupper($row['op']) === 'OR') ? 'OR' : 'AND';
      if ($qid > 0) {
        $raw_rows[] = array('qid' => $qid, 'val' => $val, 'op' => $op);
        $raw_qids[] = $qid;
      }
    }
    if (!empty($raw_rows)) {
      $q_meta = db_select('dynamic_form_questions', 'q')
        ->fields('q', array('id', 'type', 'label'))
        ->condition('id', $raw_qids, 'IN')
        ->condition('form_id', $form_id)
        ->isNull('deleted_at')
        ->execute()
        ->fetchAllAssoc('id');
      foreach ($raw_rows as $row) {
        if (isset($q_meta[$row['qid']])) {
          $filter_rows[] = array(
            'qid'   => $row['qid'],
            'val'   => $row['val'],
            'op'    => $row['op'],
            'type'  => $q_meta[$row['qid']]->type,
            'label' => $q_meta[$row['qid']]->label,
          );
        }
      }
    }
  }

  $has_filters = $filter_search !== '' || !empty($filter_rows)
               || $filter_date_from || $filter_date_to || $filter_respondent;

  // Load all questions for the filter UI and JS settings.
  $filter_questions = db_select('dynamic_form_questions', 'q')
    ->fields('q', array('id', 'label', 'type'))
    ->condition('form_id', $form_id)
    ->isNull('deleted_at')
    ->orderBy('position', 'ASC')
    ->execute()
    ->fetchAll();

  $questions_for_js = array();
  foreach ($filter_questions as $fq) {
    $questions_for_js[] = array('id' => (int) $fq->id, 'label' => $fq->label, 'type' => $fq->type);
  }

  $filter_rows_for_js = array();
  foreach ($filter_rows as $row) {
    $filter_rows_for_js[] = array('qid' => $row['qid'], 'val' => $row['val'], 'op' => $row['op']);
  }

  drupal_add_js(array(
    'dfrResponseFilter' => array(
      'filterOptionsUrl' => url('dynamic-form-response/ajax/filter-options'),
      'formId'           => $form_id,
      'filterSearch'     => $filter_search,
      'filterRows'       => $filter_rows_for_js,
      'filterDateFrom'   => $filter_date_from,
      'filterDateTo'     => $filter_date_to,
      'filterRespondent' => $filter_respondent,
      'filterSort'       => $sort,
      'solrEnabled'      => (int) module_exists('dynamic_form_solr'),
      'questions'        => $questions_for_js,
    ),
  ), 'setting');

  // ---- Build Solr filter rows (shared by ID filter + facets) ---------
  $solr_filters = array();
  foreach ($filter_rows as $row) {
    $solr_filters[] = array(
      'question_id'   => $row['qid'],
      'value'         => $row['val'],
      'question_type' => $row['type'],
      'op'            => $row['op'],
    );
  }
  $solr_criteria = array(
    'search'     => $filter_search,
    'filters'    => $solr_filters,
    'date_from'  => $filter_date_from,
    'date_to'    => $filter_date_to,
    'respondent' => $filter_respondent,
  );

  // ---- Resolve matching response IDs (Solr when available) ------------
  $solr_ids   = NULL;
  $solr_error = FALSE;

  if ($has_filters && module_exists('dynamic_form_solr')) {
    $solr_ids = dynamic_form_solr_filter_responses($form_id, $solr_criteria);

    if ($solr_ids === FALSE) {
      $solr_error = TRUE;
    }
  }

  // ---- Facets (option-type + tags questions, always when Solr is up) --
  $facet_data = array();
  if (module_exists('dynamic_form_solr') && !$solr_error) {
    $facetable_types = array('radio', 'checkbox', 'select', 'tags');
    $facet_qids      = array();
    $facet_q_map     = array();
    foreach ($filter_questions as $fq) {
      if (in_array($fq->type, $facetable_types)) {
        $facet_qids[] = (int) $fq->id;
        $facet_q_map[(int) $fq->id] = $fq;
      }
    }
    if (!empty($facet_qids)) {
      $raw_facets = dynamic_form_solr_get_response_facets($form_id, $solr_criteria, $facet_qids);
      if ($raw_facets !== FALSE) {
        foreach ($facet_q_map as $qid => $fq) {
          if (!empty($raw_facets[$qid])) {
            $facet_data[$qid] = array(
              'label' => $fq->label,
              'type'  => $fq->type,
              'items' => $raw_facets[$qid],
            );
          }
        }
      }
    }
  }

  $current_params = array(
    's'         => $filter_search,
    'date_from' => $filter_date_from,
    'date_to'   => $filter_date_to,
    'respondent' => $filter_respondent,
    'sort'      => $sort,
  );

  // ---- Build the base DB query ----------------------------------------
  $sort_dir = ($sort === 'date_asc') ? 'ASC' : 'DESC';

  $base = db_select('dynamic_form_responses', 'r')
    ->fields('r')
    ->condition('r.form_id', $form_id)
    ->condition('r.status', 1)
    ->isNull('r.deleted_at')
    ->orderBy('r.submitted_at', $sort_dir);

  if ($solr_error) {
    $base->condition('r.id', array(0), 'IN');
  }
  elseif ($solr_ids !== NULL) {
    $base->condition('r.id', empty($solr_ids) ? array(0) : $solr_ids, 'IN');
  }

  // Total unfiltered count for display context.
  $total_all = db_select('dynamic_form_responses', 'rt')
    ->condition('rt.form_id', $form_id)
    ->condition('rt.status', 1)
    ->isNull('rt.deleted_at')
    ->countQuery()->execute()->fetchField();

  $filtered_total = $base->countQuery()->execute()->fetchField();
  $responses      = $base->extend('PagerDefault')->limit(25)->execute()->fetchAll();

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

  // ---- Build HTML output ----------------------------------------------
  $out  = '<div class="dfr-breadcrumb">';
  $out .= l(t('&larr; Forms'), 'dashboard/forms',
    array('html' => TRUE, 'attributes' => array('class' => array('dfr-back-link'))));
  $out .= '<span class="dfr-breadcrumb-sep">/</span>';
  $out .= l(check_plain($form->title), 'dashboard/forms/' . $form_id . '/builder',
    array('attributes' => array('class' => array('dfr-breadcrumb-link'))));
  $out .= '</div>';

  $out .= '<div class="dfr-responses-header">';
  $out .= '<h2>' . t('Responses &mdash; @title', array('@title' => check_plain($form->title))) . '</h2>';
  if ($has_filters) {
    $out .= '<span class="dfr-response-count dfr-response-count--filtered">'
      . format_plural($filtered_total, '1 match', '@count matches')
      . ' <span class="dfr-response-count-of">' . t('of') . ' ' . (int) $total_all . '</span>'
      . '</span>';
  }
  else {
    $out .= '<span class="dfr-response-count">'
      . format_plural($total_all, '1 submitted response', '@count submitted responses')
      . '</span>';
  }
  $out .= '</div>';

  // Solr error banner (only shown when Solr criteria were requested and Solr failed).
  if ($solr_error) {
    $out .= '<div class="dfr-solr-error">'
      . t('Search service unavailable (503). Filters cannot be applied and results are hidden until the service is restored.')
      . '</div>';
  }

  // Filter bar.
  $out .= _dynamic_form_response_render_filter_bar(
    $form_id, $filter_rows, $filter_date_from, $filter_date_to,
    $filter_respondent, $filter_search, $sort
  );

  // Active filter chips.
  if ($has_filters && !$solr_error) {
    $out .= _dynamic_form_response_render_active_filters(
      $form_id, $filter_rows, $filter_date_from, $filter_date_to,
      $filter_respondent, $filter_search
    );
  }

  // Open two-column layout when facets are present.
  $has_facets = !empty($facet_data);
  if ($has_facets) {
    $out .= '<div class="dfr-facet-layout">';
    $out .= _dynamic_form_response_render_facet_panel($form_id, $facet_data, $filter_rows, $current_params);
    $out .= '<div class="dfr-facet-main">';
  }

  if ($solr_error && $has_filters) {
    // Zero results are intentional — Solr is down, do not mislead with "no matches".
  }
  elseif ((int) $filtered_total === 0 && $has_filters) {
    $out .= '<p class="dfr-empty-note dfr-empty-filtered">'
      . t('No responses match the current filters.')
      . ' ' . l(t('Clear filters'), 'dashboard/forms/' . $form_id . '/responses')
      . '</p>';
  }
  elseif (empty($responses)) {
    $out .= '<p class="dfr-empty-note">' . t('No responses have been submitted yet.') . '</p>';
  }
  else {
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
      }
      elseif (!$uid && $r->guest_email) {
        $name = $r->guest_email;
      }
      else {
        $name = t('Anonymous');
      }
      $date       = $r->submitted_at
        ? format_date($r->submitted_at, 'custom', 'M j, Y H:i')
        : '—';
      $modal_name = ($uid && isset($users[$uid]))
        ? '#' . (int) $r->id . ' – ' . check_plain($users[$uid]->name)
        : '#' . (int) $r->id . ' – ' . t('Anonymous');
      $view_link  = l(t('View'), 'dashboard/forms/' . $form_id . '/responses/' . (int) $r->id,
        array('attributes' => array('class' => array('dfr-resp-action-view'))));
      $delete_btn = '<button type="button"'
        . ' class="dfr-resp-action-delete dfb-delete-trigger"'
        . ' data-entity-type="response"'
        . ' data-entity-id="' . (int) $r->id . '"'
        . ' data-entity-name="' . check_plain($modal_name) . '">'
        . t('Delete') . '</button>';
      $actions    = '<div class="dfr-resp-actions">' . $view_link . $delete_btn . '</div>';

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

  if ($has_facets) {
    $out .= '</div>'; // .dfr-facet-main
    $out .= '</div>'; // .dfr-facet-layout
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
   FACET HELPERS
   ===================================================================== */

/**
 * Build the URL for a facet link.
 *
 * If the qid+val pair is already an active filter row it is removed;
 * otherwise it is appended as a new AND filter row.
 *
 * @param int    $form_id
 * @param int    $facet_qid
 * @param string $facet_val
 * @param array  $filter_rows   Current filter rows (each has qid, val, op).
 * @param array  $current_params  s, date_from, date_to, respondent, sort.
 * @return string  Drupal-generated URL.
 */
function _dynamic_form_response_facet_url($form_id, $facet_qid, $facet_val, array $filter_rows, array $current_params) {
  $is_active  = FALSE;
  $new_rows   = array();
  foreach ($filter_rows as $row) {
    if ((int) $row['qid'] === (int) $facet_qid && $row['val'] === $facet_val) {
      $is_active = TRUE;
    }
    else {
      $new_rows[] = $row;
    }
  }
  if (!$is_active) {
    $new_rows[] = array('qid' => $facet_qid, 'val' => $facet_val, 'op' => 'AND');
  }

  $query = array();
  if (!empty($current_params['s']))          { $query['s']          = $current_params['s']; }
  if (!empty($current_params['date_from']))  { $query['date_from']  = $current_params['date_from']; }
  if (!empty($current_params['date_to']))    { $query['date_to']    = $current_params['date_to']; }
  if (!empty($current_params['respondent'])) { $query['respondent'] = $current_params['respondent']; }
  if (!empty($current_params['sort']) && $current_params['sort'] !== 'date_desc') {
    $query['sort'] = $current_params['sort'];
  }
  foreach ($new_rows as $i => $row) {
    $query['f'][$i] = array('qid' => $row['qid'], 'val' => $row['val'], 'op' => $row['op']);
  }

  return url('dashboard/forms/' . $form_id . '/responses', array('query' => $query));
}

/**
 * Render the facet sidebar panel.
 *
 * @param int   $form_id
 * @param array $facet_data     Keyed by qid => ['label', 'type', 'items' => [['value','count']]].
 * @param array $filter_rows    Active filter rows.
 * @param array $current_params Current GET params (s, date_from, date_to, respondent, sort).
 * @return string HTML, or '' when facet_data is empty.
 */
function _dynamic_form_response_render_facet_panel($form_id, array $facet_data, array $filter_rows, array $current_params) {
  if (empty($facet_data)) {
    return '';
  }

  $out  = '<aside class="dfr-facet-panel">';
  $out .= '<div class="dfr-facet-panel-title">' . t('Filter by') . '</div>';

  foreach ($facet_data as $qid => $facet) {
    if (empty($facet['items'])) {
      continue;
    }

    $out .= '<div class="dfr-facet-section">';
    $out .= '<div class="dfr-facet-section-label" title="' . check_plain($facet['label']) . '">'
      . check_plain($facet['label']) . '</div>';
    $out .= '<ul class="dfr-facet-list">';

    foreach ($facet['items'] as $item) {
      $is_active = FALSE;
      foreach ($filter_rows as $row) {
        if ((int) $row['qid'] === (int) $qid && $row['val'] === $item['value']) {
          $is_active = TRUE;
          break;
        }
      }

      $href         = _dynamic_form_response_facet_url($form_id, $qid, $item['value'], $filter_rows, $current_params);
      $active_class = $is_active ? ' dfr-facet-item--active' : '';

      $out .= '<li class="dfr-facet-item' . $active_class . '">';
      $out .= '<a href="' . check_url($href) . '" class="dfr-facet-link"'
        . ' title="' . check_plain($item['value']) . '">';
      if ($is_active) {
        $out .= '<span class="dfr-facet-check">&#10003;</span>';
      }
      $out .= '<span class="dfr-facet-val">' . check_plain($item['value']) . '</span>';
      $out .= '<span class="dfr-facet-count">' . (int) $item['count'] . '</span>';
      $out .= '</a>';
      $out .= '</li>';
    }

    $out .= '</ul>';
    $out .= '</div>';
  }

  $out .= '</aside>';
  return $out;
}

/* =====================================================================
   RESPONSE FILTER HELPERS
   ===================================================================== */

/**
 * Renders the advanced filter bar above the responses table.
 */
function _dynamic_form_response_render_filter_bar($form_id, array $filter_rows, $filter_date_from, $filter_date_to, $filter_respondent, $filter_search = '', $sort = 'date_desc') {
  $action = check_url(url('dashboard/forms/' . $form_id . '/responses'));

  $out  = '<div class="dfr-filter-bar">';
  $out .= '<form method="get" action="' . $action . '" id="dfr-filter-form">';

  // Full-text search row.
  $out .= '<div class="dfr-filter-row dfr-filter-row--search">';
  $out .= '<div class="dfr-filter-field dfr-filter-field--search">';
  $out .= '<label for="dfr-filter-search">' . t('Search answers') . '</label>';
  $out .= '<input type="text" name="s" id="dfr-filter-search"'
    . ' placeholder="' . t('Search across all text answers…') . '"'
    . ' value="' . check_plain($filter_search) . '"'
    . ' class="dfr-filter-search-input" autocomplete="off">';
  $out .= '</div>';
  $out .= '</div>';

  // Advanced question filter rows (populated by JS from config.filterRows).
  $out .= '<div class="dfr-adv-section">';
  $out .= '<div id="dfr-filter-rows"></div>';
  $out .= '<button type="button" id="dfr-filter-add-row" class="dfr-adv-add-btn">'
    . t('+ Add filter')
    . '</button>';
  $out .= '</div>';

  // Meta row: date, respondent, sort, apply.
  $out .= '<div class="dfr-filter-row dfr-filter-row--meta">';

  $out .= '<div class="dfr-filter-field dfr-filter-field--date">';
  $out .= '<label for="dfr-filter-date-from">' . t('From') . '</label>';
  $out .= '<input type="date" name="date_from" id="dfr-filter-date-from" value="' . check_plain($filter_date_from) . '">';
  $out .= '</div>';

  $out .= '<div class="dfr-filter-field dfr-filter-field--date">';
  $out .= '<label for="dfr-filter-date-to">' . t('To') . '</label>';
  $out .= '<input type="date" name="date_to" id="dfr-filter-date-to" value="' . check_plain($filter_date_to) . '">';
  $out .= '</div>';

  $out .= '<div class="dfr-filter-field dfr-filter-field--respondent">';
  $out .= '<label for="dfr-filter-respondent">' . t('Respondent') . '</label>';
  $out .= '<select name="respondent" id="dfr-filter-respondent">';
  $out .= '<option value="">' . t('All') . '</option>';
  $out .= '<option value="registered"' . ($filter_respondent === 'registered' ? ' selected="selected"' : '') . '>' . t('Registered') . '</option>';
  $out .= '<option value="anonymous"'  . ($filter_respondent === 'anonymous'  ? ' selected="selected"' : '') . '>' . t('Anonymous')  . '</option>';
  $out .= '</select>';
  $out .= '</div>';

  $out .= '<div class="dfr-filter-field dfr-filter-field--sort">';
  $out .= '<label for="dfr-filter-sort">' . t('Sort') . '</label>';
  $out .= '<select name="sort" id="dfr-filter-sort">';
  $out .= '<option value="date_desc"' . ($sort === 'date_desc' ? ' selected="selected"' : '') . '>' . t('Newest first') . '</option>';
  $out .= '<option value="date_asc"'  . ($sort === 'date_asc'  ? ' selected="selected"' : '') . '>' . t('Oldest first') . '</option>';
  $out .= '</select>';
  $out .= '</div>';

  $out .= '<div class="dfr-filter-actions">';
  $out .= '<button type="submit" class="dfr-filter-btn-apply">' . t('Apply') . '</button>';
  $out .= '</div>';

  $out .= '</div>'; // .dfr-filter-row--meta
  $out .= '</form>';
  $out .= '</div>'; // .dfr-filter-bar

  return $out;
}

/**
 * Renders the active filter chips row.
 */
function _dynamic_form_response_render_active_filters($form_id, array $filter_rows, $filter_date_from, $filter_date_to, $filter_respondent, $filter_search = '') {
  $chips = array();

  if ($filter_search !== '') {
    $chips[] = '<span class="dfr-filter-chip">'
      . t('Search:') . ' <strong>' . check_plain(truncate_utf8($filter_search, 40, TRUE, TRUE)) . '</strong>'
      . '</span>';
  }

  foreach ($filter_rows as $i => $row) {
    if ($row['val'] === '') {
      continue;
    }
    $prefix = ($i > 0) ? '<em class="dfr-chip-op">' . check_plain($row['op']) . '</em> ' : '';
    $chips[] = '<span class="dfr-filter-chip">'
      . $prefix
      . check_plain($row['label']) . ': <strong>' . check_plain(truncate_utf8($row['val'], 40, TRUE, TRUE)) . '</strong>'
      . '</span>';
  }

  if ($filter_date_from || $filter_date_to) {
    if ($filter_date_from && $filter_date_to) {
      $date_text = check_plain($filter_date_from) . ' &rarr; ' . check_plain($filter_date_to);
    }
    elseif ($filter_date_from) {
      $date_text = t('From') . ' ' . check_plain($filter_date_from);
    }
    else {
      $date_text = t('Until') . ' ' . check_plain($filter_date_to);
    }
    $chips[] = '<span class="dfr-filter-chip">' . t('Date:') . ' <strong>' . $date_text . '</strong></span>';
  }

  if ($filter_respondent) {
    $labels = array('registered' => t('Registered'), 'anonymous' => t('Anonymous'));
    $chips[] = '<span class="dfr-filter-chip">'
      . t('Respondent:') . ' <strong>' . $labels[$filter_respondent] . '</strong>'
      . '</span>';
  }

  if (empty($chips)) {
    return '';
  }

  $out  = '<div class="dfr-filter-active">';
  $out .= '<span class="dfr-filter-active-label">' . t('Filters:') . '</span>';
  $out .= implode('', $chips);
  $out .= l(t('Clear all'), 'dashboard/forms/' . $form_id . '/responses',
    array('attributes' => array('class' => array('dfr-filter-clear-all'))));
  $out .= '</div>';

  return $out;
}

/**
 * AJAX callback: returns question type + option list for the filter bar.
 *
 * GET params:
 *   question_id  (int)  Question primary key.
 *   form_id      (int)  Form primary key — used to validate ownership.
 */
function dynamic_form_response_ajax_filter_options() {
  $question_id = isset($_GET['question_id']) ? (int) $_GET['question_id'] : 0;
  $form_id     = isset($_GET['form_id'])     ? (int) $_GET['form_id']     : 0;

  $empty = array('type' => '', 'is_option_type' => FALSE, 'options' => array());

  if (!$question_id || !$form_id) {
    drupal_json_output($empty);
    return;
  }

  $question = db_select('dynamic_form_questions', 'q')
    ->fields('q', array('id', 'type', 'label'))
    ->condition('id', $question_id)
    ->condition('form_id', $form_id)
    ->isNull('deleted_at')
    ->execute()
    ->fetchObject();

  if (!$question) {
    drupal_json_output($empty);
    return;
  }

  $option_types   = array('radio', 'checkbox', 'select', 'tags');
  $is_option_type = in_array($question->type, $option_types);
  $options        = array();

  if ($is_option_type) {
    $rows = db_select('dynamic_form_question_options', 'o')
      ->fields('o', array('option_label'))
      ->condition('question_id', $question_id)
      ->orderBy('position', 'ASC')
      ->execute()
      ->fetchAll();
    foreach ($rows as $row) {
      $options[] = array('id' => $row->option_label, 'text' => $row->option_label);
    }
  }

  drupal_json_output(array(
    'type'          => $question->type,
    'is_option_type' => $is_option_type,
    'options'       => $options,
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
  }else{
    $resp_mail = $response->guest_email ? check_plain($response->guest_email) : '—';
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
    } elseif ($q->type === 'text_editor') {
      $out .= '<div class="dfr-answer-value dfr-answer-richtext">' . $val . '</div>';
    } else {
      $out .= '<div class="dfr-answer-value">' . nl2br(check_plain($val)) . '</div>';
    }
  }

  $out .= '</div>';
  return $out;
}
