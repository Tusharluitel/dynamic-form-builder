<?php

/**
 * @file
 * Global analytics page: cross-form overview charts and KPI cards.
 */

/**
 * Page callback for dashboard/analytics.
 */
function dynamic_form_analytics_global_page() {
  drupal_set_title(t('Analytics'));

  module_load_include('inc', 'dynamic_form', 'includes/dynamic_form.dashboard');
  module_load_include('inc', 'dynamic_form_analytics', 'includes/dynamic_form_analytics.wordcloud');

  $module_path   = drupal_get_path('module', 'dynamic_form_analytics');
  $d3_path       = libraries_get_path('d3');
  if ($d3_path) {
    drupal_add_js($d3_path . '/d3.min.js', array('type' => 'file', 'weight' => -10, 'group' => JS_LIBRARY));
  }
  else {
    drupal_add_js('sites/all/libraries/d3/d3.min.js', array('type' => 'file', 'weight' => -10, 'group' => JS_LIBRARY));
  }
  drupal_add_js('sites/all/libraries/d3-cloud/build/d3.layout.cloud.js', array('type' => 'file', 'weight' => -9, 'group' => JS_LIBRARY));

  drupal_add_css($module_path . '/css/analytics.css');
  drupal_add_js($module_path . '/js/analytics.js');

  $data = _dynamic_form_analytics_global_data();

  $wordcloud = array(
    'words'          => _dynamic_form_analytics_wordcloud_query(),
    'forms'          => _dynamic_form_analytics_wordcloud_forms(),
    'ajaxData'       => url('dashboard/analytics/wordcloud-data'),
    'ajaxQuestions'  => url('dashboard/analytics/wordcloud-questions'),
  );

  drupal_add_js(array('dfAnalytics' => array('global' => $data, 'wordcloud' => $wordcloud)), 'setting');

  $sidebar_html = _dynamic_form_dashboard_sidebar('analytics');
  $content_html = _dynamic_form_analytics_global_html($data);

  return theme('dynamic_form_dashboard', array(
    'sidebar' => $sidebar_html,
    'content' => $content_html,
  ));
}

/**
 * Fetches all data needed for the global analytics page.
 */
function _dynamic_form_analytics_global_data() {
  // --- KPI counts ---
  $total_started = (int) db_query(
    'SELECT COUNT(*) FROM {dynamic_form_responses} WHERE deleted_at IS NULL'
  )->fetchField();

  $total_submitted = (int) db_query(
    'SELECT COUNT(*) FROM {dynamic_form_responses} WHERE status = 1 AND deleted_at IS NULL'
  )->fetchField();

  $active_forms = (int) db_query(
    'SELECT COUNT(*) FROM {dynamic_form_forms} WHERE status = 1 AND deleted_at IS NULL'
  )->fetchField();

  $completion_rate = $total_started > 0
    ? round($total_submitted / $total_started * 100, 1)
    : 0;

  $avg_per_form = $active_forms > 0
    ? round($total_submitted / $active_forms, 1)
    : 0;

  // --- Submissions over last 30 days ---
  $trend_rows = db_query(
    "SELECT DATE(FROM_UNIXTIME(submitted_at)) AS day, COUNT(*) AS cnt
     FROM {dynamic_form_responses}
     WHERE status = 1 AND deleted_at IS NULL
       AND submitted_at >= UNIX_TIMESTAMP(DATE_SUB(NOW(), INTERVAL 30 DAY))
     GROUP BY day ORDER BY day ASC"
  )->fetchAll();

  $trend = array();
  foreach ($trend_rows as $row) {
    $trend[] = array('day' => $row->day, 'cnt' => (int) $row->cnt);
  }

  // --- Status breakdown (submitted vs draft) ---
  $status_rows = db_query(
    'SELECT status, COUNT(*) AS cnt FROM {dynamic_form_responses}
     WHERE deleted_at IS NULL GROUP BY status'
  )->fetchAll();

  $status_data = array(
    array('label' => 'Submitted', 'cnt' => 0),
    array('label' => 'Draft',     'cnt' => 0),
  );
  foreach ($status_rows as $row) {
    if ((int) $row->status === 1) {
      $status_data[0]['cnt'] = (int) $row->cnt;
    }
    else {
      $status_data[1]['cnt'] += (int) $row->cnt;
    }
  }

  // --- Top 10 forms by submitted response count ---
  $top_rows = db_query(
    "SELECT f.title, COUNT(r.id) AS cnt
     FROM {dynamic_form_responses} r
     JOIN {dynamic_form_forms} f ON f.id = r.form_id
     WHERE r.status = 1 AND r.deleted_at IS NULL AND f.deleted_at IS NULL
     GROUP BY r.form_id ORDER BY cnt DESC LIMIT 10"
  )->fetchAll();

  $top_forms = array();
  foreach ($top_rows as $row) {
    $top_forms[] = array(
      'label' => check_plain($row->title),
      'cnt'   => (int) $row->cnt,
    );
  }

  return array(
    'kpi' => array(
      'total_submitted'  => $total_submitted,
      'total_started'    => $total_started,
      'completion_rate'  => $completion_rate,
      'active_forms'     => $active_forms,
      'avg_per_form'     => $avg_per_form,
    ),
    'trend'       => $trend,
    'status_data' => $status_data,
    'top_forms'   => $top_forms,
  );
}

