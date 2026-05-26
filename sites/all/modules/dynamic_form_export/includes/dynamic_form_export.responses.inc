<?php

/**
 * Builds and registers the responses batch for one or more forms.
 *
 * Each selected form gets its own sheet in the output XLSX.
 * Column layout (mirrors Google Forms export):
 *   Timestamp | Email | Respondent | Q1 | Q2 | … (ordered by section then
 *   question position)
 */
function _dynamic_form_export_responses_start_batch($form_ids, $from = '', $to = '') {
  $form_ids   = array_values(array_filter(array_map('intval', (array) $form_ids)));
  $operations = array();
  $inc_file   = drupal_get_path('module', 'dynamic_form_export') . '/includes/dynamic_form_export.responses.inc';
  $batch_size = 200;

  foreach ($form_ids as $index => $form_id) {
    $form_title = db_query('SELECT title FROM {dynamic_form_forms} WHERE id = :id', array(':id' => $form_id))->fetchField();
    $questions  = _dynamic_form_export_responses_questions($form_id);

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

    $operations[] = array(
      '_dynamic_form_export_responses_init',
      array($index, $total, $form_id, $form_title ?: ('Form ' . $form_id), $questions, $from, $to),
    );

    for ($offset = 0; $offset < $total; $offset += $batch_size) {
      $operations[] = array(
        '_dynamic_form_export_responses_process',
        array($index, $offset, $batch_size, $total, $form_id, $questions, $from, $to),
      );
    }
  }

  $batch_label = count($form_ids) === 1
    ? db_query('SELECT title FROM {dynamic_form_forms} WHERE id = :id', array(':id' => $form_ids[0]))->fetchField()
    : format_plural(count($form_ids), '1 form', '@count forms');

  batch_set(array(
    'title'            => t('Exporting Responses — @label', array('@label' => $batch_label)),
    'operations'       => $operations,
    'finished'         => '_dynamic_form_export_responses_finished',
    'init_message'     => t('Preparing export…'),
    'progress_message' => t('Processing batch @current of @total…'),
    'error_message'    => t('The export encountered an error. Please try again.'),
    'file'             => $inc_file,
  ));

  $_SESSION['dynamic_form_export_filename'] = 'responses-export-' . date('Y-m-d') . '.xlsx';
}

/**
 * Returns an ordered array of question objects for the given form.
 *
 * Each object has: id, label, type, section_title, section_pos, question_pos.
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

  $section_titles = array_unique(array_map(function ($q) { return $q->section_title; }, $questions));
  $multi_section  = count($section_titles) > 1;

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
 * Batch op — init: create temp JSONL file and write the header row.
 *
 * @param int    $form_index  Position in the ordered form list (0-based).
 * @param int    $total       Number of responses for this form.
 * @param int    $form_id     Form ID.
 * @param string $form_title  Human-readable form title.
 * @param array  $questions   From _dynamic_form_export_responses_questions().
 */
function _dynamic_form_export_responses_init($form_index, $total, $form_id, $form_title, $questions, $from, $to, &$context) {
  $temp_file = _dynamic_form_export_temp_dir()
    . '/dfexport_resp_' . $form_id . '_' . REQUEST_TIME . '_' . mt_rand(1000, 9999) . '.jsonl';

  $header = array('Timestamp', 'Email', 'Respondent');
  foreach ($questions as $q) {
    $header[] = $q->header;
  }

  file_put_contents($temp_file, json_encode($header) . "\n");

  if (!isset($context['results']['forms'])) {
    $context['results']['forms'] = array();
  }

  $context['results']['forms'][$form_index] = array(
    'temp_file'    => $temp_file,
    'form_id'      => $form_id,
    'form_title'   => $form_title,
    'rows_written' => 0,
    'total'        => $total,
  );

  $context['message'] = t('Initializing export for "@title"…', array('@title' => $form_title));
}

/**
 * Batch op — process: query a chunk of responses and append rows to JSONL.
 *
 * @param int   $form_index  Matches the index used in the init op.
 * @param int   $offset      Row offset for this chunk.
 */
function _dynamic_form_export_responses_process($form_index, $offset, $batch_size, $total, $form_id, $questions, $from, $to, &$context) {
  if (empty($context['results']['forms'][$form_index]['temp_file'])) {
    return;
  }

  $temp_file = $context['results']['forms'][$form_index]['temp_file'];

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

  $response_ids = array_map(function ($r) { return (int) $r->id; }, $responses);

  $answer_rows = db_select('dynamic_form_answers', 'a')
    ->fields('a', array('response_id', 'question_id', 'value'))
    ->condition('a.response_id', $response_ids, 'IN')
    ->execute()
    ->fetchAll();

  $answers = array();
  foreach ($answer_rows as $ar) {
    $answers[(int) $ar->response_id][(int) $ar->question_id] = $ar->value;
  }

  $question_ids = array_map(function ($q) { return (int) $q->id; }, $questions);

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
    $context['results']['forms'][$form_index]['rows_written']++;
  }

  file_put_contents($temp_file, $lines, FILE_APPEND | LOCK_EX);

  $done = $offset + count($responses);
  $context['message'] = t('Exported @done of @total responses for "@title"…', array(
    '@done'  => number_format($done),
    '@total' => number_format($total),
    '@title' => $context['results']['forms'][$form_index]['form_title'],
  ));
}

