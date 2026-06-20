<?php

/**
 * @file
 * Entity controllers for the dynamic_form_response and dynamic_form_answer
 * entity types.
 *
 * Each controller extends DrupalDefaultEntityController, overrides buildQuery()
 * to exclude soft-deleted rows from every entity_load() call, and provides
 * write helpers that use drupal_write_record() / db_update() + resetCache().
 *
 * db_* is retained where entity API cannot reach:
 *   - JOINs to the non-entity dynamic_form_response_sessions table
 *     (hasSubmittedByGuest, findDraftByGuest, isGuestSessionOwner, createSession)
 *   - COUNT aggregates (hasSubmittedByUser, hasSubmittedByGuest)
 *   - Key-value mapping queries (getAnswersMap)
 *   - Single-column partial updates (touch, setGuestEmail)
 *   - upsert() existence check — intentionally finds soft-deleted rows so a
 *     re-submitted answer updates in place rather than creating a duplicate
 *
 * Retrieve a controller instance via entity_get_controller():
 *   entity_get_controller('dynamic_form_response')->loadSingle($id);
 *   entity_get_controller('dynamic_form_answer')->upsert($r, $q, $v, $now);
 */

/* ==================================================================
   RESPONSE
   ================================================================== */

class DynamicFormResponseController extends DrupalDefaultEntityController {

  protected function buildQuery($ids, $conditions = array(), $revision_id = FALSE) {
    $query = parent::buildQuery($ids, $conditions, $revision_id);
    $query->isNull('base.deleted_at');
    return $query;
  }

  public function loadSingle($id) {
    $entities = $this->load(array((int) $id));
    return $entities ? reset($entities) : FALSE;
  }

  // ---- reads ---------------------------------------------------------------

  /**
   * Returns TRUE if the authenticated user already has a submitted response
   * for this form.
   */
  public function hasSubmittedByUser($form_id, $user_id) {
    return (bool) db_select('dynamic_form_responses', 'r')
      ->condition('form_id', (int) $form_id)
      ->condition('user_id', (int) $user_id)
      ->condition('status', 1)
      ->isNull('deleted_at')
      ->countQuery()->execute()->fetchField();
  }

