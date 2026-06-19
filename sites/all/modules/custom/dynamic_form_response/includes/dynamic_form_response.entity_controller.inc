<?php

/**
 * @file
 * Data-access layer for dynamic_form_response and dynamic_form_answer entities.
 *
 * All entity loads/saves and the small number of db_* calls that cannot be
 * expressed through entity_load_single() (joins to the sessions table, single-
 * column updates) live here.  submit.inc and the module file call these static
 * methods instead of touching the database directly.
 */
class DynamicFormResponseEntityController {

  /* ==================================================================
     FORM
     ================================================================== */

  /**
   * Loads a dynamic_form_form entity by ID.
   *
   * Soft-deleted forms return FALSE (filtered by DynamicFormSoftDeleteEntityController).
   */
  static function loadForm($form_id) {
    return entity_load_single('dynamic_form_form', (int) $form_id);
  }

  /* ==================================================================
     RESPONSE — reads
     ================================================================== */

  /**
   * Loads a dynamic_form_response entity by ID.
   *
   * Soft-deleted responses return FALSE.
   */
  static function load($response_id) {
    return entity_load_single('dynamic_form_response', (int) $response_id);
  }

  /**
   * Returns TRUE if the authenticated user already has a submitted response
   * for this form.
   */
  static function hasSubmittedByUser($form_id, $user_id) {
    return (bool) db_select('dynamic_form_responses', 'r')
      ->condition('form_id', (int) $form_id)
      ->condition('user_id', (int) $user_id)
      ->condition('status', 1)
      ->isNull('deleted_at')
      ->countQuery()->execute()->fetchField();
  }

  /**
   * Returns TRUE if the anonymous guest (identified by $guest_token) already
   * has a submitted response for this form.
   */
  static function hasSubmittedByGuest($form_id, $guest_token) {
    $q = db_select('dynamic_form_responses', 'r');
    $q->join('dynamic_form_response_sessions', 's', 's.response_id = r.id');
    return (bool) $q
      ->condition('r.form_id', (int) $form_id)
      ->condition('r.status', 1)
      ->condition('s.guest_token', $guest_token)
      ->isNull('r.deleted_at')
      ->countQuery()->execute()->fetchField();
  }

  /**
   * Finds and returns the active draft entity for an authenticated user,
   * or NULL if none exists.
   */
  static function findDraftByUser($form_id, $user_id) {
    $id = db_select('dynamic_form_responses', 'r')
      ->fields('r', array('id'))
      ->condition('form_id', (int) $form_id)
      ->condition('user_id', (int) $user_id)
      ->condition('status', 0)
      ->isNull('deleted_at')
      ->execute()
      ->fetchField();
    return $id ? entity_load_single('dynamic_form_response', (int) $id) : NULL;
  }

  /**
   * Finds and returns the active draft entity for an anonymous guest,
   * or NULL if none exists.
   */
  static function findDraftByGuest($form_id, $guest_token) {
    $q = db_select('dynamic_form_responses', 'r');
    $q->join('dynamic_form_response_sessions', 's', 's.response_id = r.id');
    $id = $q
      ->fields('r', array('id'))
      ->condition('r.form_id', (int) $form_id)
      ->condition('r.status', 0)
      ->condition('s.guest_token', $guest_token)
      ->isNull('r.deleted_at')
      ->execute()
      ->fetchField();
    return $id ? entity_load_single('dynamic_form_response', (int) $id) : NULL;
  }

  /**
   * Returns TRUE if the given guest token has a session record for this response.
   */
  static function isGuestSessionOwner($response_id, $guest_token) {
    return (bool) db_select('dynamic_form_response_sessions', 's')
      ->condition('response_id', (int) $response_id)
      ->condition('guest_token', $guest_token)
      ->countQuery()->execute()->fetchField();
  }

  /* ==================================================================
     RESPONSE — writes
     ================================================================== */

