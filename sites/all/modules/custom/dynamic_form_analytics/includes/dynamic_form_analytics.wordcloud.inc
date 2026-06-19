<?php

/**
 * @file
 * Word cloud AJAX endpoints and data helpers for Dynamic Form Analytics.
 */

// ---------------------------------------------------------------------------
// AJAX callbacks
// ---------------------------------------------------------------------------

/**
 * AJAX callback: returns tag word-cloud frequency data as JSON.
 */
function dynamic_form_analytics_wordcloud_data() {
  // 60 word-cloud queries per hour per IP (heavy DB aggregation).
  $ip = ip_address();
  if (!flood_is_allowed('dfa_wordcloud', 60, 3600, $ip)) {
    drupal_add_http_header('Status', '429 Too Many Requests');
    drupal_json_output(array('error' => t('Too many requests.')));
    drupal_exit();
  }
  flood_register_event('dfa_wordcloud', 3600, $ip);

  $form_id     = isset($_GET['form_id'])     ? (int) $_GET['form_id']     : 0;
  $question_id = isset($_GET['question_id']) ? (int) $_GET['question_id'] : 0;

  // When scoped to a specific form, verify the caller can edit that form.
  if ($form_id && !_dynamic_form_edit_access($form_id)) {
    drupal_json_output(array('error' => t('Access denied.')));
    drupal_exit();
  }

  $words = _dynamic_form_analytics_wordcloud_query($form_id, $question_id);
  drupal_json_output(array('words' => $words));
  drupal_exit();
}

/**
 * AJAX callback: returns tag-type questions for a given form.
 *
 * Path: dashboard/analytics/wordcloud-questions/{form_id}
 * Response: {"questions":[{"id":26,"label":"Skills"}, ...]}
 */
function dynamic_form_analytics_wordcloud_questions($form_id) {
  $form_id = (int) $form_id;

  if (!_dynamic_form_edit_access($form_id)) {
    drupal_json_output(array('error' => t('Access denied.')));
    drupal_exit();
  }

  $rows = db_select('dynamic_form_questions', 'q')
    ->fields('q', array('id', 'label'))
    ->condition('q.form_id', $form_id)
    ->condition('q.type', 'tags')
    ->isNull('q.deleted_at')
    ->orderBy('q.section_id')
    ->orderBy('q.position')
    ->execute()
    ->fetchAll();

  $questions = array();
  foreach ($rows as $row) {
    $questions[] = array('id' => (int) $row->id, 'label' => check_plain($row->label));
  }

  drupal_json_output(array('questions' => $questions));
  drupal_exit();
}

// ---------------------------------------------------------------------------
// Data helpers (called from page callbacks)
// ---------------------------------------------------------------------------
function _dynamic_form_analytics_wordcloud_query($form_id = 0, $question_id = 0) {
  $query = db_select('dynamic_form_tag_answers', 'ta');
  $query->join('dynamic_form_tags', 't', 't.id = ta.tag_id');
  $query->join('dynamic_form_responses', 'r', 'r.id = ta.response_id');
  $query->addField('t', 'standardized_value', 'text');
  $query->addExpression('COUNT(*)', 'count');
  $query->condition('r.status', 1);

  if ($form_id > 0) {
    $query->condition('r.form_id', $form_id);
    $query->addExpression('MIN(ta.question_id)', 'question_id');
  }
  if ($question_id > 0) {
    $query->condition('ta.question_id', $question_id);
  }

  $query->groupBy('ta.tag_id');
  $query->orderBy('count', 'DESC');
  $query->range(0, 150);

  $words = array();
  foreach ($query->execute()->fetchAll() as $row) {
    $word = array('text' => check_plain($row->text), 'count' => (int) $row->count);
    if ($form_id > 0) {
      $word['question_id'] = (int) $row->question_id;
    }
    $words[] = $word;
  }
  return $words;
}

/**
 * AJAX callback: returns per-form usage counts for a single tag word.
 *
 * GET params:
 *   tag (string, required) — the tag's standardized value.
 *
 * Response: {"tag":"python","forms":[{"form_id":3,"title":"Job App","count":7,"question_id":12},...]}
 */
