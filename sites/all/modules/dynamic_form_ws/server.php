<?php

/**
 * @file
 * CLI entry point for the Ratchet WebSocket server.
 *
 * Usage:
 *   cd sites/all/modules/dynamic_form_ws
 *   php server.php
 *
 * The server listens on 0.0.0.0:8080 and manages per-form presence rooms.
 * It shares the same MySQL database as Drupal 7 for token validation.
 */

// Resolve paths relative to this file.
$moduleDir = __DIR__;
$drupalRoot = realpath($moduleDir . '/../../../..');

// Load Composer autoloader (Ratchet + ReactPHP).
$autoloader = $moduleDir . '/vendor/autoload.php';
if (!file_exists($autoloader)) {
  fwrite(STDERR, "ERROR: vendor/autoload.php not found.\n");
  fwrite(STDERR, "Run: cd $moduleDir && composer install\n");
  exit(1);
}
require_once $autoloader;

// Load the presence handler (not using PSR-4 autoload here; plain require is fine
// since this file is outside the Drupal bootstrap).
require_once $moduleDir . '/src/FormBuilderPresence.php';

// Read Drupal DB credentials from sites/default/settings.php.
// We parse only the $databases array without a full Drupal bootstrap.
$settingsFile = $drupalRoot . '/sites/default/settings.php';
if (!file_exists($settingsFile)) {
  fwrite(STDERR, "ERROR: Cannot find $settingsFile\n");
  exit(1);
}

// Minimal stub so settings.php can be included without Drupal functions.
if (!function_exists('conf_path')) {
  function conf_path() { return 'sites/default'; }
}

$databases = array();
include $settingsFile;

if (empty($databases['default']['default'])) {
  fwrite(STDERR, "ERROR: No default database found in settings.php\n");
  exit(1);
}

$db   = $databases['default']['default'];
$host = $db['host'];
$port = isset($db['port']) && $db['port'] ? $db['port'] : 3306;

// On macOS/XAMPP, 'localhost' causes PDO to look for a Unix socket at the
// system-default path (/var/mysql/mysql.sock) which doesn't exist.
// Fix: use 127.0.0.1 (forces TCP) or, if that also fails, try the known
// XAMPP socket path.
if (strtolower($host) === 'localhost') {
  $host = '127.0.0.1';
}

$dsn = sprintf('mysql:host=%s;port=%s;dbname=%s;charset=utf8',
  $host, $port, $db['database']
);

try {
  $pdo = new PDO($dsn, $db['username'], $db['password'], array(
    PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
  ));
}
catch (PDOException $e) {
  // Fallback: try the XAMPP socket path directly.
  $xamppSocket = '/Applications/XAMPP/xamppfiles/var/mysql/mysql.sock';
  if (file_exists($xamppSocket)) {
    $dsn = sprintf('mysql:unix_socket=%s;dbname=%s;charset=utf8',
      $xamppSocket, $db['database']
    );
    try {
      $pdo = new PDO($dsn, $db['username'], $db['password'], array(
        PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
      ));
    }
    catch (PDOException $e2) {
      fwrite(STDERR, 'ERROR: DB connection failed (socket fallback): ' . $e2->getMessage() . "\n");
      exit(1);
    }
  }
  else {
    fwrite(STDERR, 'ERROR: DB connection failed: ' . $e->getMessage() . "\n");
    exit(1);
  }
}

// Build and start the Ratchet server.
use Ratchet\Server\IoServer;
use Ratchet\Http\HttpServer;
use Ratchet\WebSocket\WsServer;
use React\EventLoop\Factory;

$loop     = Factory::create();
$presence = new FormBuilderPresence($pdo);

// Purge expired tokens every 60 seconds.
$loop->addPeriodicTimer(60, function () use ($presence) {
  $presence->purgeExpiredTokens();
});

$server = IoServer::factory(
  new HttpServer(new WsServer($presence)),
  8080,
  '0.0.0.0',
  $loop
);

echo '[' . date('Y-m-d H:i:s') . '] WebSocket server started on port 8080' . PHP_EOL;

$loop->run();
