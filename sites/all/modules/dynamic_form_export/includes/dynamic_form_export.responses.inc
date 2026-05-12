<?php

/**
 * Builds and registers the responses batch for a given form.
 *
 * Column layout (mirrors Google Forms export):
 *   Timestamp | Email | Respondent | Q1 | Q2 | … (one col per question,
 *   ordered by section position then question position)
 */
function _dynamic_form_export_responses_start_batch($form_id, $from = '', $to = '') {
  // Count matching responses.
  $count_q = db_select('dynamic_form_responses', 'r')
    ->condition('r.form_id', $form_id)
    ->condition('r.status', 1)
    ->isNotNull('r.submitted_at')
    ->isNull('r.deleted_at');
  if (!empty($from)) {
    $count_q->condition('r.submitted_at', strtotime($from . ' 00:00:00'), '>=');
  }
  if (!empty($to)) {
    $count_q->condition('r.submitted_at', strtotime($to . ' 23:59:59'), '<=');
  }
  $total = (int) $count_q->countQuery()->execute()->fetchField();

  // Load questions to build the header row and column map.
  $questions = _dynamic_form_export_responses_questions($form_id);

  $batch_size = 200;
  $inc_file   = drupal_get_path('module', 'dynamic_form_export') . '/includes/dynamic_form_export.responses.inc';

  $operations   = array();
  $operations[] = array('_dynamic_form_export_responses_init', array($total, $form_id, $questions, $from, $to));

  for ($offset = 0; $offset < $total; $offset += $batch_size) {
    $operations[] = array('_dynamic_form_export_responses_process', array($offset, $batch_size, $total, $form_id, $questions, $from, $to));
  }

  // Load the form title for the filename.
  $form_title = db_query('SELECT title FROM {dynamic_form_forms} WHERE id = :id', array(':id' => $form_id))->fetchField();
  $safe_title = preg_replace('/[^a-z0-9]+/i', '-', strtolower($form_title ?: 'form-' . $form_id));

  batch_set(array(
    'title'            => t('Exporting Responses — @form', array('@form' => $form_title)),
    'operations'       => $operations,
    'finished'         => '_dynamic_form_export_responses_finished',
    'init_message'     => t('Preparing export…'),
    'progress_message' => t('Processing batch @current of @total…'),
    'error_message'    => t('The export encountered an error. Please try again.'),
    'file'             => $inc_file,
  ));

  // Store the filename in session so the finished callback can use it.
  $_SESSION['dynamic_form_export_filename'] = 'responses-' . $safe_title . '-' . date('Y-m-d') . '.xlsx';
}

/**
 * Returns an ordered array of question objects for the given form.
 *
 * Each object has: id, label, question_type, section_title, section_pos, question_pos.
 * Questions are ordered by section position then question position.
 * Multi-section forms prefix the header with "Section: ".
 */
function _dynamic_form_export_responses_questions($form_id) {
  $query = db_select('dynamic_form_questions', 'q');
  $query->join('dynamic_form_sections', 's', 's.id = q.section_id');
  $query->fields('q', array('id', 'label', 'type'));
  $query->addField('q', 'position', 'question_pos');
  $query->addField('s', 'name', 'section_title');
  $query->addField('s', 'position', 'section_pos');
  $query->condition('q.form_id', $form_id)
    ->isNull('q.deleted_at')
    ->isNull('s.deleted_at')
    ->orderBy('s.position', 'ASC')
    ->orderBy('q.position', 'ASC');

  $questions = $query->execute()->fetchAll();

  // Determine whether the form has multiple sections.
  $section_titles = array_unique(array_map(function ($q) { return $q->section_title; }, $questions));
  $multi_section  = count($section_titles) > 1;

  // Build column header labels.
  foreach ($questions as &$q) {
    $q->header = $multi_section
      ? $q->section_title . ': ' . $q->label
      : $q->label;
  }
  unset($q);

  return $questions;
}

// ── Batch operations ──────────────────────────────────────────────────────────

/**
 * Batch op #0: create temp JSONL file and write the header row.
 */
function _dynamic_form_export_responses_init($total, $form_id, $questions, $from, $to, &$context) {
  $temp_file = _dynamic_form_export_temp_dir() . '/dfexport_resp_' . $form_id . '_' . REQUEST_TIME . '_' . mt_rand(1000, 9999) . '.jsonl';

  $header = array('Timestamp', 'Email', 'Respondent');
  foreach ($questions as $q) {
    $header[] = $q->header;
  }

  file_put_contents($temp_file, json_encode($header) . "\n");

  $context['results']['temp_file']    = $temp_file;
  $context['results']['rows_written'] = 0;
  $context['results']['total']        = $total;
  $context['message']                 = t('Export file initialized.');
}

/**
 * Batch op #1..N: query a chunk of responses and append rows to the JSONL file.
 */
