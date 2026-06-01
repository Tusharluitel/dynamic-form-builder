<?php


/**
 * Page callback for dashboard/forms/%/analytics.
 *
 * @param int $form_id
 *   The form ID from the URL.
 */
function dynamic_form_analytics_form_page($form_id) {
  $form_id = (int) $form_id;

  module_load_include('inc', 'dynamic_form', 'includes/dynamic_form.dashboard');
  module_load_include('inc', 'dynamic_form_analytics', 'includes/dynamic_form_analytics.wordcloud');

  $form = db_select('dynamic_form_forms', 'f')
    ->fields('f')
    ->condition('f.id', $form_id)
    ->condition('f.deleted_at', NULL, 'IS NULL')
    ->execute()
    ->fetchObject();

  if (!$form) {
    return drupal_not_found();
  }

  drupal_set_title(t('Analytics: @title', array('@title' => $form->title)));

  $module_path = drupal_get_path('module', 'dynamic_form_analytics');
  $d3_path     = libraries_get_path('d3');
  if ($d3_path) {
    drupal_add_js($d3_path . '/d3.min.js', array('type' => 'file', 'weight' => -10, 'group' => JS_LIBRARY));
  }
  else {
    drupal_add_js('sites/all/libraries/d3/d3.min.js', array('type' => 'file', 'weight' => -10, 'group' => JS_LIBRARY));
  }
  drupal_add_js('sites/all/libraries/d3-cloud/build/d3.layout.cloud.js', array('type' => 'file', 'weight' => -9, 'group' => JS_LIBRARY));

  drupal_add_css($module_path . '/css/analytics.css');
  drupal_add_js($module_path . '/js/analytics.js');

  $data = _dynamic_form_analytics_form_data($form_id);

  $wordcloud_form = array(
    'words'         => _dynamic_form_analytics_wordcloud_query($form_id),
    'formId'        => $form_id,
    'questions'     => _dynamic_form_analytics_wordcloud_tag_questions($form_id),
    'ajaxData'      => url('dashboard/analytics/wordcloud-data'),
    'responsesBase' => url('dashboard/forms/' . $form_id . '/responses'),
  );

  drupal_add_js(array('dfAnalytics' => array('form' => $data, 'wordcloudForm' => $wordcloud_form)), 'setting');

  $sidebar_html = _dynamic_form_dashboard_sidebar('analytics');
  $content_html = _dynamic_form_analytics_form_html($form, $data);

  return theme('dynamic_form_dashboard', array(
    'sidebar' => $sidebar_html,
    'content' => $content_html,
  ));
}

/**
 * Fetches all data needed for a single form's analytics page.
 *
 * @param int $form_id
 */
function _dynamic_form_analytics_form_data($form_id) {
  // --- KPI counts ---
  $total_started = (int) db_query(
    'SELECT COUNT(*) FROM {dynamic_form_responses}
     WHERE form_id = :fid AND deleted_at IS NULL',
    array(':fid' => $form_id)
  )->fetchField();

  $total_submitted = (int) db_query(
    'SELECT COUNT(*) FROM {dynamic_form_responses}
     WHERE form_id = :fid AND status = 1 AND deleted_at IS NULL',
    array(':fid' => $form_id)
  )->fetchField();

  $completion_rate = $total_started > 0
    ? round($total_submitted / $total_started * 100, 1)
    : 0;

  // Average time to complete in seconds (submitted_at - created_at).
  $avg_secs = (float) db_query(
    'SELECT AVG(submitted_at - created_at) FROM {dynamic_form_responses}
     WHERE form_id = :fid AND status = 1 AND deleted_at IS NULL
       AND submitted_at IS NOT NULL',
    array(':fid' => $form_id)
  )->fetchField();

  $avg_time_label = _dynamic_form_analytics_format_duration((int) $avg_secs);

  // --- Daily submission trend (all time) ---
  $trend_rows = db_query(
    "SELECT DATE(FROM_UNIXTIME(submitted_at)) AS day, COUNT(*) AS cnt
     FROM {dynamic_form_responses}
     WHERE form_id = :fid AND status = 1 AND deleted_at IS NULL
     GROUP BY day ORDER BY day ASC",
    array(':fid' => $form_id)
  )->fetchAll();

  $trend = array();
  foreach ($trend_rows as $row) {
    $trend[] = array('day' => $row->day, 'cnt' => (int) $row->cnt);
  }

  // --- Respondent type: registered vs anonymous ---
  $resp_type = db_query(
    'SELECT
       SUM(user_id > 0) AS registered,
       SUM(user_id = 0) AS anonymous
     FROM {dynamic_form_responses}
     WHERE form_id = :fid AND status = 1 AND deleted_at IS NULL',
    array(':fid' => $form_id)
  )->fetchObject();

  $respondent_data = array(
    array('label' => t('Registered'), 'cnt' => (int) $resp_type->registered),
    array('label' => t('Anonymous'),  'cnt' => (int) $resp_type->anonymous),
  );

  // --- Per-question answer distributions ---
  $questions = db_select('dynamic_form_questions', 'q')
    ->fields('q', array('id', 'label', 'type', 'section_id', 'position'))
    ->condition('q.form_id', $form_id)
    ->condition('q.deleted_at', NULL, 'IS NULL')
    ->orderBy('q.section_id')
    ->orderBy('q.position')
    ->execute()
    ->fetchAll();

  $question_data = array();
  foreach ($questions as $q) {
    $question_data[] = _dynamic_form_analytics_question_distribution($q);
  }

  return array(
    'form_id' => $form_id,
    'kpi' => array(
      'total_started'   => $total_started,
      'total_submitted' => $total_submitted,
      'completion_rate' => $completion_rate,
      'avg_time'        => $avg_time_label,
      'abandoned'       => $total_started - $total_submitted,
    ),
    'trend'          => $trend,
    'respondent'     => $respondent_data,
    'questions'      => $question_data,
  );
}

