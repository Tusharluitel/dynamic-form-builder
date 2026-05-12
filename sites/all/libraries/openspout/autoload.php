<?php

spl_autoload_register(function ($class) {
  $prefix = 'OpenSpout\\';
  $base_dir = __DIR__ . '/src/';

  if (strncmp($prefix, $class, strlen($prefix)) !== 0) {
    return;
  }

  $relative = substr($class, strlen($prefix));
  $file = $base_dir . str_replace('\\', '/', $relative) . '.php';

  if (file_exists($file)) {
    require $file;
  }
});
