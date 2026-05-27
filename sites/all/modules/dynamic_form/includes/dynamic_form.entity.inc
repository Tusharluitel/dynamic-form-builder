<?php

/**
 * @file
 * EntityAPIController subclass for Dynamic Form entities.
 *
 * Loaded via files[] in dynamic_form.info so the class is autoloaded by
 * Drupal's registry. Access callbacks live in dynamic_form.module so they
 * are always present at request time (files[] does not autoload functions).
 */

/**
 * RestWSEntityResourceController subclass for Dynamic Form entities.
 *
 * Overrides checkAccess() so that EntityFieldQuery-based list/count calls
 * (used by restws for GET /dynamic_form_form.json etc.) automatically exclude
 * soft-deleted rows. Without this, deleted entities appear as [] ghost slots
 * in the list output because restws fetches IDs via EFQ (no deleted_at filter)
 * and then entity_load() silently drops them via buildQuery().
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
 * column. The dashboard's Trash page loads soft-deleted rows directly via
 * db_select and does not go through entity_load(), so this filter does not
 * affect restore/permanent-delete flows.
 */
class DynamicFormSoftDeleteEntityController extends EntityAPIController {

  /**
   * Adds deleted_at IS NULL to the SELECT query before it executes.
   *
   * Override buildQuery() (which returns a SelectQueryInterface) rather than
   * query() (which returns the already-executed DatabaseStatementBase).
   */
  protected function buildQuery($ids, $conditions = array(), $revision_id = FALSE) {
    $query = parent::buildQuery($ids, $conditions, $revision_id);
    $query->isNull('base.deleted_at');
    return $query;
  }
}