function dynamic_form_analytics_wordcloud_tag_forms() {
  $tag = isset($_GET['tag']) ? trim($_GET['tag']) : '';
  if ($tag === '') {
    drupal_json_output(array('tag' => '', 'forms' => array()));
    drupal_exit();
  }

  $forms = _dynamic_form_analytics_wordcloud_per_form_query($tag);
  drupal_json_output(array('tag' => check_plain($tag), 'forms' => $forms));
  drupal_exit();
}

/**
 * Returns per-form breakdown for a given tag text, with all matching questions.
 */
function _dynamic_form_analytics_wordcloud_per_form_query($tag_text) {
  $rows = db_query(
    'SELECT r.form_id, f.title, ta.question_id, q.label AS question_label, COUNT(*) AS count
     FROM {dynamic_form_tag_answers} ta
     JOIN {dynamic_form_tags} t      ON t.id  = ta.tag_id
     JOIN {dynamic_form_responses} r ON r.id  = ta.response_id AND r.status = 1 AND r.deleted_at IS NULL
     JOIN {dynamic_form_forms} f     ON f.id  = r.form_id      AND f.deleted_at IS NULL
     LEFT JOIN {dynamic_form_questions} q ON q.id = ta.question_id AND q.deleted_at IS NULL
     WHERE t.standardized_value = :tag
     GROUP BY r.form_id, ta.question_id, f.title, q.label
     ORDER BY r.form_id ASC, count DESC',
    array(':tag' => $tag_text)
  )->fetchAll();

  // Aggregate rows into one entry per form with a questions sub-array.
  $form_map = array();
  foreach ($rows as $row) {
    $fid = (int) $row->form_id;
    if (!isset($form_map[$fid])) {
      $form_map[$fid] = array(
        'form_id'   => $fid,
        'title'     => check_plain($row->title),
        'count'     => 0,
        'questions' => array(),
      );
    }
    $form_map[$fid]['count'] += (int) $row->count;
    if ($row->question_label !== NULL) {
      $form_map[$fid]['questions'][] = array(
        'id'    => (int) $row->question_id,
        'label' => check_plain($row->question_label),
      );
    }
  }

  // Sort forms by total count DESC; break ties by form_id ASC for stability.
  $forms = array_values($form_map);
  usort($forms, function ($a, $b) {
    if ($b['count'] !== $a['count']) {
      return $b['count'] - $a['count'];
    }
    return $a['form_id'] - $b['form_id'];
  });
  return $forms;
}

/**
 * Returns forms that have at least one submitted tag answer (for the dropdown).
 *
 * @return array  [['id' => int, 'title' => string], ...]
 */
function _dynamic_form_analytics_wordcloud_forms() {
  $rows = db_query(
    'SELECT DISTINCT f.id, f.title
     FROM {dynamic_form_forms} f
     JOIN {dynamic_form_responses} r ON r.form_id = f.id
     JOIN {dynamic_form_tag_answers} ta ON ta.response_id = r.id
     WHERE f.deleted_at IS NULL AND r.status = 1 AND r.deleted_at IS NULL
     ORDER BY f.title ASC'
  )->fetchAll();

  $forms = array();
  foreach ($rows as $row) {
    $forms[] = array('id' => (int) $row->id, 'title' => check_plain($row->title));
  }
  return $forms;
}

/**
 * Returns tag-type questions for a form (for the question filter dropdown).
 *
 * @param int $form_id
 * @return array  [['id' => int, 'label' => string], ...]
 */
function _dynamic_form_analytics_wordcloud_tag_questions($form_id) {
  $rows = db_select('dynamic_form_questions', 'q')
    ->fields('q', array('id', 'label'))
    ->condition('q.form_id', $form_id)
    ->condition('q.type', 'tags')
    ->isNull('q.deleted_at')
    ->orderBy('q.section_id')
    ->orderBy('q.position')
    ->execute()
    ->fetchAll();

  $questions = array();
  foreach ($rows as $row) {
    $questions[] = array('id' => (int) $row->id, 'label' => check_plain($row->label));
  }
  return $questions;
}
