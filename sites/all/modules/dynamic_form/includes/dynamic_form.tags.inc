<?php

/**
 * @file
 * Tags question type — autocomplete endpoint and save helper.
 */

/**
 * AJAX endpoint: returns matching tags for Select2 autocomplete.
 *
 * Expects GET param 'term'. Returns JSON array of {id, text} objects
 * where id == text == standardized_value (Select2 format).
 *
 * URL: dynamic-form/ajax/tags/autocomplete?term=php
 */
function dynamic_form_ajax_tags_autocomplete() {
  $term = isset($_GET['term']) ? trim($_GET['term']) : '';

  $results = array();

  if (drupal_strlen($term) >= 1) {
    $query = db_select('dynamic_form_tags', 't')
      ->fields('t', array('standardized_value'))
      ->condition('standardized_value', db_like($term) . '%', 'LIKE')
      ->orderBy('standardized_value', 'ASC')
      ->range(0, 20)
      ->execute();

    foreach ($query as $row) {
      $results[] = array(
        'id'   => $row->standardized_value,
        'text' => $row->standardized_value,
      );
    }
  }

  drupal_json_output($results);
  drupal_exit();
}

/**
 * Saves tag answers for a tags-type question within a form response.
 *
 * For each raw tag value:
 *   1. Normalize via _dynamic_form_normalize_tag().
 *   2. Upsert into dynamic_form_tags (get or create the canonical row).
 *   3. Insert into dynamic_form_tag_answers with the raw value preserved.
 *
 * Also writes a comma-joined string of normalized values into
 * dynamic_form_answers.value for display fallback on the responses page.
 *
 * @param int   $response_id  FK to dynamic_form_responses.
 * @param int   $question_id  FK to dynamic_form_questions.
 * @param array $raw_tags     Raw strings submitted by the respondent.
 *
 * @return array  Normalized tag values that were saved (empty strings skipped).
 */
function _dynamic_form_save_tag_answers($response_id, $question_id, array $raw_tags) {
  global $user;
  $now = REQUEST_TIME;

  // Remove any existing tag answers for this question + response (draft re-saves).
  db_delete('dynamic_form_tag_answers')
    ->condition('question_id', (int) $question_id)
    ->condition('response_id', (int) $response_id)
    ->execute();

  $normalized = array();

  foreach ($raw_tags as $raw) {
    $raw = trim($raw);
    if ($raw === '') {
      continue;
    }

    $standardized = _dynamic_form_normalize_tag($raw);
    if ($standardized === FALSE) {
      continue;
    }

    // Get existing tag id or insert a new canonical tag.
    $tag_id = db_select('dynamic_form_tags', 't')
      ->fields('t', array('id'))
      ->condition('standardized_value', $standardized)
      ->execute()
      ->fetchField();

    if (!$tag_id) {
      $tag_id = db_insert('dynamic_form_tags')
        ->fields(array(
          'standardized_value' => $standardized,
          'created_at'         => $now,
        ))
        ->execute();
    }

    // Skip duplicate tags within the same submission.
    if (in_array($standardized, $normalized)) {
      continue;
    }

    db_insert('dynamic_form_tag_answers')
      ->fields(array(
        'question_id' => (int) $question_id,
        'response_id' => (int) $response_id,
        'tag_id'      => (int) $tag_id,
        'raw_value'   => $raw,
        'created_at'  => $now,
      ))
      ->execute();

    $normalized[] = $standardized;
  }

  // Write comma-joined normalized values into dynamic_form_answers for display.
  $display_value = implode(', ', $normalized);
  db_merge('dynamic_form_answers')
    ->key(array('response_id' => (int) $response_id, 'question_id' => (int) $question_id))
    ->fields(array(
      'response_id' => (int) $response_id,
      'question_id' => (int) $question_id,
      'value'       => $display_value,
      'created_at'  => $now,
      'updated_at'  => $now,
    ))
    ->execute();

  return $normalized;
}
