<?php

/**
 * @file
 * Radar visualization page for Dynamic Form Analytics.
 *
 * Plots all forms on a 3-segment radar (visibility: public / restricted /
 * members-only) where radial distance reflects submission volume and dot
 * color reflects completion rate.
 */

// ---------------------------------------------------------------------------
// Page callback
// ---------------------------------------------------------------------------

/**
 * Page callback for dashboard/analytics/radar.
 */
function dynamic_form_analytics_radar_page() {
  drupal_set_title(t('Form Radar'));

  module_load_include('inc', 'dynamic_form', 'includes/dynamic_form.dashboard');

  $module_path = drupal_get_path('module', 'dynamic_form_analytics');
  $d3_path     = libraries_get_path('d3');
  if ($d3_path) {
    drupal_add_js($d3_path . '/d3.min.js', array('type' => 'file', 'weight' => -10, 'group' => JS_LIBRARY));
  }
  else {
    drupal_add_js('sites/all/libraries/d3/d3.min.js', array('type' => 'file', 'weight' => -10, 'group' => JS_LIBRARY));
  }

  drupal_add_css($module_path . '/css/analytics.css');
  drupal_add_js($module_path . '/js/analytics.js');

  $forms = _dynamic_form_analytics_radar_data();
  drupal_add_js(array('dfAnalytics' => array(
    'radar'      => $forms,
    'radarAjax'  => url('dashboard/analytics/radar-data'),
  )), 'setting');

  $sidebar_html = _dynamic_form_dashboard_sidebar('radar');
  $content_html = _dynamic_form_analytics_radar_html();

  return theme('dynamic_form_dashboard', array(
    'sidebar' => $sidebar_html,
    'content' => $content_html,
  ));
}

// ---------------------------------------------------------------------------
// Data
// ---------------------------------------------------------------------------

/**
 * Returns all non-deleted forms with their submission stats.
 *
 * @param string|null $from  Optional start date (YYYY-MM-DD); filters responses by created_at.
 * @param string|null $to    Optional end date   (YYYY-MM-DD); filters responses by created_at.
 *
 * @return array  [['id','title','visibility','status','total_started','total_submitted','completion_rate'], ...]
 */
function _dynamic_form_analytics_radar_data($from = NULL, $to = NULL) {
  $date_cond = 'r.deleted_at IS NULL';
  $args      = array();

  if ($from) {
    $date_cond         .= ' AND r.created_at >= :from';
    $args[':from']      = strtotime($from . ' 00:00:00');
  }
  if ($to) {
    $date_cond         .= ' AND r.created_at <= :to';
    $args[':to']        = strtotime($to . ' 23:59:59');
  }

  $sql = 'SELECT f.id, f.title, f.visibility, f.status,
                 COUNT(r.id)                                    AS total_started,
                 SUM(CASE WHEN r.status = 1 THEN 1 ELSE 0 END) AS total_submitted
          FROM {dynamic_form_forms} f
          LEFT JOIN {dynamic_form_responses} r
                 ON r.form_id = f.id AND ' . $date_cond . '
          WHERE f.deleted_at IS NULL
          GROUP BY f.id
          ORDER BY total_submitted DESC';

  $rows = db_query($sql, $args)->fetchAll();

  $forms = array();
  foreach ($rows as $row) {
    $started   = (int) $row->total_started;
    $submitted = (int) $row->total_submitted;
    $forms[] = array(
      'id'              => (int) $row->id,
      'title'           => check_plain($row->title),
      'visibility'      => $row->visibility ? $row->visibility : 'p',
      'status'          => (int) $row->status,
      'total_started'   => $started,
      'total_submitted' => $submitted,
      'completion_rate' => $started > 0 ? round($submitted / $started, 2) : 0,
    );
  }
  return $forms;
}

// ---------------------------------------------------------------------------
// AJAX callback
// ---------------------------------------------------------------------------

/**
 * AJAX endpoint: dashboard/analytics/radar-data
 * Accepts optional GET params: from (YYYY-MM-DD), to (YYYY-MM-DD).
 */