/**
 * Fetches answer distribution data for a single question.
 *
 * @param object $q  Question record from DB.
 * @return array
 */
function _dynamic_form_analytics_question_distribution($q) {
  $base = array(
    'id'    => (int) $q->id,
    'label' => check_plain($q->label),
    'type'  => $q->type,
    'chart' => NULL,
    'data'  => array(),
  );

  switch ($q->type) {

    case 'radio':
    case 'select':
    case 'checkbox':
      // Fetch option labels for this question.
      $options = db_select('dynamic_form_question_options', 'o')
        ->fields('o', array('option_label'))
        ->condition('o.question_id', $q->id)
        ->condition('o.deleted_at', NULL, 'IS NULL')
        ->orderBy('o.position')
        ->execute()
        ->fetchCol();

      // Count how many times each stored value appears.
      $rows = db_query(
        'SELECT a.value, COUNT(*) AS cnt
         FROM {dynamic_form_answers} a
         JOIN {dynamic_form_responses} r ON r.id = a.response_id
         WHERE a.question_id = :qid AND r.status = 1
           AND r.deleted_at IS NULL AND a.deleted_at IS NULL
           AND a.value IS NOT NULL AND a.value != \'\'
         GROUP BY a.value ORDER BY cnt DESC',
        array(':qid' => $q->id)
      )->fetchAll();

      $dist = array();
      // Seed all known options with 0.
      foreach ($options as $opt) {
        $dist[check_plain($opt)] = 0;
      }
      foreach ($rows as $row) {
        // Checkbox answers may be comma-separated multi-values.
        $values = ($q->type === 'checkbox')
          ? array_map('trim', explode(',', $row->value))
          : array($row->value);
        foreach ($values as $v) {
          $key = check_plain($v);
          if (!isset($dist[$key])) {
            $dist[$key] = 0;
          }
          $dist[$key] += (int) $row->cnt;
        }
      }
      arsort($dist);

      $data = array();
      foreach ($dist as $label => $cnt) {
        $data[] = array('label' => $label, 'cnt' => $cnt);
      }

      $base['chart'] = 'hbar';
      $base['data']  = $data;
      break;

    case 'tags':
      $rows = db_query(
        'SELECT t.standardized_value AS tag, COUNT(*) AS cnt
         FROM {dynamic_form_tag_answers} ta
         JOIN {dynamic_form_tags} t ON t.id = ta.tag_id
         JOIN {dynamic_form_responses} r ON r.id = ta.response_id
         WHERE ta.question_id = :qid AND r.status = 1 AND r.deleted_at IS NULL
         GROUP BY ta.tag_id ORDER BY cnt DESC LIMIT 20',
        array(':qid' => $q->id)
      )->fetchAll();

      $data = array();
      foreach ($rows as $row) {
        $data[] = array('label' => check_plain($row->tag), 'cnt' => (int) $row->cnt);
      }

      $base['chart'] = 'hbar';
      $base['data']  = $data;
      break;

    case 'date':
      $rows = db_query(
        "SELECT DATE_FORMAT(a.value, '%Y-%m') AS month, COUNT(*) AS cnt
         FROM {dynamic_form_answers} a
         JOIN {dynamic_form_responses} r ON r.id = a.response_id
         WHERE a.question_id = :qid AND r.status = 1
           AND r.deleted_at IS NULL AND a.deleted_at IS NULL
           AND a.value IS NOT NULL AND a.value != ''
         GROUP BY month ORDER BY month ASC",
        array(':qid' => $q->id)
      )->fetchAll();

      $data = array();
      foreach ($rows as $row) {
        $data[] = array('label' => $row->month, 'cnt' => (int) $row->cnt);
      }

      $base['chart'] = 'vbar';
      $base['data']  = $data;
      break;

    case 'file':
      $total_files = (int) db_query(
        'SELECT COUNT(af.id)
         FROM {dynamic_form_answer_files} af
         JOIN {dynamic_form_answers} a ON a.id = af.answer_id
         JOIN {dynamic_form_responses} r ON r.id = a.response_id
         WHERE a.question_id = :qid AND r.status = 1
           AND r.deleted_at IS NULL AND a.deleted_at IS NULL AND af.deleted_at IS NULL',
        array(':qid' => $q->id)
      )->fetchField();

      $type_rows = db_query(
        'SELECT af.file_type, COUNT(*) AS cnt
         FROM {dynamic_form_answer_files} af
         JOIN {dynamic_form_answers} a ON a.id = af.answer_id
         JOIN {dynamic_form_responses} r ON r.id = a.response_id
         WHERE a.question_id = :qid AND r.status = 1
           AND r.deleted_at IS NULL AND a.deleted_at IS NULL AND af.deleted_at IS NULL
         GROUP BY af.file_type ORDER BY cnt DESC',
        array(':qid' => $q->id)
      )->fetchAll();

      $data = array();
      foreach ($type_rows as $row) {
        $data[] = array('label' => check_plain($row->file_type), 'cnt' => (int) $row->cnt);
      }

      $base['chart']       = 'donut';
      $base['data']        = $data;
      $base['total_files'] = $total_files;
      break;

    case 'text':
    case 'textarea':
    default:
      $count = (int) db_query(
        'SELECT COUNT(*)
         FROM {dynamic_form_answers} a
         JOIN {dynamic_form_responses} r ON r.id = a.response_id
         WHERE a.question_id = :qid AND r.status = 1
           AND r.deleted_at IS NULL AND a.deleted_at IS NULL
           AND a.value IS NOT NULL AND a.value != \'\'',
        array(':qid' => $q->id)
      )->fetchField();

      $samples_raw = db_query(
        'SELECT a.value
         FROM {dynamic_form_answers} a
         JOIN {dynamic_form_responses} r ON r.id = a.response_id
         WHERE a.question_id = :qid AND r.status = 1
           AND r.deleted_at IS NULL AND a.deleted_at IS NULL
           AND a.value IS NOT NULL AND a.value != \'\'
         LIMIT 5',
        array(':qid' => $q->id)
      )->fetchCol();

      $samples = array_map('check_plain', $samples_raw);

      $base['chart']   = 'text';
      $base['count']   = $count;
      $base['samples'] = $samples;
      break;
  }

  return $base;
}

