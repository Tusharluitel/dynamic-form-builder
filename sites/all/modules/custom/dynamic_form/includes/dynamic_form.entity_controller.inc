<?php



/* ==================================================================
   FORM
   ================================================================== */

class DynamicFormFormController extends DrupalDefaultEntityController {

  protected function buildQuery($ids, $conditions = array(), $revision_id = FALSE) {
    $query = parent::buildQuery($ids, $conditions, $revision_id);
    $query->isNull('base.deleted_at');
    return $query;
  }

  public function loadSingle($id) {
    $entities = $this->load(array((int) $id));
    return $entities ? reset($entities) : NULL;
  }

  public function create(array $fields, $now) {
    $fields['created_at'] = $now;
    $fields['updated_at'] = $now;
    $entity = (object) $fields;
    drupal_write_record('dynamic_form_forms', $entity);
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
    drupal_write_record('dynamic_form_forms', $entity, array('id'));
    $this->resetCache(array((int) $id));
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

class DynamicFormSectionController extends DrupalDefaultEntityController {

  protected function buildQuery($ids, $conditions = array(), $revision_id = FALSE) {
    $query = parent::buildQuery($ids, $conditions, $revision_id);
    $query->isNull('base.deleted_at');
    return $query;
  }

  public function loadSingle($id) {
    $entities = $this->load(array((int) $id));
    return $entities ? reset($entities) : FALSE;
  }

  public function create(array $fields, $now) {
    $fields['created_at'] = $now;
    $fields['updated_at'] = $now;
    $entity = (object) $fields;
    drupal_write_record('dynamic_form_sections', $entity);
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
    drupal_write_record('dynamic_form_sections', $entity, array('id'));
    $this->resetCache(array((int) $id));
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

class DynamicFormQuestionController extends DrupalDefaultEntityController {

  protected function buildQuery($ids, $conditions = array(), $revision_id = FALSE) {
    $query = parent::buildQuery($ids, $conditions, $revision_id);
    $query->isNull('base.deleted_at');
    return $query;
  }

  public function loadSingle($id) {
    $entities = $this->load(array((int) $id));
    return $entities ? reset($entities) : FALSE;
  }

  public function create(array $fields, $now) {
    $fields['created_at'] = $now;
    $fields['updated_at'] = $now;
    $entity = (object) $fields;
    drupal_write_record('dynamic_form_questions', $entity);
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
    drupal_write_record('dynamic_form_questions', $entity, array('id'));
    $this->resetCache(array((int) $id));
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

class DynamicFormQuestionOptionController extends DrupalDefaultEntityController {

  protected function buildQuery($ids, $conditions = array(), $revision_id = FALSE) {
    $query = parent::buildQuery($ids, $conditions, $revision_id);
    $query->isNull('base.deleted_at');
    return $query;
  }

  public function loadSingle($id) {
    $entities = $this->load(array((int) $id));
    return $entities ? reset($entities) : FALSE;
  }

  public function create(array $fields, $now) {
    $fields['created_at'] = $now;
    $fields['updated_at'] = $now;
    $entity = (object) $fields;
    drupal_write_record('dynamic_form_question_options', $entity);
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