  /**
   * Creates a new draft response entity and returns its ID.
   */
  static function createDraft($form_id, $user_id, $guest_email, $now) {
    $entity = (object) array(
      'form_id'     => (int) $form_id,
      'user_id'     => (int) $user_id,
      'status'      => 0,
      'guest_email' => $guest_email,
      'created_at'  => $now,
      'updated_at'  => $now,
    );
    entity_save('dynamic_form_response', $entity);
    return (int) $entity->id;
  }

  /**
   * Inserts a session/device context row for a newly-created draft.
   *
   * @param int         $response_id  Primary key of the draft response.
   * @param string|null $guest_token  NULL for authenticated users.
   * @param int         $now          REQUEST_TIME.
   */
  static function createSession($response_id, $guest_token, $now) {
    db_insert('dynamic_form_response_sessions')
      ->fields(array(
        'response_id' => (int) $response_id,
        'ip_address'  => ip_address(),
        'user_agent'  => isset($_SERVER['HTTP_USER_AGENT'])
          ? substr($_SERVER['HTTP_USER_AGENT'], 0, 512) : '',
        'session_id'  => hash('sha256', session_id()),
        'guest_token' => $guest_token,
        'created_at'  => $now,
      ))
      ->execute();
  }

  /**
   * Bumps the updated_at timestamp on an existing draft.
   *
   * A single-column update; loading the full entity for this alone would be wasteful.
   */
  static function touch($response_id, $now) {
    db_update('dynamic_form_responses')
      ->fields(array('updated_at' => $now))
      ->condition('id', (int) $response_id)
      ->execute();
  }

  /**
   * Persists the guest email on an existing draft when it first arrives.
   * The isNull guard prevents overwriting an email that was already stored.
   */
  static function setGuestEmail($response_id, $email) {
    db_update('dynamic_form_responses')
      ->fields(array('guest_email' => $email))
      ->condition('id', (int) $response_id)
      ->isNull('guest_email')
      ->execute();
  }

  /**
   * Marks a draft as submitted (status = 1) via entity_save so the entity
   * cache is invalidated automatically.
   *
   * @return bool  FALSE if the entity could not be loaded.
   */
  static function submit($response_id, $now) {
    $entity = entity_load_single('dynamic_form_response', (int) $response_id);
    if (!$entity) {
      return FALSE;
    }
    $entity->status       = 1;
    $entity->submitted_at = $now;
    $entity->updated_at   = $now;
    entity_save('dynamic_form_response', $entity);
    return TRUE;
  }

  /* ==================================================================
     ANSWER
     ================================================================== */

  /**
   * Returns a question_id => value map for all answers on a response.
   *
   * db_select with fetchAllKeyed is more efficient than loading full entities
   * when we only need the two columns.
   */
  static function getAnswersMap($response_id) {
    return db_select('dynamic_form_answers', 'a')
      ->fields('a', array('question_id', 'value'))
      ->condition('response_id', (int) $response_id)
      ->execute()
      ->fetchAllKeyed(0, 1);
  }

  /**
   * Inserts a new answer entity or updates an existing one.
   *
   * The composite key (response_id + question_id) is not the primary key,
   * so we still use db_select to find the existing row id, then delegate
   * the actual write to entity_save.
   *
   * @return int  Primary key of the inserted or updated answer row.
   */
  static function upsertAnswer($response_id, $question_id, $value, $now = NULL) {
    $now = $now ?: REQUEST_TIME;

    $existing_id = db_select('dynamic_form_answers', 'a')
      ->fields('a', array('id'))
      ->condition('response_id', (int) $response_id)
      ->condition('question_id', (int) $question_id)
      ->execute()
      ->fetchField();

    if ($existing_id) {
      $entity = entity_load_single('dynamic_form_answer', (int) $existing_id);
      $entity->value      = $value;
      $entity->updated_at = $now;
      entity_save('dynamic_form_answer', $entity);
      return (int) $existing_id;
    }

    $entity = (object) array(
      'response_id' => (int) $response_id,
      'question_id' => (int) $question_id,
      'value'       => $value,
      'created_at'  => $now,
      'updated_at'  => $now,
    );
    entity_save('dynamic_form_answer', $entity);
    return (int) $entity->id;
  }
}
