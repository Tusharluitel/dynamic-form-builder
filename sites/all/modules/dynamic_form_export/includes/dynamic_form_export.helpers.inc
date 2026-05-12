<?php

/**
 * Download page.
 *
 * Serves a minimal HTML page that immediately triggers the file download via
 * a hidden iframe (so the browser never navigates away) and then redirects
 * the user to the dashboard.  The actual binary is streamed by the separate
 * dynamic_form_export_stream_file() callback.
 */
function dynamic_form_export_download_page() {
  if (empty($_SESSION['dynamic_form_export']['file'])) {
    drupal_set_message(t('No export file found. Please start a new export.'), 'warning');
    drupal_goto('dashboard/export/audit-log');
  }

  if (!file_exists($_SESSION['dynamic_form_export']['file'])) {
    unset($_SESSION['dynamic_form_export']);
    drupal_set_message(t('The export file has expired. Please run the export again.'), 'warning');
    drupal_goto('dashboard/export/audit-log');
  }

  $stream_url = url('dashboard/export/stream');
  $dashboard_url = url('dashboard');

  // The page itself is invisible to the user — it loads the file in a hidden
  // iframe (which triggers the browser Save-As dialog without navigating away)
  // then redirects the top-level window to the dashboard where the success
  // toast from drupal_set_message will appear.
  $html  = '<!DOCTYPE html><html><head><meta charset="utf-8">';
  $html .= '<title>Downloading…</title></head><body>';
  $html .= '<iframe src="' . check_plain($stream_url) . '" style="display:none"></iframe>';
  $html .= '<script>';
  $html .= 'setTimeout(function(){ window.location = ' . drupal_json_encode($dashboard_url) . '; }, 1500);';
  $html .= '</script>';
  $html .= '</body></html>';

  print $html;
  drupal_exit();
}

/**
 * Stream callback — sends the raw XLSX binary and cleans up.
 */
function dynamic_form_export_stream_file() {
  if (empty($_SESSION['dynamic_form_export']['file'])) {
    drupal_exit();
  }

  $file_path = $_SESSION['dynamic_form_export']['file'];
  $filename  = $_SESSION['dynamic_form_export']['filename'];

  if (!file_exists($file_path)) {
    unset($_SESSION['dynamic_form_export']);
    drupal_exit();
  }

  while (ob_get_level()) {
    ob_end_clean();
  }

  drupal_add_http_header('Content-Type', 'application/vnd.openxmlformats-officedocument.spreadsheetml.sheet');
  drupal_add_http_header('Content-Disposition', 'attachment; filename="' . $filename . '"');
  drupal_add_http_header('Content-Length', filesize($file_path));
  drupal_add_http_header('Cache-Control', 'private, no-cache, must-revalidate');
  drupal_add_http_header('Pragma', 'no-cache');
  drupal_send_headers();

  $fd = fopen($file_path, 'rb');
  while (!feof($fd)) {
    print fread($fd, 8192);
  }
  fclose($fd);

  unset($_SESSION['dynamic_form_export']);
  @unlink($file_path);

  drupal_exit();
}