/**
 * Batch finished: convert per-form JSONL files → a multi-sheet XLSX.
 */
function _dynamic_form_export_responses_finished($success, $results, $operations) {
  if (!$success || empty($results['forms'])) {
    drupal_set_message(t('Response export failed or produced no output. Please try again.'), 'error');
    drupal_goto('dashboard/export');
  }

  require_once DRUPAL_ROOT . '/sites/all/libraries/openspout/autoload.php';

  $xlsx_path = _dynamic_form_export_temp_dir()
    . '/dfexport_resp_' . REQUEST_TIME . '_' . mt_rand(1000, 9999) . '.xlsx';

  $header_style = (new \OpenSpout\Common\Entity\Style\Style())->setFontBold();

  $options = new \OpenSpout\Writer\XLSX\Options();
  $options->setTempFolder(_dynamic_form_export_temp_dir());

  $writer = new \OpenSpout\Writer\XLSX\Writer($options);
  $writer->openToFile($xlsx_path);

  $total_rows  = 0;
  $first_sheet = TRUE;
  $used_names  = array();

  // Process forms in their original order.
  ksort($results['forms']);

  foreach ($results['forms'] as $form_info) {
    $sheet_name = _dynamic_form_export_safe_sheet_name($form_info['form_title'], $used_names);

    if ($first_sheet) {
      $writer->getCurrentSheet()->setName($sheet_name);
      $first_sheet = FALSE;
    }
    else {
      $writer->addNewSheetAndMakeItCurrent()->setName($sheet_name);
    }

    $temp_file = $form_info['temp_file'];
    if (!file_exists($temp_file)) {
      continue;
    }

    $handle      = fopen($temp_file, 'r');
    $is_first_row = TRUE;
    while (($line = fgets($handle)) !== FALSE) {
      $line = trim($line);
      if ($line === '') {
        continue;
      }
      $cells = json_decode($line, TRUE);
      if (!is_array($cells)) {
        continue;
      }
      $writer->addRow(\OpenSpout\Common\Entity\Row::fromValues($cells, $is_first_row ? $header_style : NULL));
      $is_first_row = FALSE;
    }
    fclose($handle);
    @unlink($temp_file);

    $total_rows += $form_info['rows_written'];
  }

  $writer->close();

  if (!file_exists($xlsx_path)) {
    drupal_set_message(t('Failed to generate the XLSX file. Please try again.'), 'error');
    drupal_goto('dashboard/export');
  }

  $filename = isset($_SESSION['dynamic_form_export_filename'])
    ? $_SESSION['dynamic_form_export_filename']
    : 'responses-export-' . date('Y-m-d') . '.xlsx';

  unset($_SESSION['dynamic_form_export_filename']);

  $_SESSION['dynamic_form_export'] = array(
    'file'     => $xlsx_path,
    'filename' => $filename,
    'rows'     => $total_rows,
  );

  drupal_set_message(t('Responses exported successfully across @sheets sheet(s).', array(
    '@sheets' => number_format(count($results['forms'])),
  )));
}

// ── Helpers ───────────────────────────────────────────────────────────────────

/**
 * Returns a sanitised Excel sheet name (max 31 chars, no \ / * ? : [ ]).
 *
 * Appends a numeric suffix when $used_names already contains the candidate so
 * every sheet in the workbook has a unique name.
 *
 * @param string $title       Raw form title.
 * @param array  &$used_names Running list of names already assigned in this
 *                            workbook; updated in-place.
 */
function _dynamic_form_export_safe_sheet_name($title, &$used_names) {
  $name = preg_replace('/[\\\\\/\*\?\:\[\]]/', ' ', $title);
  $name = trim($name);
  if ($name === '') {
    $name = 'Sheet';
  }
  if (strlen($name) > 31) {
    $name = substr($name, 0, 31);
  }

  $base    = $name;
  $counter = 2;
  while (in_array($name, $used_names)) {
    $suffix = ' (' . $counter++ . ')';
    $name   = substr($base, 0, 31 - strlen($suffix)) . $suffix;
  }

  $used_names[] = $name;
  return $name;
}

/**
 * Formats a raw answer value for the spreadsheet based on question type.
 *
 *  - checkbox / multi-select  → comma-separated string
 *  - file                     → comma-separated filenames
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
