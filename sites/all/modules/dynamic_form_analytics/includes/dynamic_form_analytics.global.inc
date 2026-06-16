<?php

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
    'ajaxTagForms'   => url('dashboard/analytics/wordcloud-tag-forms'),
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
  global $user;

  // Admins ('edit any form') see site-wide data; others see only their forms.
  $form_ids = NULL; // NULL = no restriction.
  if (!user_access('edit any form')) {
    $owned = db_select('dynamic_form_forms', 'f')
      ->fields('f', array('id'))
      ->condition('f.created_by', $user->uid)
      ->isNull('f.deleted_at')
      ->execute()->fetchCol();
    $member = db_select('dynamic_form_members', 'm')
      ->fields('m', array('form_id'))
      ->condition('m.user_id', $user->uid)
      ->isNull('m.deleted_at')
      ->execute()->fetchCol();
    $form_ids = array_values(array_unique(array_merge((array) $owned, (array) $member)));

    if (empty($form_ids)) {
      return array(
        'kpi'         => array(
          'total_submitted' => 0, 'total_started' => 0,
          'completion_rate' => 0, 'active_forms'  => 0, 'avg_per_form' => 0,
        ),
        'trend'       => array(),
        'status_data' => array(
          array('label' => 'Submitted', 'cnt' => 0),
          array('label' => 'Draft',     'cnt' => 0),
        ),
        'top_forms'   => array(),
      );
    }
  }

  // Helper: add form_id IN condition to a SelectQuery on dynamic_form_responses.
  $add_resp_filter = function ($query) use ($form_ids) {
    if ($form_ids !== NULL) {
      $query->condition('form_id', $form_ids, 'IN');
    }
    return $query;
  };

  $q = $add_resp_filter(db_select('dynamic_form_responses', 'r')->isNull('r.deleted_at'));
  $q->addExpression('COUNT(*)');
  $total_started = (int) $q->execute()->fetchField();

  $q = $add_resp_filter(db_select('dynamic_form_responses', 'r')->isNull('r.deleted_at'));
  $q->condition('r.status', 1);
  $q->addExpression('COUNT(*)');
  $total_submitted = (int) $q->execute()->fetchField();

  $fq = db_select('dynamic_form_forms', 'f')
    ->condition('f.status', 1)
    ->isNull('f.deleted_at');
  if ($form_ids !== NULL) {
    $fq->condition('f.id', $form_ids, 'IN');
  }
  $fq->addExpression('COUNT(*)');
  $active_forms = (int) $fq->execute()->fetchField();

  $completion_rate = $total_started > 0
    ? round($total_submitted / $total_started * 100, 1)
    : 0;
  $avg_per_form = $active_forms > 0
    ? round($total_submitted / $active_forms, 1)
    : 0;

  // Submissions over last 30 days.
  $tq = $add_resp_filter(db_select('dynamic_form_responses', 'r')->isNull('r.deleted_at'));
  $tq->condition('r.status', 1);
  $tq->condition('r.submitted_at', REQUEST_TIME - 30 * 86400, '>=');
  $tq->addExpression('DATE(FROM_UNIXTIME(r.submitted_at))', 'day');
  $tq->addExpression('COUNT(*)', 'cnt');
  $tq->groupBy('day');
  $tq->orderBy('day', 'ASC');
  $trend = array();
  foreach ($tq->execute()->fetchAll() as $row) {
    $trend[] = array('day' => $row->day, 'cnt' => (int) $row->cnt);
  }

  $sq = $add_resp_filter(db_select('dynamic_form_responses', 'r')->isNull('r.deleted_at'));
  $sq->fields('r', array('status'));
  $sq->addExpression('COUNT(*)', 'cnt');
  $sq->groupBy('r.status');
  $status_data = array(
    array('label' => 'Submitted', 'cnt' => 0),
    array('label' => 'Draft',     'cnt' => 0),
  );
  foreach ($sq->execute()->fetchAll() as $row) {
    if ((int) $row->status === 1) { $status_data[0]['cnt'] = (int) $row->cnt; }
    else                          { $status_data[1]['cnt'] += (int) $row->cnt; }
  }

  // Top 10 forms by response count.
  $pq = db_select('dynamic_form_responses', 'r');
  $pq->join('dynamic_form_forms', 'f', 'f.id = r.form_id');
  $pq->isNull('r.deleted_at');
  $pq->isNull('f.deleted_at');
  $pq->condition('r.status', 1);
  if ($form_ids !== NULL) {
    $pq->condition('r.form_id', $form_ids, 'IN');
  }
  $pq->fields('f', array('title'));
  $pq->addExpression('COUNT(r.id)', 'cnt');
  $pq->groupBy('r.form_id');
  $pq->orderBy('cnt', 'DESC');
  $pq->range(0, 10);
  $top_forms = array();
  foreach ($pq->execute()->fetchAll() as $row) {
    $top_forms[] = array('label' => check_plain($row->title), 'cnt' => (int) $row->cnt);
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
 * Builds the global analytics page HTML via the theme system.
 */
function _dynamic_form_analytics_global_html($data) {
  $kpi = $data['kpi'];

  $kpi_cards = array(
    array('label' => t('Total Submitted'),  'value' => $kpi['total_submitted'],       'icon' => '<i class="fa-solid fa-circle-check"></i>',  'modifier' => 'success'),
    array('label' => t('Completion Rate'),  'value' => $kpi['completion_rate'] . '%', 'icon' => '<i class="fa-solid fa-chart-line"></i>',    'modifier' => 'primary'),
    array('label' => t('Active Forms'),     'value' => $kpi['active_forms'],          'icon' => '<i class="fa-regular fa-file-lines"></i>',  'modifier' => 'accent'),
    array('label' => t('Avg / Form'),       'value' => $kpi['avg_per_form'],          'icon' => '<i class="fa-solid fa-chart-bar"></i>',     'modifier' => 'warning'),
  );

  return theme('dynamic_form_analytics_global_page', array(
    'radar_url'     => check_url(url('dashboard/analytics/radar')),
    'kpi_cards'     => $kpi_cards,
    'has_trend'     => !empty($data['trend']),
    'has_top_forms' => !empty($data['top_forms']),
  ));
}
