<?php

class DynamicFormRestWSEntityResourceController extends RestWSEntityResourceController {

  protected function checkAccess(EntityFieldQuery $query) {
    parent::checkAccess($query);
    $query->propertyCondition('deleted_at', NULL);
  }
}

