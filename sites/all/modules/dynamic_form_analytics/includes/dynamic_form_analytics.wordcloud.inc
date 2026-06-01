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
 */
function dynamic_form_analytics_wordcloud_data() {
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

/**
 * Query tag frequency counts from the dedicated tag-answer tables.
 *
 * When $form_id > 0, also returns question_id (first tag question for that tag
 * in the form) so the JS can build a pre-filtered responses URL on click.
 *
 * @param int $form_id      0 = all forms.
 * @param int $question_id  0 = all tag questions.
 * @return array  [['text' => string, 'count' => int, ('question_id' => int)], ...]
 */
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
 *
 * Groups by (form_id, question_id) so every tag question in a form is returned,
 * then aggregates into one entry per form with a nested questions array.
 *
 * @param string $tag_text  Exact standardized_value to look up.
 * @return array  [['form_id'=>int,'title'=>string,'count'=>int,'questions'=>[['id'=>int,'label'=>string],...]], ...]
 */
function _dynamic_form_analytics_wordcloud_per_form_query($tag_text) {
  $query = db_select('dynamic_form_tag_answers', 'ta');
  $query->join('dynamic_form_tags',      't', 't.id = ta.tag_id');
  $query->join('dynamic_form_responses', 'r', 'r.id = ta.response_id');
  $query->join('dynamic_form_forms',     'f', 'f.id = r.form_id');
  $query->leftJoin('dynamic_form_questions', 'q', 'q.id = ta.question_id');
  $query->addField('r', 'form_id');
  $query->addField('f', 'title');
  $query->addField('ta', 'question_id');
  $query->addField('q', 'label', 'question_label');
  $query->addExpression('COUNT(*)', 'count');
  $query->condition('t.standardized_value', $tag_text);
  $query->condition('r.status', 1);
  $query->isNull('r.deleted_at');
  $query->isNull('f.deleted_at');
  $query->isNull('q.deleted_at');
  $query->groupBy('r.form_id');
  $query->groupBy('ta.question_id');
  $query->groupBy('f.title');
  $query->groupBy('q.label');
  $query->orderBy('r.form_id');
  $query->orderBy('count', 'DESC');

  // Aggregate rows into one entry per form with a questions sub-array.
  $form_map = array();
  foreach ($query->execute()->fetchAll() as $row) {
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