  /**
   * Returns TRUE if the anonymous guest already has a submitted response
   * for this form.
   */
  public function hasSubmittedByGuest($form_id, $guest_token) {
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
   * Finds the active draft for an authenticated user, or NULL if none.
   *
   * entity_load() goes through buildQuery(), which adds isNull('deleted_at').
   */
  public function findDraftByUser($form_id, $user_id) {
    $entities = entity_load('dynamic_form_response', FALSE, array(
      'form_id' => (int) $form_id,
      'user_id' => (int) $user_id,
      'status'  => 0,
    ));
    return $entities ? reset($entities) : NULL;
  }

  /**
   * Finds the active draft for an anonymous guest, or NULL if none.
   *
   * Requires a JOIN to the non-entity sessions table, so db_select is used
   * to get the ID before loading the entity.
   */
  public function findDraftByGuest($form_id, $guest_token) {
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
    return $id ? $this->loadSingle((int) $id) : NULL;
  }

  /**
   * Returns TRUE if the given guest token has a session record for this response.
   */
  public function isGuestSessionOwner($response_id, $guest_token) {
    return (bool) db_select('dynamic_form_response_sessions', 's')
      ->condition('response_id', (int) $response_id)
      ->condition('guest_token', $guest_token)
      ->countQuery()->execute()->fetchField();
  }

  // ---- writes --------------------------------------------------------------

  /**
   * Creates a new draft response entity and returns it.
   */
  public function createDraft($form_id, $user_id, $guest_email, $now) {
    $entity = (object) array(
      'form_id'     => (int) $form_id,
      'user_id'     => (int) $user_id,
      'status'      => 0,
      'guest_email' => $guest_email,
      'created_at'  => $now,
      'updated_at'  => $now,
    );
    drupal_write_record('dynamic_form_responses', $entity);
    return $this->loadSingle((int) $entity->id);
  }

  /**
   * Inserts a session/device context row for a newly-created draft.
   *
   * dynamic_form_response_sessions is not an entity type, so db_insert is used.
   */
  public function createSession($response_id, $guest_token, $now) {
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
   * Bumps updated_at on an existing draft.
   *
   * Single-column update; loading the full entity for this alone would be
   * wasteful.
   */
  public function touch($id, $now) {
    db_update('dynamic_form_responses')
      ->fields(array('updated_at' => $now))
      ->condition('id', (int) $id)
      ->execute();
  }

  /**
   * Persists the guest email on an existing draft when it first arrives.
   *
   * The isNull guard on the UPDATE is atomic and prevents overwriting an
   * email that was already stored.  A load+save approach would have a race
   * condition.
   */
  public function setGuestEmail($id, $email) {
    db_update('dynamic_form_responses')
      ->fields(array('guest_email' => $email))
      ->condition('id', (int) $id)
      ->isNull('guest_email')
      ->execute();
  }

  /**
   * Marks a draft as submitted and returns the updated entity, or FALSE.
   */
  public function submit($id, $now) {
    $entity = $this->loadSingle($id);
    if (!$entity) {
      return FALSE;
    }
    $entity->status       = 1;
    $entity->submitted_at = $now;
    $entity->updated_at   = $now;
    drupal_write_record('dynamic_form_responses', $entity, array('id'));
    $this->resetCache(array((int) $id));
    return $this->loadSingle($id);
  }

  /**
   * Soft-deletes a response.
   */
  public function softDelete($id, $uid, $now = NULL) {
    $now = $now ?: REQUEST_TIME;
    db_update('dynamic_form_responses')
      ->fields(array('deleted_at' => $now, 'deleted_by' => (int) $uid, 'updated_at' => $now))
      ->condition('id', (int) $id)
      ->execute();
    $this->resetCache(array((int) $id));
  }
}

/* ==================================================================
   ANSWER
   ================================================================== */

class DynamicFormAnswerController extends DrupalDefaultEntityController {

  protected function buildQuery($ids, $conditions = array(), $revision_id = FALSE) {
    $query = parent::buildQuery($ids, $conditions, $revision_id);
    $query->isNull('base.deleted_at');
    return $query;
  }

  public function loadSingle($id) {
    $entities = $this->load(array((int) $id));
    return $entities ? reset($entities) : FALSE;
  }

  /**
   * Returns a question_id => value map for all answers in a response.
   *
   * Uses db_select for the key-value result; entity_load would return full
   * objects requiring an extra loop with no benefit.
   */
  public function getAnswersMap($response_id) {
    return db_select('dynamic_form_answers', 'a')
      ->fields('a', array('question_id', 'value'))
      ->condition('response_id', (int) $response_id)
      ->execute()
      ->fetchAllKeyed(0, 1);
  }

  /**
   * Inserts or updates one answer for a given response + question pair.
   *
   * The existence check uses db_select without an isNull('deleted_at') filter
   * — intentionally — so that a re-submitted answer revives a soft-deleted row
   * in place rather than creating a duplicate.
   *
   * Returns the answer entity ID (int).
   */
  public function upsert($response_id, $question_id, $value, $now = NULL) {
    $now = $now ?: REQUEST_TIME;

    $existing_id = db_select('dynamic_form_answers', 'a')
      ->fields('a', array('id'))
      ->condition('response_id', (int) $response_id)
      ->condition('question_id', (int) $question_id)
      ->execute()
      ->fetchField();

    if ($existing_id) {
      db_update('dynamic_form_answers')
        ->fields(array('value' => $value, 'updated_at' => $now, 'deleted_at' => NULL))
        ->condition('id', (int) $existing_id)
        ->execute();
      $this->resetCache(array((int) $existing_id));
      return (int) $existing_id;
    }

    $entity = (object) array(
      'response_id' => (int) $response_id,
      'question_id' => (int) $question_id,
      'value'       => $value,
      'created_at'  => $now,
      'updated_at'  => $now,
    );
    drupal_write_record('dynamic_form_answers', $entity);
    return (int) $entity->id;
  }
}
