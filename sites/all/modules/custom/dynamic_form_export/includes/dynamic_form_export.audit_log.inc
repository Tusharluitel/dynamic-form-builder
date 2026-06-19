<?php

/**
 * Page form — shows row count and a "Start Export" button.
 */
function dynamic_form_export_audit_log_form($form, &$form_state) {
  drupal_set_breadcrumb(array(
    l(t('Home'), '<front>'),
    l(t('Dashboard'), 'dashboard'),
    t('Export Audit Log'),
  ));

  $total = (int) db_query('SELECT COUNT(*) FROM {dynamic_form_audit_log}')->fetchField();

  $form['info'] = array(
    '#markup' => '<p>' . t('Ready to export <strong>@count</strong> audit log entries. Processing runs in batches of 500 rows and shows live progress.', array('@count' => number_format($total))) . '</p>',
  );

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => t('Start Export'),
  );
  $form['actions']['cancel'] = array(
    '#markup' => ' ' . l(t('Cancel'), 'dashboard'),
  );

  return $form;
}

/**
 * Form submit — delegates to the shared batch starter.
 */
function dynamic_form_export_audit_log_form_submit($form, &$form_state) {
  _dynamic_form_export_audit_log_start_batch();
  $form_state['redirect'] = FALSE;
  batch_process('dashboard/export');
}

/**
 * Builds and registers the audit log batch.
 *
 * Called both from the standalone form submit and from the hub form submit,
 * so the batch setup lives in exactly one place.
 *
 * @param string $from  Optional ISO date string (Y-m-d) for the start of the range.
 * @param string $to    Optional ISO date string (Y-m-d) for the end of the range.
 */
function _dynamic_form_export_audit_log_start_batch($from = '', $to = '') {
  // Build the base query first, apply date conditions, then wrap in countQuery().
  // countQuery() wraps the query in a subquery and the table alias is no longer
  // visible in the outer scope, so conditions must be on the inner query.
  $query = db_select('dynamic_form_audit_log', 'l');
  if (!empty($from)) {
    $query->condition('l.created_at', strtotime($from . ' 00:00:00'), '>=');
  }
  if (!empty($to)) {
    $query->condition('l.created_at', strtotime($to . ' 23:59:59'), '<=');
  }
  $total     = (int) $query->countQuery()->execute()->fetchField();
  $batch_size = 500;
  $inc_file   = drupal_get_path('module', 'dynamic_form_export') . '/includes/dynamic_form_export.audit_log.inc';

  $operations   = array();
  $operations[] = array('_dynamic_form_export_audit_log_init', array($total, $from, $to));

  for ($offset = 0; $offset < $total; $offset += $batch_size) {
    $operations[] = array('_dynamic_form_export_audit_log_process', array($offset, $batch_size, $total, $from, $to));
  }

  batch_set(array(
    'title'            => t('Exporting Audit Log'),
    'operations'       => $operations,
    'finished'         => '_dynamic_form_export_audit_log_finished',
    'init_message'     => t('Preparing export…'),
    'progress_message' => t('Processing batch @current of @total…'),
    'error_message'    => t('The export encountered an error. Please try again.'),
    'file'             => $inc_file,
  ));
}

/**
 * Batch op #0: create the temp JSONL file and write the header row.
 *
 * Stores the temp file path and a running row counter in $context['results']
 * so subsequent operations can append to the same file.
 */
function _dynamic_form_export_audit_log_init($total, $from, $to, &$context) {
  $temp_file = _dynamic_form_export_temp_dir() . '/dfexport_audit_' . REQUEST_TIME . '_' . mt_rand(1000, 9999) . '.jsonl';

  $header = array('ID', 'Entity Type', 'Entity ID', 'Action', 'Actor', 'IP Address', 'Timestamp', 'Old Value', 'New Value');
  file_put_contents($temp_file, json_encode($header) . "\n");

  $context['results']['temp_file']    = $temp_file;
  $context['results']['rows_written'] = 0;
  $context['results']['total']        = $total;
  $context['message']                 = t('Export file initialized.');
}

/**
 * Batch op #1..N: query one chunk and append rows as JSON lines to the temp file.
 */
