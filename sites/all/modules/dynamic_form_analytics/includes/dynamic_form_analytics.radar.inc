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
  drupal_add_js(array('dfAnalytics' => array('radar' => $forms)), 'setting');

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
 * @return array  [['id','title','visibility','status','total_started','total_submitted','completion_rate'], ...]
 */
function _dynamic_form_analytics_radar_data() {
  $rows = db_query(
    'SELECT f.id, f.title, f.visibility, f.status,
            COUNT(r.id)                                          AS total_started,
            SUM(CASE WHEN r.status = 1 THEN 1 ELSE 0 END)       AS total_submitted
     FROM {dynamic_form_forms} f
     LEFT JOIN {dynamic_form_responses} r
            ON r.form_id = f.id AND r.deleted_at IS NULL
     WHERE f.deleted_at IS NULL
     GROUP BY f.id
     ORDER BY total_submitted DESC'
  )->fetchAll();

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

  $output .= '<div class="dfb-chart-card">';
  $output .= '<div id="dfa-chart-radar" class="dfb-chart-container dfb-chart-container--radar"></div>';
  $output .= '</div>';

  $output .= '</div>'; // .dfb-analytics-page

  return $output;
}