function dynamic_form_analytics_radar_ajax() {
  $from = isset($_GET['from']) && preg_match('/^\d{4}-\d{2}-\d{2}$/', $_GET['from']) ? $_GET['from'] : NULL;
  $to   = isset($_GET['to'])   && preg_match('/^\d{4}-\d{2}-\d{2}$/', $_GET['to'])   ? $_GET['to']   : NULL;
  drupal_json_output(_dynamic_form_analytics_radar_data($from, $to));
}

// ---------------------------------------------------------------------------
// HTML
// ---------------------------------------------------------------------------

/**
 * Builds the radar page HTML — full-height chart with minimal header.
 */
function _dynamic_form_analytics_radar_html() {
  $output  = '<div class="dfb-analytics-page">';

  $output .= '<div class="dfb-analytics-header dfb-analytics-header--split">';
  $output .= '<div class="dfb-analytics-header-text">';
  $output .= '<h2>' . t('Form Radar') . '</h2>';
  $output .= '<p class="dfb-dashboard-subtitle">'
    . t('Dot position: segment = visibility, ring = submission volume. Dot color = completion rate.')
    . '</p>';
  $output .= '</div>';
  $output .= '<a href="' . url('dashboard/analytics') . '" class="dfb-btn dfb-btn-ghost">'
    . '&larr; ' . t('Back to Analytics') . '</a>';
  $output .= '</div>';

  // Filter bar.
  $output .= '<div id="dfa-radar-filters" class="dfa-radar-filters">';

  // Visibility toggles.
  $output .= '<div class="dfa-filter-group">';
  $output .= '<span class="dfa-filter-label">' . t('Visibility') . '</span>';
  $output .= '<div class="dfa-filter-toggles">';
  $output .= '<button class="dfa-filter-toggle active" data-filter="visibility" data-key="p" style="--toggle-color:#4f6ef7">' . t('Public') . '</button>';
  $output .= '<button class="dfa-filter-toggle active" data-filter="visibility" data-key="r" style="--toggle-color:#f59e0b">' . t('Restricted') . '</button>';
  $output .= '<button class="dfa-filter-toggle active" data-filter="visibility" data-key="m" style="--toggle-color:#8b5cf6">' . t('Members Only') . '</button>';
  $output .= '</div></div>';

  // Ring toggles.
  $output .= '<div class="dfa-filter-group">';
  $output .= '<span class="dfa-filter-label">' . t('Volume Ring') . '</span>';
  $output .= '<div class="dfa-filter-toggles">';
  foreach (array('Emerging', 'Growing', 'Active', 'Popular') as $ring) {
    $output .= '<button class="dfa-filter-toggle active" data-filter="ring" data-key="' . $ring . '">' . t($ring) . '</button>';
  }
  $output .= '</div></div>';

  // Completion rate slider.
  $output .= '<div class="dfa-filter-group">';
  $output .= '<span class="dfa-filter-label">' . t('Min Completion') . '</span>';
  $output .= '<div class="dfa-filter-range">';
  $output .= '<input type="range" id="dfa-completion-min" min="0" max="100" value="0" step="5">';
  $output .= '<span id="dfa-completion-val">0%</span>';
  $output .= '</div></div>';

  // Date range.
  $output .= '<div class="dfa-filter-group">';
  $output .= '<span class="dfa-filter-label">' . t('Response Date') . '</span>';
  $output .= '<div class="dfa-filter-date-group">';
  $output .= '<input type="date" id="dfa-date-from" class="dfa-date-input">';
  $output .= '<span class="dfa-date-sep">' . t('to') . '</span>';
  $output .= '<input type="date" id="dfa-date-to" class="dfa-date-input">';
  $output .= '<button id="dfa-date-apply" class="dfa-filter-apply-btn">' . t('Apply') . '</button>';
  $output .= '<button id="dfa-date-clear" class="dfa-filter-clear-btn">' . t('Clear') . '</button>';
  $output .= '</div></div>';

  $output .= '</div>'; // .dfa-radar-filters

  $output .= '<div class="dfb-chart-card">';
  $output .= '<div id="dfa-chart-radar" class="dfb-chart-container dfb-chart-container--radar"></div>';
  $output .= '</div>';

  $output .= '</div>'; // .dfb-analytics-page

  return $output;
}
