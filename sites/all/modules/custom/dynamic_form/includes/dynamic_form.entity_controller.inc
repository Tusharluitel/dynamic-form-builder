<?php

/**
 * @file
 * Entity controllers for the four core Dynamic Form entity types.
 *
 * Each controller extends EntityAPIController (contrib entity module), overrides
 * buildQuery() to exclude soft-deleted rows from every entity_load() call,
 * and provides create(), update(), softDelete(), and type-specific helpers.
 *
 * create() and update() use entity_save() so entity hooks (insert/update) fire
 * correctly and the static cache is reset automatically by EntityAPIController.
 *
 * softDelete() keeps db_update + resetCache() because EntityAPIController::delete()
 * performs a hard delete, which is not what we want.
 *
 * Retrieve a controller instance via entity_get_controller():
 *   entity_get_controller('dynamic_form_form')->loadSingle($id);
 *   entity_get_controller('dynamic_form_question')->insert($fields, $now);
 */

/* ==================================================================
   FORM
   ================================================================== */

class DynamicFormFormController extends EntityAPIController {

  protected function buildQuery($ids, $conditions = array(), $revision_id = FALSE) {
    $query = parent::buildQuery($ids, $conditions, $revision_id);
    $query->isNull('base.deleted_at');
    return $query;
  }

  public function loadSingle($id) {
    $entities = $this->load(array((int) $id));
    return $entities ? reset($entities) : FALSE;
  }

  public function insert(array $fields, $now) {
    $fields['created_at'] = $now;
    $fields['updated_at'] = $now;
    $entity = (object) $fields;
    entity_save('dynamic_form_form', $entity);
    return $this->loadSingle((int) $entity->id);
  }

  public function update($id, array $fields, $now) {
    $entity = $this->loadSingle($id);
    if (!$entity) {
      return FALSE;
    }
    $fields['updated_at'] = $now;
    foreach ($fields as $key => $value) {
      $entity->$key = $value;
    }
    entity_save('dynamic_form_form', $entity);
    return $this->loadSingle($id);
  }

  public function softDelete($id, $uid, $now) {
    db_update('dynamic_form_forms')
      ->fields(array('deleted_at' => $now, 'deleted_by' => (int) $uid, 'updated_at' => $now))
      ->condition('id', (int) $id)
      ->execute();
    $this->resetCache(array((int) $id));
  }
}

/* ==================================================================
   SECTION
   ================================================================== */

class DynamicFormSectionController extends EntityAPIController {

  protected function buildQuery($ids, $conditions = array(), $revision_id = FALSE) {
    $query = parent::buildQuery($ids, $conditions, $revision_id);
    $query->isNull('base.deleted_at');
    return $query;
  }

  public function loadSingle($id) {
    $entities = $this->load(array((int) $id));
    return $entities ? reset($entities) : FALSE;
  }

  public function insert(array $fields, $now) {
    $fields['created_at'] = $now;
    $fields['updated_at'] = $now;
    $entity = (object) $fields;
    entity_save('dynamic_form_section', $entity);
    return $this->loadSingle((int) $entity->id);
  }

  public function update($id, array $fields, $now) {
    $entity = $this->loadSingle($id);
    if (!$entity) {
      return FALSE;
    }
    $fields['updated_at'] = $now;
    foreach ($fields as $key => $value) {
      $entity->$key = $value;
    }
    entity_save('dynamic_form_section', $entity);
    return $this->loadSingle($id);
  }

  public function rename($id, $name, $now) {
    return $this->update($id, array('name' => $name), $now);
  }

  public function softDelete($id, $uid, $now) {
    db_update('dynamic_form_sections')
      ->fields(array('deleted_at' => $now, 'deleted_by' => (int) $uid, 'updated_at' => $now))
      ->condition('id', (int) $id)
      ->execute();
    $this->resetCache(array((int) $id));
  }

  /**
   * Returns all non-deleted sections for a form, ordered by position.
   *
   * Uses db_select because entity_load() does not support ORDER BY.
   */
  public function getSectionsForForm($form_id) {
    return db_select('dynamic_form_sections', 's')
      ->fields('s')
      ->condition('form_id', (int) $form_id)
      ->isNull('deleted_at')
      ->orderBy('position', 'ASC')
      ->execute()
      ->fetchAll();
  }
}

/* ==================================================================
   QUESTION
   ================================================================== */

class DynamicFormQuestionController extends EntityAPIController {

  protected function buildQuery($ids, $conditions = array(), $revision_id = FALSE) {
    $query = parent::buildQuery($ids, $conditions, $revision_id);
    $query->isNull('base.deleted_at');
    return $query;
  }

  public function loadSingle($id) {
    $entities = $this->load(array((int) $id));
    return $entities ? reset($entities) : FALSE;
  }

  public function insert(array $fields, $now) {
    $fields['created_at'] = $now;
    $fields['updated_at'] = $now;
    $entity = (object) $fields;
    entity_save('dynamic_form_question', $entity);
    dynamic_form_invalidate_questions_cache((int) $fields['form_id']);
    return $this->loadSingle((int) $entity->id);
  }

  public function update($id, array $fields, $now) {
    $entity = $this->loadSingle($id);
    if (!$entity) {
      return FALSE;
    }
    $form_id = (int) $entity->form_id;
    $fields['updated_at'] = $now;
    foreach ($fields as $key => $value) {
      $entity->$key = $value;
    }
    entity_save('dynamic_form_question', $entity);
    dynamic_form_invalidate_questions_cache($form_id);
    return $this->loadSingle($id);
  }

  public function softDelete($id, $uid, $now) {
    $entity = $this->loadSingle($id);
    db_update('dynamic_form_questions')
      ->fields(array('deleted_at' => $now, 'deleted_by' => (int) $uid, 'updated_at' => $now))
      ->condition('id', (int) $id)
      ->execute();
    $this->resetCache(array((int) $id));
    if ($entity) {
      dynamic_form_invalidate_questions_cache((int) $entity->form_id);
    }
  }
}

/* ==================================================================
   QUESTION OPTION
   ================================================================== */

class DynamicFormQuestionOptionController extends EntityAPIController {

  protected function buildQuery($ids, $conditions = array(), $revision_id = FALSE) {
    $query = parent::buildQuery($ids, $conditions, $revision_id);
    $query->isNull('base.deleted_at');
    return $query;
  }

  public function loadSingle($id) {
    $entities = $this->load(array((int) $id));
    return $entities ? reset($entities) : FALSE;
  }

  public function insert(array $fields, $now) {
    $fields['created_at'] = $now;
    $fields['updated_at'] = $now;
    $entity = (object) $fields;
    entity_save('dynamic_form_question_option', $entity);
    return $this->loadSingle((int) $entity->id);
  }

  /**
   * Soft-deletes all non-deleted options belonging to a question.
   *
   * Uses db_update for the bulk operation, then resets only the
   * affected entity IDs from the static cache.
   */
  public function softDeleteForQuestion($question_id, $uid, $now) {
    $question_id = (int) $question_id;
    $ids = db_select('dynamic_form_question_options', 'o')
      ->fields('o', array('id'))
      ->condition('question_id', $question_id)
      ->isNull('deleted_at')
      ->execute()
      ->fetchCol();

    if (!$ids) {
      return;
    }

    db_update('dynamic_form_question_options')
      ->fields(array('deleted_at' => $now, 'deleted_by' => (int) $uid))
      ->condition('question_id', $question_id)
      ->isNull('deleted_at')
      ->execute();

    $this->resetCache($ids);
  }
}