function _dynamic_form_export_responses_process($offset, $batch_size, $total, $form_id, $questions, $from, $to, &$context) {
  if (empty($context['results']['temp_file'])) {
    return;
  }

  // Query response rows for this chunk.
  $q = db_select('dynamic_form_responses', 'r');
  $q->leftJoin('users', 'u', 'u.uid = r.user_id');
  $q->fields('r', array('id', 'user_id', 'guest_email', 'submitted_at'));
  $q->addField('u', 'name', 'username');
  $q->addField('u', 'mail', 'user_mail');
  $q->condition('r.form_id', $form_id)
    ->condition('r.status', 1)
    ->isNotNull('r.submitted_at')
    ->isNull('r.deleted_at')
    ->orderBy('r.submitted_at', 'ASC');

  if (!empty($from)) {
    $q->condition('r.submitted_at', strtotime($from . ' 00:00:00'), '>=');
  }
  if (!empty($to)) {
    $q->condition('r.submitted_at', strtotime($to . ' 23:59:59'), '<=');
  }

  $responses = $q->range($offset, $batch_size)->execute()->fetchAll();

  if (empty($responses)) {
    return;
  }

  // Collect response IDs and load all their answers in one query.
  $response_ids = array_map(function ($r) { return (int) $r->id; }, $responses);

  $answer_rows = db_select('dynamic_form_answers', 'a')
    ->fields('a', array('response_id', 'question_id', 'value'))
    ->condition('a.response_id', $response_ids, 'IN')
    ->execute()
    ->fetchAll();

  // Key answers as $answers[response_id][question_id] = value.
  $answers = array();
  foreach ($answer_rows as $ar) {
    $answers[(int) $ar->response_id][(int) $ar->question_id] = $ar->value;
  }

  // Build question ID list for fast column lookup.
  $question_ids = array_map(function ($q) { return (int) $q->id; }, $questions);

  // Build one JSON line per response.
  $lines = '';
  foreach ($responses as $r) {
    $email     = $r->user_id ? ($r->user_mail ?: '') : ($r->guest_email ?: '');
    $name      = $r->user_id ? ($r->username ?: 'uid:' . $r->user_id) : 'Anonymous';
    $timestamp = format_date($r->submitted_at, 'custom', 'Y-m-d H:i:s');

    $row = array($timestamp, $email, $name);

    foreach ($question_ids as $qid) {
      $raw   = isset($answers[(int) $r->id][$qid]) ? $answers[(int) $r->id][$qid] : '';
      $row[] = _dynamic_form_export_format_answer($raw, _dynamic_form_export_question_type_by_id($questions, $qid));
    }

    $lines .= json_encode($row) . "\n";
    $context['results']['rows_written']++;
  }

  file_put_contents($context['results']['temp_file'], $lines, FILE_APPEND | LOCK_EX);

  $done = $offset + count($responses);
  $context['message'] = t('Exported @done of @total responses…', array(
    '@done'  => number_format($done),
    '@total' => number_format($total),
  ));
}

/**
 * Batch finished: convert JSONL → XLSX, store in session.
 */
function _dynamic_form_export_responses_finished($success, $results, $operations) {
  if (!$success || empty($results['temp_file']) || !file_exists($results['temp_file'])) {
    drupal_set_message(t('Response export failed or produced no output. Please try again.'), 'error');
    drupal_goto('dashboard/export');
  }

  require_once DRUPAL_ROOT . '/sites/all/libraries/openspout/autoload.php';

  $xlsx_path = _dynamic_form_export_temp_dir() . '/dfexport_resp_' . REQUEST_TIME . '_' . mt_rand(1000, 9999) . '.xlsx';

  $header_style = (new \OpenSpout\Common\Entity\Style\Style())->setFontBold();

  $options = new \OpenSpout\Writer\XLSX\Options();
  $options->setTempFolder(_dynamic_form_export_temp_dir());

  $writer   = new \OpenSpout\Writer\XLSX\Writer($options);
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
    $writer->addRow(\OpenSpout\Common\Entity\Row::fromValues($cells, $is_first ? $header_style : NULL));
    $is_first = FALSE;
  }
  fclose($handle);
  $writer->close();

  @unlink($results['temp_file']);

  if (!file_exists($xlsx_path)) {
    drupal_set_message(t('Failed to generate the XLSX file. Please try again.'), 'error');
    drupal_goto('dashboard/export');
  }

  $filename = isset($_SESSION['dynamic_form_export_filename'])
    ? $_SESSION['dynamic_form_export_filename']
    : 'responses-' . date('Y-m-d') . '.xlsx';

  unset($_SESSION['dynamic_form_export_filename']);

  $_SESSION['dynamic_form_export'] = array(
    'file'     => $xlsx_path,
    'filename' => $filename,
    'rows'     => $results['rows_written'],
  );

  drupal_set_message(t('Responses exported successfully', array(
    '@count' => number_format($results['rows_written']),
  )));
}

// ── Helpers ───────────────────────────────────────────────────────────────────

/**
 * Formats a raw answer value for the spreadsheet based on question type.
 *
 *  - checkbox / multi-select  → JSON-decoded array → comma-separated string
 *  - file                     → JSON-decoded array of objects → comma-separated filenames
 *  - everything else          → plain string
 */
function _dynamic_form_export_format_answer($raw, $type) {
  if ($raw === NULL || $raw === '') {
    return '';
  }

  if (in_array($type, array('checkbox', 'file')) || strpos($raw, '[') === 0) {
    $decoded = json_decode($raw, TRUE);
    if (is_array($decoded)) {
      if ($type === 'file') {
        $names = array();
        foreach ($decoded as $f) {
          $names[] = is_array($f) ? ($f['filename'] ?? $f['name'] ?? '') : (string) $f;
        }
        return implode(', ', array_filter($names));
      }
      return implode(', ', $decoded);
    }
  }

  return $raw;
}

/**
 * Returns the type string for a given question ID from the preloaded list.
 */
function _dynamic_form_export_question_type_by_id($questions, $qid) {
  foreach ($questions as $q) {
    if ((int) $q->id === (int) $qid) {
      return $q->type;
    }
  }
  return 'text';
}