/**
 * Builds the per-form analytics page HTML.
 *
 * @param object $form  Form record from DB.
 * @param array  $data  Data from _dynamic_form_analytics_form_data().
 */
function _dynamic_form_analytics_form_html($form, $data) {
  $kpi = $data['kpi'];

  $output  = '<div class="dfb-analytics-page">';

  // Header with back links.
  $output .= '<div class="dfb-analytics-header">';
  $output .= '<div class="dfb-analytics-header-top">';
  $output .= '<div>';
  $output .= '<h2>' . t('Analytics: @title', array('@title' => check_plain($form->title))) . '</h2>';
  $output .= '<p class="dfb-dashboard-subtitle">' . t('Submission data and question breakdowns') . '</p>';
  $output .= '</div>';
  $output .= '<div class="dfb-analytics-header-actions">';
  $output .= '<a href="' . url('dashboard/forms/' . $form->id . '/responses') . '" class="dfb-btn dfb-btn-secondary">' . t('View Responses') . '</a>';
  $output .= '<a href="' . url('dashboard/analytics') . '" class="dfb-btn dfb-btn-ghost"><i class="fa-solid fa-arrow-left"></i> ' . t('All Analytics') . '</a>';
  $output .= '</div>';
  $output .= '</div>';
  $output .= '</div>';

  // KPI cards.
  $output .= '<div class="dfb-stats-grid">';
  $output .= _dynamic_form_stat_card(t('Submitted'),        $kpi['total_submitted'],  '<i class="fa-solid fa-circle-check"></i>',  'success');
  $output .= _dynamic_form_stat_card(t('Completion Rate'),  $kpi['completion_rate'] . '%', '<i class="fa-solid fa-chart-line"></i>', 'primary');
  $output .= _dynamic_form_stat_card(t('Abandoned'),        $kpi['abandoned'],        '<i class="fa-solid fa-ban"></i>', 'warning');
  $output .= _dynamic_form_stat_card(t('Avg Time'),         $kpi['avg_time'],         '<i class="fa-regular fa-clock"></i>',  'accent');
  $output .= '</div>';

  // Charts grid.
  $output .= '<div class="dfb-analytics-grid">';

  // Submission trend (full width).
  $output .= '<div class="dfb-chart-card dfb-chart-card--wide">';
  $output .= '<h3 class="dfb-chart-title">' . t('Daily Submissions') . '</h3>';
  if (empty($data['trend'])) {
    $output .= '<div class="dfb-analytics-empty">' . t('No submissions recorded yet.') . '</div>';
  }
  else {
    $output .= '<div id="dfa-chart-form-trend" class="dfb-chart-container"></div>';
  }
  $output .= '</div>';

  // Completion funnel.
  $output .= '<div class="dfb-chart-card">';
  $output .= '<h3 class="dfb-chart-title">' . t('Completion Funnel') . '</h3>';
  $output .= '<div id="dfa-chart-funnel" class="dfb-chart-container dfb-chart-container--donut"></div>';
  $output .= '</div>';

  // Respondent type.
  $output .= '<div class="dfb-chart-card">';
  $output .= '<h3 class="dfb-chart-title">' . t('Respondent Type') . '</h3>';
  $output .= '<div id="dfa-chart-respondent" class="dfb-chart-container dfb-chart-container--donut"></div>';
  $output .= '</div>';

  $output .= '</div>'; // .dfb-analytics-grid

  // Tag word cloud for this form.
  $output .= '<div class="dfb-chart-card dfb-chart-card--wide dfb-wc-card">';
  $output .= '<div class="dfb-wc-header">';
  $output .= '<h3 class="dfb-chart-title">' . t('Tag Word Cloud') . '</h3>';
  $output .= '<div class="dfb-wc-filters">';
  $output .= '<select id="dfa-wc-question-form" class="dfb-wc-select" aria-label="' . t('Filter by question') . '">';
  $output .= '<option value="">' . t('All Tag Questions') . '</option>';
  $output .= '</select>';
  $output .= '</div>'; // .dfb-wc-filters
  $output .= '</div>'; // .dfb-wc-header
  $output .= '<div id="dfa-chart-wordcloud-form" class="dfb-chart-container dfb-chart-container--wordcloud"></div>';
  $output .= '</div>'; // .dfb-wc-card

  // Per-question sections.
  if (!empty($data['questions'])) {
    $output .= '<div class="dfb-analytics-questions">';
    $output .= '<h3 class="dfb-analytics-section-title">' . t('Question Breakdown') . '</h3>';

    foreach ($data['questions'] as $q) {
      $output .= '<div class="dfb-question-analytics-card" data-question-id="' . $q['id'] . '">';
      $output .= '<div class="dfb-question-analytics-header">';
      $output .= '<span class="dfb-question-analytics-label">' . $q['label'] . '</span>';
      $output .= '<span class="dfb-question-type-badge">' . check_plain($q['type']) . '</span>';
      $output .= '</div>';

      if ($q['chart'] === 'text') {
        $output .= '<div class="dfb-analytics-text-responses">';
        $output .= '<p class="dfb-analytics-text-count">' . format_plural($q['count'], '1 response', '@count responses') . '</p>';
        if (!empty($q['samples'])) {
          $output .= '<ul class="dfb-analytics-samples">';
          foreach ($q['samples'] as $sample) {
            $output .= '<li>' . truncate_utf8($sample, 120, TRUE, TRUE) . '</li>';
          }
          $output .= '</ul>';
        }
        $output .= '</div>';
      }
      elseif (!empty($q['data'])) {
        $chart_class = 'dfb-chart-container';
        if ($q['chart'] === 'hbar') {
          $chart_class .= ' dfb-chart-container--hbar';
        }
        elseif ($q['chart'] === 'donut') {
          $chart_class .= ' dfb-chart-container--donut';
        }
        $output .= '<div id="dfa-q-' . $q['id'] . '" class="' . $chart_class . '"></div>';
      }
      else {
        $output .= '<div class="dfb-analytics-empty">' . t('No answers recorded yet.') . '</div>';
      }

      $output .= '</div>'; // .dfb-question-analytics-card
    }

    $output .= '</div>'; // .dfb-analytics-questions
  }

  $output .= '</div>'; // .dfb-analytics-page

  return $output;
}

/**
 * Formats a duration in seconds to a human-readable string.
 *
 * @param int $seconds
 * @return string
 */
function _dynamic_form_analytics_format_duration($seconds) {
  if ($seconds <= 0) {
    return t('N/A');
  }
  $minutes = (int) floor($seconds / 60);
  $secs    = $seconds % 60;
  if ($minutes === 0) {
    return $secs . 's';
  }
  return $minutes . 'm ' . $secs . 's';
}
