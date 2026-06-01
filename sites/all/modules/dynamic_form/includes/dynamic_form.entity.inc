<?php

/**
 * EntityAPIController subclass for Dynamic Form entities.
 */

/**
 * RestWSEntityResourceController subclass for Dynamic Form entities.
 *
 */
class DynamicFormRestWSEntityResourceController extends RestWSEntityResourceController {

  /**
   * Adds deleted_at IS NULL to every restws list/count EntityFieldQuery.
   *
   * EntityFieldQuery::propertyCondition($col, NULL) → SelectQuery::isNull($col)
   * because EntityFieldQuery::addCondition() calls isNull() when value is NULL.
   */
  protected function checkAccess(EntityFieldQuery $query) {
    parent::checkAccess($query);
    $query->propertyCondition('deleted_at', NULL);
  }
}

/**
 * EntityAPIController that hides soft-deleted rows (deleted_at IS NOT NULL).
 *
 * Used for every Dynamic Form entity whose base table has a `deleted_at`
 * column. 
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
