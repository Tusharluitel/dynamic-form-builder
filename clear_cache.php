<?php

define('DRUPAL_ROOT', getcwd());
require_once DRUPAL_ROOT . '/includes/bootstrap.inc';

drupal_bootstrap(DRUPAL_BOOTSTRAP_FULL);

drupal_flush_all_caches();

echo "Caches cleared.\n";