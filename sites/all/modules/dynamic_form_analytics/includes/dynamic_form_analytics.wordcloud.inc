<?php

/**
 * @file
 * Word cloud AJAX endpoints and data helpers for Dynamic Form Analytics.
 *
 * Data source: dynamic_form_tag_answers + dynamic_form_tags + dynamic_form_responses.
 * MySQL is used directly (rather than Solr) because the dedicated tag tables
 * give exact per-tag counts in a single GROUP BY query; Solr faceting on
 * sm_q_* would require knowing which question IDs are "tags" type up front,
 * adding complexity without any accuracy benefit for this use-case.
 */

// ---------------------------------------------------------------------------
// AJAX callbacks
// ---------------------------------------------------------------------------

/**
 * AJAX callback: returns tag word-cloud frequency data as JSON.
 *
 * GET params:
 *   form_id     (int, optional) — filter to a single form.
 *   question_id (int, optional) — further filter to a single tag question.
 *
 * Response: {"words":[{"text":"quant","count":5}, ...]}
 */
function dynamic_form_analytics_wordcloud_data() {
  $form_id     = isset($_GET['form_id'])     ? (int) $_GET['form_id']     : 0;
  $question_id = isset($_GET['question_id']) ? (int) $_GET['question_id'] : 0;

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

/**
 * Query tag frequency counts from the dedicated tag-answer tables.
 *
 * @param int $form_id      0 = all forms.
 * @param int $question_id  0 = all tag questions.
 * @return array  [['text' => string, 'count' => int], ...]  sorted by count DESC.
 */
function _dynamic_form_analytics_wordcloud_query($form_id = 0, $question_id = 0) {
  $query = db_select('dynamic_form_tag_answers', 'ta');
  $query->join('dynamic_form_tags', 't', 't.id = ta.tag_id');
  $query->join('dynamic_form_responses', 'r', 'r.id = ta.response_id');
  $query->addField('t', 'standardized_value', 'text');
  $query->addExpression('COUNT(*)', 'count');
  // Include submitted responses regardless of soft-deletion: the word cloud
  // shows all-time tag usage, not just responses currently active in the system.
  $query->condition('r.status', 1);

  if ($form_id > 0) {
    $query->condition('r.form_id', $form_id);
  }
  if ($question_id > 0) {
    $query->condition('ta.question_id', $question_id);
  }

  $query->groupBy('ta.tag_id');
  $query->orderBy('count', 'DESC');
  $query->range(0, 150);

  $words = array();
  foreach ($query->execute()->fetchAll() as $row) {
    $words[] = array('text' => check_plain($row->text), 'count' => (int) $row->count);
  }
  return $words;
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