/**
 * Builds the global analytics page HTML.
 */
function _dynamic_form_analytics_global_html($data) {
  $kpi = $data['kpi'];

  $output  = '<div class="dfb-analytics-page">';
  $output .= '<div class="dfb-analytics-header">';
  $output .= '<h2>' . t('Analytics Overview') . '</h2>';
  $output .= '<p class="dfb-dashboard-subtitle">' . t('Aggregated data across all forms') . '</p>';
  $output .= '</div>';

  // KPI stat cards.
  $output .= '<div class="dfb-stats-grid">';
  $output .= _dynamic_form_stat_card(t('Total Submitted'),   $kpi['total_submitted'],  '&#9989;',  'success');
  $output .= _dynamic_form_stat_card(t('Completion Rate'),   $kpi['completion_rate'] . '%', '&#128200;', 'primary');
  $output .= _dynamic_form_stat_card(t('Active Forms'),      $kpi['active_forms'],     '&#128196;', 'accent');
  $output .= _dynamic_form_stat_card(t('Avg / Form'),        $kpi['avg_per_form'],     '&#128202;', 'warning');
  $output .= '</div>';

  // Charts row 1: trend line + status donut side by side.
  $output .= '<div class="dfb-analytics-grid">';

  $output .= '<div class="dfb-chart-card dfb-chart-card--wide">';
  $output .= '<h3 class="dfb-chart-title">' . t('Submissions — Last 30 Days') . '</h3>';
  if (empty($data['trend'])) {
    $output .= '<div class="dfb-analytics-empty">' . t('No submissions in the last 30 days.') . '</div>';
  }
  else {
    $output .= '<div id="dfa-chart-global-trend" class="dfb-chart-container"></div>';
  }
  $output .= '</div>';

  $output .= '<div class="dfb-chart-card">';
  $output .= '<h3 class="dfb-chart-title">' . t('Response Status') . '</h3>';
  $output .= '<div id="dfa-chart-status-donut" class="dfb-chart-container dfb-chart-container--donut"></div>';
  $output .= '</div>';

  $output .= '<div class="dfb-chart-card dfb-chart-card--wide">';
  $output .= '<h3 class="dfb-chart-title">' . t('Top Forms by Responses') . '</h3>';
  if (empty($data['top_forms'])) {
    $output .= '<div class="dfb-analytics-empty">' . t('No submitted responses yet.') . '</div>';
  }
  else {
    $output .= '<div id="dfa-chart-top-forms" class="dfb-chart-container dfb-chart-container--hbar"></div>';
  }
  $output .= '</div>';

  $output .= '</div>'; // .dfb-analytics-grid

  // Tag word cloud card.
  $output .= '<div class="dfb-chart-card dfb-chart-card--wide dfb-wc-card">';
  $output .= '<div class="dfb-wc-header">';
  $output .= '<h3 class="dfb-chart-title">' . t('Tag Word Cloud') . '</h3>';
  $output .= '<div class="dfb-wc-filters">';
  $output .= '<select id="dfa-wc-form" class="dfb-wc-select" aria-label="' . t('Filter by form') . '">';
  $output .= '<option value="">' . t('All Forms') . '</option>';
  $output .= '</select>';
  $output .= '<select id="dfa-wc-question" class="dfb-wc-select" disabled aria-label="' . t('Filter by question') . '">';
  $output .= '<option value="">' . t('All Tag Questions') . '</option>';
  $output .= '</select>';
  $output .= '</div>'; // .dfb-wc-filters
  $output .= '</div>'; // .dfb-wc-header
  $output .= '<div id="dfa-chart-wordcloud" class="dfb-chart-container dfb-chart-container--wordcloud"></div>';
  $output .= '</div>'; // .dfb-wc-card

  $output .= '</div>'; // .dfb-analytics-page

  return $output;
}
