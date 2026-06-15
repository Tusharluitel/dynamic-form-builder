<?php

/**
 * EntityAPIController subclass for Dynamic Form entities.
 */

/**
 * RestWSEntityResourceController subclass for Dynamic Form entities.
 *
 */
class DynamicFormRestWSEntityResourceController extends RestWSEntityResourceController {

  
  protected function checkAccess(EntityFieldQuery $query) {
    parent::checkAccess($query);
    $query->propertyCondition('deleted_at', NULL);
  }
}

/**
 * EntityAPIController that hides soft-deleted rows (deleted_at IS NOT NULL).
 */
class DynamicFormSoftDeleteEntityController extends EntityAPIController {

  /**
   * Adds deleted_at IS NULL to the SELECT query before it executes.
   */
  protected function buildQuery($ids, $conditions = array(), $revision_id = FALSE) {
    $query = parent::buildQuery($ids, $conditions, $revision_id);
    $query->isNull('base.deleted_at');
    return $query;
  }
}