function _dynamic_form_export_audit_log_process($offset, $batch_size, $total, $from, $to, &$context) {
  if (empty($context['results']['temp_file'])) {
    return;
  }

  $query = db_select('dynamic_form_audit_log', 'l')
    ->fields('l')
    ->fields('u', array('name'))
    ->orderBy('l.id', 'ASC');
  $query->leftJoin('users', 'u', 'u.uid = l.actor_uid');
  $query->addField('u', 'name', 'actor_name');
  if (!empty($from)) {
    $query->condition('l.created_at', strtotime($from . ' 00:00:00'), '>=');
  }
  if (!empty($to)) {
    $query->condition('l.created_at', strtotime($to . ' 23:59:59'), '<=');
  }
  $rows = $query->range($offset, $batch_size)->execute();

  $lines = '';
  $count = 0;
  foreach ($rows as $row) {
    $actor = ($row->actor_uid == 0)
      ? 'Anonymous'
      : ($row->actor_name ?: 'uid:' . $row->actor_uid);

    $cells = array(
      (int) $row->id,
      $row->entity_type,
      (int) $row->entity_id,
      $row->action,
      $actor,
      $row->ip_address ?: '',
      format_date($row->created_at, 'custom', 'Y-m-d H:i:s'),
      _dynamic_form_export_format_json_field($row->old_value),
      _dynamic_form_export_format_json_field($row->new_value),
    );

    $lines .= json_encode($cells) . "\n";
    $count++;
  }

  if ($lines !== '') {
    file_put_contents($context['results']['temp_file'], $lines, FILE_APPEND | LOCK_EX);
  }

  $context['results']['rows_written'] += $count;
  $done = $offset + $count;

  $context['message'] = t('Exported @done of @total records…', array(
    '@done'  => number_format($done),
    '@total' => number_format($total),
  ));
}

/**
 * Batch finished callback: convert the JSONL temp file to XLSX using OpenSpout.
 *
 * On success: stores the XLSX path in $_SESSION and lets batch_process()
 * redirect to dashboard/export/download.
 * On failure: sets an error message and redirects back to the export form.
 */
function _dynamic_form_export_audit_log_finished($success, $results, $operations) {
  if (!$success || empty($results['temp_file']) || !file_exists($results['temp_file'])) {
    drupal_set_message(t('Export failed or produced no output. Please try again.'), 'error');
    drupal_goto('dashboard/export/audit-log');
  }

  require_once DRUPAL_ROOT . '/sites/all/libraries/openspout/autoload.php';

  $xlsx_path = _dynamic_form_export_temp_dir() . '/dfexport_audit_' . REQUEST_TIME . '_' . mt_rand(1000, 9999) . '.xlsx';

  $header_style = (new \OpenSpout\Common\Entity\Style\Style())->setFontBold();

  // Tell OpenSpout to use our writable directory for its internal ZIP temp
  // files. Without this it falls back to sys_get_temp_dir() which is
  // drwx------ on macOS and not writable by Apache's daemon user.
  $options = new \OpenSpout\Writer\XLSX\Options();
  $options->setTempFolder(_dynamic_form_export_temp_dir());

  $writer = new \OpenSpout\Writer\XLSX\Writer($options);
  $writer->openToFile($xlsx_path);

  $handle   = fopen($results['temp_file'], 'r');
  $is_first = TRUE;

  while (($line = fgets($handle)) !== FALSE) {
    $line = trim($line);
    if ($line === '') {
      continue;
    }
    $cells = json_decode($line, TRUE);
    if (!is_array($cells)) {
      continue;
    }
    if ($is_first) {
      $writer->addRow(\OpenSpout\Common\Entity\Row::fromValues($cells, $header_style));
      $is_first = FALSE;
    }
    else {
      $writer->addRow(\OpenSpout\Common\Entity\Row::fromValues($cells));
    }
  }

  fclose($handle);
  $writer->close();

  @unlink($results['temp_file']);

  if (!file_exists($xlsx_path)) {
    drupal_set_message(t('Failed to generate the XLSX file. Please try again.'), 'error');
    drupal_goto('dashboard/export/audit-log');
  }

  $_SESSION['dynamic_form_export'] = array(
    'file'     => $xlsx_path,
    'filename' => 'audit-log-' . date('Y-m-d') . '.xlsx',
    'rows'     => $results['rows_written'],
  );

  drupal_set_message(t('Audit log exported successfully — @count rows.', array(
    '@count' => number_format($results['rows_written']),
  )));
}

/**
 * Decode a JSON field and re-encode it as readable text for a spreadsheet cell.
 *
 * Excel hard-limits cells to 32 767 characters; we stay well under at 5 000.
 */
function _dynamic_form_export_format_json_field($json_string) {
  if ($json_string === NULL || $json_string === '') {
    return '';
  }
  $decoded = json_decode($json_string, TRUE);
  if ($decoded === NULL) {
    return strlen($json_string) > 5000 ? substr($json_string, 0, 5000) . '…' : $json_string;
  }
  $out = json_encode($decoded, JSON_PRETTY_PRINT | JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES);
  return strlen($out) > 5000 ? substr($out, 0, 5000) . '…' : $out;
}
