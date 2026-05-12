<?php

/**
 * #post_render callback: changes type="text" to type="date" on a textfield.
 *
 * Drupal 7's theme_textfield() hardcodes type="text", overwriting any
 * #attributes['type']. This post-render patch swaps it to type="date" so
 * the browser renders a native date picker with no default value.
 */
function _dynamic_form_export_date_input_post_render($markup, $element) {
  return preg_replace('/\btype="text"/', 'type="date"', $markup, 1);
}

/**
 * Export hub page — single form with type dropdown, optional form picker, and date range.
 */
function dynamic_form_export_hub_page() {
  module_load_include('inc', 'dynamic_form', 'includes/dynamic_form.dashboard');

  drupal_set_title(t('Export'));

  $module_path = drupal_get_path('module', 'dynamic_form_export');
  $select2_base = 'sites/all/libraries/select2';

  drupal_add_css($select2_base . '/select2.css');
  drupal_add_js($select2_base . '/select2.min.js', array('weight' => 10));
  drupal_add_css($module_path . '/css/export-hub.css');
  drupal_add_js($module_path . '/js/export-hub.js', array('weight' => 15));

  drupal_set_breadcrumb(array(
    l(t('Home'), '<front>'),
    l(t('Dashboard'), 'dashboard'),
    t('Export'),
  ));

  // If a completed export is waiting, trigger the download via a hidden iframe
  // so the browser shows the Save-As dialog without navigating away from this
  // styled page.  The stream endpoint streams the binary and clears the session.
  $download_markup = '';
  if (!empty($_SESSION['dynamic_form_export']['file']) && file_exists($_SESSION['dynamic_form_export']['file'])) {
    $stream_url = url('dashboard/export/stream', array('absolute' => TRUE));
    $download_markup = '<iframe src="' . check_plain($stream_url) . '" style="display:none" aria-hidden="true"></iframe>';
  }

  $sidebar  = _dynamic_form_dashboard_sidebar('export');
  $content  = $download_markup . drupal_render(drupal_get_form('dynamic_form_export_hub_form'));

  return theme('dynamic_form_dashboard', array(
    'sidebar' => $sidebar,
    'content' => $content,
  ));
}

/**
 * Export hub form — type selector, conditional form picker, date range.
 */
function dynamic_form_export_hub_form($form, &$form_state) {
  $form['#attributes']['class'][] = 'dfb-export-hub-form';

  $form['header'] = array(
    '#markup' => '<div class="dfb-page-header">'
      . '<h1 class="dfb-page-title">' . t('Export Data') . '</h1>'
      . '<p class="dfb-page-subtitle">' . t('Choose a dataset, apply an optional date range, then export.') . '</p>'
      . '</div>',
  );

  $form['export_type'] = array(
    '#type'       => 'select',
    '#title'      => t('Export type'),
    '#options'    => array(
      ''          => t('— Select what to export —'),
      'audit-log' => t('Audit Log'),
      'responses' => t('Responses'),
    ),
    '#required'   => TRUE,
    '#attributes' => array(
      'class' => array('dfb-export-select'),
      'id'    => 'dfb-export-type',
    ),
  );

  // Form picker — visible only when export_type = responses.
  // Wrapped in a plain div so JS can show/hide it independently of Select2.
  $forms = db_select('dynamic_form_forms', 'f')
    ->fields('f', array('id', 'title'))
    ->isNull('f.deleted_at')
    ->orderBy('f.title', 'ASC')
    ->execute()
    ->fetchAllKeyed(0, 1);

  $form_options = array('' => t('— Select a form —')) + $forms;

  $form['form_id_wrapper'] = array(
    '#type'       => 'container',
    '#attributes' => array('class' => array('dfb-export-form-picker'), 'id' => 'dfb-export-form-picker'),
  );

  $form['form_id_wrapper']['export_form_id'] = array(
    '#type'       => 'select',
    '#title'      => t('Form'),
    '#options'    => $form_options,
    '#attributes' => array(
      'class' => array('dfb-export-form-select'),
      'id'    => 'dfb-export-form-id',
    ),
  );

  // Date range.
  $form['date_range'] = array(
    '#type'       => 'container',
    '#attributes' => array('class' => array('dfb-export-date-row')),
  );

  $form['date_range']['from'] = array(
    '#type'         => 'textfield',
    '#title'        => t('From'),
    '#attributes'   => array('class' => array('dfb-export-date')),
    '#post_render'  => array('_dynamic_form_export_date_input_post_render'),
  );

  $form['date_range']['to'] = array(
    '#type'         => 'textfield',
    '#title'        => t('To'),
    '#attributes'   => array('class' => array('dfb-export-date')),
    '#post_render'  => array('_dynamic_form_export_date_input_post_render'),
  );

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type'       => 'submit',
    '#value'      => t('Export'),
    '#attributes' => array('class' => array('dfb-export-submit-btn')),
  );

  return $form;
}

/**
 * Validation.
 */
function dynamic_form_export_hub_form_validate($form, &$form_state) {
  $type = $form_state['values']['export_type'];
  $from = $form_state['values']['from'];
  $to   = $form_state['values']['to'];

  if ($type === 'forms') {
    form_set_error('export_type', t('Forms export is not yet available.'));
  }

  if ($type === 'responses') {
    if (empty($form_state['values']['export_form_id'])) {
      form_set_error('export_form_id', t('Please select a form to export responses for.'));
    }
  }

  if (!empty($from) && !empty($to) && $from > $to) {
    form_set_error('date_range][to', t('"To" date must be on or after the "From" date.'));
  }
}

/**
 * Submit — start the appropriate batch.
 */
function dynamic_form_export_hub_form_submit($form, &$form_state) {
  $type    = $form_state['values']['export_type'];
  $from    = $form_state['values']['from'];
  $to      = $form_state['values']['to'];
  $form_id = isset($form_state['values']['export_form_id']) ? (int) $form_state['values']['export_form_id'] : 0;

  $form_state['redirect'] = FALSE;

  if ($type === 'audit-log') {
    module_load_include('inc', 'dynamic_form_export', 'includes/dynamic_form_export.audit_log');
    _dynamic_form_export_audit_log_start_batch($from, $to);
    batch_process('dashboard/export');
  }
  elseif ($type === 'responses') {
    module_load_include('inc', 'dynamic_form_export', 'includes/dynamic_form_export.responses');
    _dynamic_form_export_responses_start_batch($form_id, $from, $to);
    batch_process('dashboard/export');
  }
}
