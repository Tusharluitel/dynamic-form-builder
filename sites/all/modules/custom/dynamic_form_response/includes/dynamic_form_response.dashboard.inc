<?php


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
      $raw = isset($row['val']) ? $row['val'] : '';
      // Range fields (date/time) submit val_from and val_to instead of val.
      if (isset($row['val_from']) || isset($row['val_to'])) {
        $rf  = trim(isset($row['val_from']) ? $row['val_from'] : '');
        $rt  = trim(isset($row['val_to'])   ? $row['val_to']   : '');
        $val = ($rf !== '' || $rt !== '') ? ($rf . '|' . $rt) : '';
      }
      // Multi-select submits val as an array (f[i][val][]); join with | for storage.
      elseif (is_array($raw)) {
        $val = implode('|', array_filter(array_map('trim', $raw)));
      }
      else {
        $val = trim($raw);
      }
      $op  = (isset($row['op']) && strtoupper($row['op']) === 'OR') ? 'OR' : 'AND';
      if ($qid > 0 && $val !== '') {
        $raw_rows[] = array('qid' => $qid, 'val' => $val, 'op' => $op);
        $raw_qids[] = $qid;
      }
    }
    if (!empty($raw_rows)) {
      $raw_qid_set = array_flip($raw_qids);
      $q_meta = array();
      foreach (dynamic_form_get_questions($form_id) as $q) {
        if (isset($raw_qid_set[(int) $q->id])) {
          $q_meta[(int) $q->id] = $q;
        }
      }
      foreach ($raw_rows as $row) {
        if (isset($q_meta[$row['qid']])) {
          $type = $q_meta[$row['qid']]->type;
          $rval = $row['val'];
          // Tags Select2 hidden input stores multi-values comma-separated;
          // normalise to pipe so the rest of the pipeline treats it uniformly.
          if ($type === 'tags' && strpos($rval, ',') !== FALSE) {
            $rval = implode('|', array_filter(array_map('trim', explode(',', $rval))));
          }
          $filter_rows[] = array(
            'qid'   => $row['qid'],
            'val'   => $rval,
            'op'    => $row['op'],
            'type'  => $type,
            'label' => $q_meta[$row['qid']]->label,
          );
        }
      }
    }
  }

  $has_filters = $filter_search !== '' || !empty($filter_rows)
               || $filter_date_from || $filter_date_to || $filter_respondent;

  // Load only questions marked allow_filter=1 for the filter UI and JS settings.
  $filter_questions = array();
  foreach (dynamic_form_get_questions($form_id) as $q) {
    if ($q->allow_filter) {
      $filter_questions[] = $q;
    }
  }

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

  $solr_ids   = NULL;
  $solr_error = FALSE;

  if ($has_filters && module_exists('dynamic_form_solr')) {
    $solr_ids = dynamic_form_solr_filter_responses($form_id, $solr_criteria);

    if ($solr_ids === FALSE) {
      $solr_error = TRUE;
    }
  }

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
  $out = theme('dynamic_form_response_responses_header', array(
    'back_url'         => url('dashboard/forms'),
    'form_id'          => $form_id,
    'form_title'       => check_plain($form->title),
    'form_builder_url' => url('dashboard/forms/' . $form_id . '/builder'),
    'has_filters'      => $has_filters,
    'filtered_total'   => $filtered_total,
    'total_all'        => $total_all,
    'solr_error'       => $solr_error,
  ));

  // Filter bar.
  $out .= _dynamic_form_response_render_filter_bar(
    $form_id, $filter_rows, $filter_date_from, $filter_date_to,
    $filter_respondent, $filter_search, $sort, $filter_questions
  );

  // Active filter chips.
  if ($has_filters && !$solr_error) {
    $out .= _dynamic_form_response_render_active_filters(
      $form_id, $filter_rows, $filter_date_from, $filter_date_to,
      $filter_respondent, $filter_search
    );
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
    $header = array(
      array('data' => '#',              'class' => array('dfr-col-num')),
      array('data' => t('Respondent'),  'class' => array('dfr-col-respondent')),
      array('data' => t('Submitted'),   'class' => array('dfr-col-date')),
      array('data' => t('Actions'),     'class' => array('dfr-col-actions')),
    );
    $rows = array();
    $i = 1;
    foreach ($responses as $r) {
      $uid = (int) $r->user_id;
      if ($uid && isset($users[$uid])) {
        $name = check_plain($users[$uid]->name)
          . ' &lt;' . check_plain($users[$uid]->mail) . '&gt;';
      }
      elseif (!$uid && $r->guest_email) {
        $name = check_plain($r->guest_email);
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
      $rows[] = array(
        array('data' => $i,                                                        'class' => array('dfr-col-num')),
        array('data' => $name,                                                     'class' => array('dfr-col-respondent')),
        array('data' => $date,                                                     'class' => array('dfr-col-date')),
        array('data' => '<div class="dfr-resp-actions">' . $view_link . $delete_btn . '</div>', 'class' => array('dfr-col-actions')),
      );
      $i++;
    }
    $out .= '<div class="dfr-table-scroll">';
    $out .= theme('table', array(
      'header'     => $header,
      'rows'       => $rows,
      'attributes' => array('class' => array('dfr-response-table')),
    ));
    $out .= '</div>';
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
   RESPONSE FILTER HELPERS
   ===================================================================== */

/**
 * Form API builder for the response filter bar.
 *
 * This is a GET-method form; Drupal's token and build-id fields are removed
 * via #pre_render so they do not pollute the query string. The page callback
 * still reads filter values directly from $_GET.
 *
 * Extra args passed through drupal_get_form():
 *   $form_id_val, $filter_rows, $filter_date_from, $filter_date_to,
 *   $filter_respondent, $filter_search, $sort, $filter_questions
 */
function dynamic_form_response_filter_form($form, &$form_state, $form_id_val, array $filter_rows, $filter_date_from, $filter_date_to, $filter_respondent, $filter_search = '', $sort = 'date_desc', array $filter_questions = array()) {

  $form['#method']     = 'get';
  $form['#action']     = url('dashboard/forms/' . (int) $form_id_val . '/responses');
  $form['#id']         = 'dfr-filter-form';
  $form['#token']      = FALSE;
  $form['#pre_render'] = array('_dynamic_form_response_filter_pre_render');
  $form['#prefix']     = '<div class="dfr-filter-bar">';
  $form['#suffix']     = '</div>';

  // Build a qid → active val map for pre-filling question fields.
  $active_vals = array();
  foreach ($filter_rows as $row) {
    $active_vals[(int) $row['qid']] = $row['val'];
  }

  // Batch-load options for option-type questions (single query).
  // Tags questions use live Solr AJAX autocomplete instead of static options.
  $option_types = array('radio', 'checkbox', 'select');
  $option_qids  = array();
  foreach ($filter_questions as $fq) {
    if (in_array($fq->type, $option_types)) {
      $option_qids[] = (int) $fq->id;
    }
  }
  $options_by_qid = array();
  if (!empty($option_qids)) {
    $opt_rows = db_select('dynamic_form_question_options', 'o')
      ->fields('o', array('question_id', 'option_label'))
      ->condition('question_id', $option_qids, 'IN')
      ->isNull('deleted_at')
      ->orderBy('position', 'ASC')
      ->execute()
      ->fetchAll();
    foreach ($opt_rows as $or) {
      $options_by_qid[(int) $or->question_id][] = $or->option_label;
    }
  }

  $ac_url = url('dynamic-form/ajax/tags/autocomplete');

  // ── Full-text search ──────────────────────────────────────────────────────
  $form['search_row'] = array(
    '#type'       => 'container',
    '#attributes' => array('class' => array('dfr-filter-row', 'dfr-filter-row--search')),
  );
  $form['search_row']['s'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Search answers'),
    '#default_value' => $filter_search,
    '#attributes'    => array(
      'placeholder'  => t('Search across all text answers…'),
      'autocomplete' => 'off',
      'class'        => array('dfr-filter-search-input'),
    ),
    '#prefix'        => '<div class="dfr-filter-field dfr-filter-field--search">',
    '#suffix'        => '</div>',
  );

  // ── Question filters ──────────────────────────────────────────────────────
  if (!empty($filter_questions)) {
    // #tree on $form['f'] causes child elements to get names like f[0][val].
    $form['f'] = array(
      '#tree'       => TRUE,
      '#type'       => 'container',
      '#attributes' => array('class' => array('dfr-qfilter-grid')),
      '#prefix'     => '<div class="dfr-qfilter-section">'
                     . '<div class="dfr-qfilter-heading">' . t('Filter by question') . '</div>',
      '#suffix'     => '</div>',
    );

    foreach ($filter_questions as $idx => $fq) {
      $qid      = (int) $fq->id;
      $val      = isset($active_vals[$qid]) ? $active_vals[$qid] : '';
      $input_id = 'dfr-qf-' . $qid;
      $is_range = in_array($fq->type, array('date', 'time'));
      $cls      = $is_range
        ? array('dfr-qfilter-field', 'dfr-qfilter-field--range')
        : array('dfr-qfilter-field');
      $clear_style = $val !== '' ? '' : ' style="display:none"';
      $clear_btn   = '<button type="button" class="dfr-qfilter-clear"'
        . ' aria-label="' . t('Clear') . '"' . $clear_style
        . '><i class="fa-solid fa-xmark"></i></button>';

      // Each question filter is a container div with class + data-qid.
      $form['f'][$idx] = array(
        '#type'       => 'container',
        '#tree'       => TRUE,
        '#attributes' => array('class' => $cls, 'data-qid' => (string) $qid),
      );

      // Custom label markup appears first inside the container.
      $form['f'][$idx]['_label'] = array(
        '#markup' => '<label for="' . $input_id . '">' . check_plain($fq->label) . '</label>',
      );

      // Hidden fields carry qid and operator metadata.
      $form['f'][$idx]['qid'] = array(
        '#type'       => 'hidden',
        '#value'      => $qid,
        '#attributes' => array('class' => array('dfr-qfilter-qid')),
      );
      $form['f'][$idx]['op'] = array(
        '#type'       => 'hidden',
        '#value'      => 'AND',
        '#attributes' => array('class' => array('dfr-qfilter-op')),
      );

      if ($fq->type === 'tags') {
        $tags_val = str_replace('|', ',', $val);
        $form['f'][$idx]['val'] = array(
          '#type'       => 'hidden',
          '#value'      => $tags_val,
          '#attributes' => array(
            'id'                    => $input_id,
            'class'                 => array('dfr-qfilter-tags', 'dfr-qfilter-val'),
            'data-autocomplete-url' => check_url($ac_url),
            'data-placeholder'      => t('Type to search tags…'),
          ),
          '#suffix' => $clear_btn,
        );
      }
      elseif (in_array($fq->type, $option_types) && !empty($options_by_qid[$qid])) {
        $active_arr  = ($val !== '') ? explode('|', $val) : array();
        $all_count   = count($options_by_qid[$qid]);
        $all_sel_lbl = (count($active_arr) === $all_count && $all_count > 0)
          ? t('Deselect all') : t('Select all');
        $opts = array();
        foreach ($options_by_qid[$qid] as $opt) {
          $opts[check_plain($opt)] = check_plain($opt);
        }
        $form['f'][$idx]['val'] = array(
          '#type'          => 'select',
          '#title'         => '',
          '#title_display' => 'none',
          '#multiple'      => TRUE,
          '#options'       => $opts,
          '#default_value' => $active_arr,
          '#id'            => $input_id,
          '#attributes'    => array(
            'class'            => array('dfr-qfilter-select', 'dfr-qfilter-val'),
            'data-placeholder' => t('— Any —'),
          ),
          '#prefix' => '<button type="button" class="dfr-qfilter-select-all">' . $all_sel_lbl . '</button>',
          '#suffix' => $clear_btn,
        );
      }
      elseif ($fq->type === 'date') {
        $dp       = array_pad(explode('|', $val, 2), 2, '');
        $from_val = check_plain(trim($dp[0]));
        $to_val   = check_plain(trim($dp[1]));
        // theme_textfield() forces type="text" internally, so use #markup to
        // emit a proper <input type="date"> that the browser renders as a picker.
        $form['f'][$idx]['_date_range'] = array(
          '#markup' => '<div class="dfr-qfilter-range">'
            . '<input type="date" id="' . check_plain($input_id . '-from') . '"'
            . ' name="f[' . $idx . '][val_from]"'
            . ' value="' . $from_val . '"'
            . ' class="dfr-qfilter-input dfr-qfilter-range-from dfr-qfilter-val">'
            . '<span class="dfr-qfilter-range-sep">&rarr;</span>'
            . '<input type="date" id="' . check_plain($input_id . '-to') . '"'
            . ' name="f[' . $idx . '][val_to]"'
            . ' value="' . $to_val . '"'
            . ' class="dfr-qfilter-input dfr-qfilter-range-to dfr-qfilter-val">'
            . '</div>' . $clear_btn,
        );
      }
      elseif ($fq->type === 'time') {
        $tp       = array_pad(explode('|', $val, 2), 2, '');
        $from_val = check_plain(trim($tp[0]));
        $to_val   = check_plain(trim($tp[1]));
        $form['f'][$idx]['_time_range'] = array(
          '#markup' => '<div class="dfr-qfilter-range">'
            . '<input type="time" id="' . check_plain($input_id . '-from') . '"'
            . ' name="f[' . $idx . '][val_from]"'
            . ' value="' . $from_val . '"'
            . ' class="dfr-qfilter-input dfr-qfilter-range-from dfr-qfilter-val">'
            . '<span class="dfr-qfilter-range-sep">&rarr;</span>'
            . '<input type="time" id="' . check_plain($input_id . '-to') . '"'
            . ' name="f[' . $idx . '][val_to]"'
            . ' value="' . $to_val . '"'
            . ' class="dfr-qfilter-input dfr-qfilter-range-to dfr-qfilter-val">'
            . '</div>' . $clear_btn,
        );
      }
      elseif (in_array($fq->type, array('number', 'rating', 'linear_scale'))) {
        $form['f'][$idx]['val'] = array(
          '#type'          => 'textfield',
          '#title'         => '',
          '#title_display' => 'none',
          '#default_value' => $val,
          '#id'            => $input_id,
          '#attributes'    => array(
            'type'        => 'number',
            'placeholder' => t('Any'),
            'class'       => array('dfr-qfilter-input', 'dfr-qfilter-val'),
          ),
          '#suffix' => $clear_btn,
        );
      }
      else {
        $form['f'][$idx]['val'] = array(
          '#type'          => 'textfield',
          '#title'         => '',
          '#title_display' => 'none',
          '#default_value' => $val,
          '#id'            => $input_id,
          '#attributes'    => array(
            'placeholder'  => t('Any answer…'),
            'autocomplete' => 'off',
            'class'        => array('dfr-qfilter-input', 'dfr-qfilter-val'),
          ),
          '#suffix' => $clear_btn,
        );
      }
    }
  }

  $form['meta_row'] = array(
    '#type'       => 'container',
    '#attributes' => array('class' => array('dfr-filter-row', 'dfr-filter-row--meta')),
  );

  $form['meta_row']['date_from'] = array(
    '#markup' => '<div class="dfr-filter-field dfr-filter-field--date">'
      . '<label for="dfr-filter-date-from">' . t('From') . '</label>'
      . '<input type="date" id="dfr-filter-date-from" name="date_from"'
      . ' value="' . check_plain($filter_date_from) . '">'
      . '</div>',
  );

  $form['meta_row']['date_to'] = array(
    '#markup' => '<div class="dfr-filter-field dfr-filter-field--date">'
      . '<label for="dfr-filter-date-to">' . t('To') . '</label>'
      . '<input type="date" id="dfr-filter-date-to" name="date_to"'
      . ' value="' . check_plain($filter_date_to) . '">'
      . '</div>',
  );

  $form['meta_row']['respondent'] = array(
    '#type'          => 'select',
    '#title'         => t('Respondent'),
    '#id'            => 'dfr-filter-respondent',
    '#options'       => array(
      ''           => t('All'),
      'registered' => t('Registered'),
      'anonymous'  => t('Anonymous'),
    ),
    '#default_value' => $filter_respondent,
    '#prefix'        => '<div class="dfr-filter-field dfr-filter-field--respondent">',
    '#suffix'        => '</div>',
  );

  $form['meta_row']['sort'] = array(
    '#type'          => 'select',
    '#title'         => t('Sort'),
    '#id'            => 'dfr-filter-sort',
    '#options'       => array(
      'date_desc' => t('Newest first'),
      'date_asc'  => t('Oldest first'),
    ),
    '#default_value' => $sort,
    '#prefix'        => '<div class="dfr-filter-field dfr-filter-field--sort">',
    '#suffix'        => '</div>',
  );

  // Use #markup for the submit button so it renders as <button> (not <input>)
  // and carries no name= attribute, preventing it from appearing in the GET URL.
  $form['meta_row']['actions'] = array(
    '#markup' => '<div class="dfr-filter-actions">'
               . '<button type="submit" class="dfr-filter-btn-apply">' . t('Apply') . '</button>'
               . '</div>',
  );

  return $form;
}

/**
 * #pre_render callback: strip FAPI housekeeping fields from GET filter form.
 */
function _dynamic_form_response_filter_pre_render($form) {
  unset($form['form_build_id'], $form['form_id'], $form['form_token']);
  return $form;
}

/**
 * Renders the filter bar above the responses table.
 */
function _dynamic_form_response_render_filter_bar($form_id, array $filter_rows, $filter_date_from, $filter_date_to, $filter_respondent, $filter_search = '', $sort = 'date_desc', array $filter_questions = array()) {
  return drupal_render(drupal_get_form(
    'dynamic_form_response_filter_form',
    $form_id, $filter_rows, $filter_date_from, $filter_date_to,
    $filter_respondent, $filter_search, $sort, $filter_questions
  ));
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
    if (in_array($row['type'], array('date', 'time'))) {
      $rp   = array_pad(explode('|', $row['val'], 2), 2, '');
      $rfrom = trim($rp[0]);
      $rto   = trim($rp[1]);
      if ($rfrom !== '' && $rto !== '') {
        $display_val = check_plain($rfrom) . ' &rarr; ' . check_plain($rto);
      }
      elseif ($rfrom !== '') {
        $display_val = t('From') . ' ' . check_plain($rfrom);
      }
      else {
        $display_val = t('Until') . ' ' . check_plain($rto);
      }
    }
    else {
      $display_val = check_plain(truncate_utf8(str_replace('|', ', ', $row['val']), 40, TRUE, TRUE));
    }
    $chips[] = '<span class="dfr-filter-chip">'
      . $prefix
      . check_plain($row['label']) . ': <strong>' . $display_val . '</strong>'
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


/**
 * Page callback: dashboard/forms/%/responses/%
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

  $questions = dynamic_form_get_questions($form_id);

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

  $main .= theme('dynamic_form_response_detail_meta', array(
    'form_title'   => check_plain($form->title),
    'resp_name'    => $resp_name,
    'resp_mail'    => $resp_mail,
    'submitted_at' => $submitted_at,
  ));

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
  $base_url = 'dashboard/forms/' . $form_id . '/responses/';
  $panel_html = '';
  if (user_access('view response panel')) {
    $panel_html = theme('dynamic_form_response_detail_panel', array(
      'answered_q'        => $answered_q,
      'total_q'           => $total_q,
      'pct'               => $pct,
      'prev_url'          => $prev_id !== NULL ? url($base_url . $prev_id) : FALSE,
      'next_url'          => $next_id !== NULL ? url($base_url . $next_id) : FALSE,
      'current_pos'       => $current_pos !== FALSE ? ($current_pos + 1) : 0,
      'total_responses'   => count($all_ids),
      'all_responses_url' => url('dashboard/forms/' . $form_id . '/responses'),
      'form_builder_url'  => url('dashboard/forms/' . $form_id . '/builder'),
    ));
  }

  // ---- Compose two-column layout ------------------------------------
  $out = '<div class="dfr-detail-layout">'
    . '<div class="dfr-detail-main">' . $main . '</div>'
    . $panel_html
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

  $out = theme('dynamic_form_dashboard_page_header', array(
    'title'    => t('My Submissions'),
    'subtitle' => t('Forms you have filled out or started'),
  ));

  if (empty($rows)) {
    $out .= theme('dynamic_form_dashboard_empty_state', array(
      'icon'    => '<i class="fa-regular fa-pen-to-square"></i>',
      'message' => t("You haven't submitted any forms yet. <a href=\"!url\">Browse available forms</a> to get started.", array('!url' => url('dashboard/browse'))),
    ));
  }
  else {
    $status_labels = array(
      0 => '<span class="dfb-badge dfb-badge-draft">'     . t('Draft')     . '</span>',
      1 => '<span class="dfb-badge dfb-badge-published">' . t('Submitted') . '</span>',
    );

    $tbl_header = array(
      t('Form'), t('Status'), t('Submitted'), t('Actions'),
    );
    $tbl_rows = array();
    foreach ($rows as $r) {
      $status_badge = isset($status_labels[(int) $r->status])
        ? $status_labels[(int) $r->status]
        : check_plain($r->status);

      $date = ($r->submitted_at && (int) $r->status === 1)
        ? format_date($r->submitted_at, 'custom', 'M j, Y H:i')
        : '—';

      $action = ((int) $r->status === 1)
        ? l(t('View'),     'dashboard/forms/' . (int) $r->form_id . '/responses/' . (int) $r->id)
        : l(t('Continue'), 'forms/' . check_plain($r->form_slug));

      $tbl_rows[] = array(
        check_plain($r->form_title),
        $status_badge,
        $date,
        $action,
      );
    }
    $out .= theme('table', array(
      'header'     => $tbl_header,
      'rows'       => $tbl_rows,
      'attributes' => array('class' => array('dfr-response-table')),
    ));
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

  if ($val === NULL || $val === '') {
    $value_html = '<div class="dfr-answer-empty">' . t('(No answer provided)') . '</div>';
  }
  else {
    $decoded = @json_decode($val, TRUE);
    if (is_array($decoded) && isset($decoded['fid'], $decoded['filename'], $decoded['url'])) {
      $value_html = '<div class="dfr-answer-value">'
        . '<a href="' . check_url($decoded['url']) . '" target="_blank" class="dfr-file-link">'
        . '<i class="fa-solid fa-paperclip"></i> ' . check_plain($decoded['filename'])
        . '</a>'
        . '</div>';
    }
    elseif (is_array($decoded)) {
      $value_html = '<div class="dfr-answer-value">'
        . implode(', ', array_map('check_plain', $decoded)) . '</div>';
    }
    elseif ($q->type === 'text_editor') {
      $value_html = '<div class="dfr-answer-value dfr-answer-richtext">' . $val . '</div>';
    }
    else {
      $value_html = '<div class="dfr-answer-value">' . nl2br(check_plain($val)) . '</div>';
    }
  }

  return theme('dynamic_form_response_answer_card', array(
    'label'      => check_plain($q->label),
    'value_html' => $value_html,
  ));
}

