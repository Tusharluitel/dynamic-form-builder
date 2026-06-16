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
 * Builds the per-form analytics page HTML via the theme system.
 *
 * @param object $form  Form record from DB.
 * @param array  $data  Data from _dynamic_form_analytics_form_data().
 */
function _dynamic_form_analytics_form_html($form, $data) {
  $kpi = $data['kpi'];

  $kpi_cards = array(
    array('label' => t('Submitted'),       'value' => $kpi['total_submitted'],       'icon' => '<i class="fa-solid fa-circle-check"></i>', 'modifier' => 'success'),
    array('label' => t('Completion Rate'), 'value' => $kpi['completion_rate'] . '%', 'icon' => '<i class="fa-solid fa-chart-line"></i>',   'modifier' => 'primary'),
    array('label' => t('Abandoned'),       'value' => $kpi['abandoned'],             'icon' => '<i class="fa-solid fa-ban"></i>',          'modifier' => 'warning'),
    array('label' => t('Avg Time'),        'value' => $kpi['avg_time'],              'icon' => '<i class="fa-regular fa-clock"></i>',      'modifier' => 'accent'),
  );

  $questions_html = '';
  foreach ($data['questions'] as $q) {
    $questions_html .= _dynamic_form_analytics_render_question_card($q);
  }

  return theme('dynamic_form_analytics_form_page', array(
    'form_title'     => check_plain($form->title),
    'form_id'        => (int) $form->id,
    'responses_url'  => check_url(url('dashboard/forms/' . $form->id . '/responses')),
    'analytics_url'  => check_url(url('dashboard/analytics')),
    'kpi_cards'      => $kpi_cards,
    'has_trend'      => !empty($data['trend']),
    'questions_html' => $questions_html,
  ));
}

/**
 * Renders a single question analytics card.
 *
 * Determines the correct body HTML based on the question's chart type, then
 * delegates the card wrapper to the theme system.
 *
 * @param array $q  Question data array from _dynamic_form_analytics_form_data().
 * @return string   Rendered HTML.
 */
function _dynamic_form_analytics_render_question_card($q) {
  if ($q['chart'] === 'text') {
    $samples_html = '';
    if (!empty($q['samples'])) {
      $items = '';
      foreach ($q['samples'] as $sample) {
        $items .= '<li>' . truncate_utf8($sample, 120, TRUE, TRUE) . '</li>';
      }
      $samples_html = '<ul class="dfb-analytics-samples">' . $items . '</ul>';
    }
    $body_html = '<div class="dfb-analytics-text-responses">'
      . '<p class="dfb-analytics-text-count">'
      . format_plural($q['count'], '1 response', '@count responses')
      . '</p>'
      . $samples_html
      . '</div>';
  }
  elseif (!empty($q['data'])) {
    $chart_class = 'dfb-chart-container';
    if ($q['chart'] === 'hbar') {
      $chart_class .= ' dfb-chart-container--hbar';
    }
    elseif ($q['chart'] === 'donut') {
      $chart_class .= ' dfb-chart-container--donut';
    }
    $body_html = '<div id="dfa-q-' . $q['id'] . '" class="' . $chart_class . '"></div>';
  }
  else {
    $body_html = '<div class="dfb-analytics-empty">' . t('No answers recorded yet.') . '</div>';
  }

  return theme('dynamic_form_analytics_question_card', array(
    'qid'       => $q['id'],
    'label'     => $q['label'],
    'type'      => check_plain($q['type']),
    'body_html' => $body_html,
  ));
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
