<?php
/**
 * Simple WebSocket server for form builder collaboration.
 *
 * Run from terminal:  php ws_server.php
 * Listens on:         ws://localhost:8080
 *
 * PHP 8 note: socket_*() returns Socket objects, not integer resources.
 * We use object identity (===) and spl_object_id() throughout — never (int).
 *
 * Message types (JSON):
 *   join             { type, form_id, uid, name }
 *   presence         { type, users:[{uid,name,color}] }   (server→client)
 *   section_renamed  { type, form_id, section_id, name }
 *   deleted          { type, form_id, entity_type, entity_id }
 *   content_changed  { type, form_id }
 */

$host = '0.0.0.0';
$port = 8080;

$server = socket_create(AF_INET, SOCK_STREAM, SOL_TCP);
socket_set_option($server, SOL_SOCKET, SO_REUSEADDR, 1);
socket_bind($server, $host, $port);
socket_listen($server, 10);
socket_set_nonblock($server);

echo "[WS] WebSocket server running on ws://{$host}:{$port}\n";
echo "[WS] Press Ctrl+C to stop.\n\n";

// $sockets  — flat list of all open Socket objects (server + clients).
// $clients  — int id => { socket, handshaked, form_id, uid, name, color }
// $sock_map — spl_object_id($sock) => client id  (fast reverse lookup)
$sockets  = array($server);
$clients  = array();
$sock_map = array();
$next_id  = 0;

$colours = array('#E74C3C','#3498DB','#2ECC71','#F39C12','#9B59B6','#1ABC9C','#E67E22','#E91E63');

while (true) {
  $read  = $sockets;
  $write = $except = null;

  // Guard: socket_select() throws ValueError if $read is empty.
  if (empty($read)) {
    usleep(200000);
    continue;
  }

  $n = socket_select($read, $write, $except, 0, 200000);
  if ($n === false || $n === 0) {
    continue;
  }

  foreach ($read as $sock) {
    // ---- New connection ----
    if ($sock === $server) {
      $new = socket_accept($server);
      if ($new === false) { continue; }
      socket_set_nonblock($new);

      $id            = $next_id++;
      $sockets[]     = $new;
      $sock_map[spl_object_id($new)] = $id;
      $clients[$id]  = array(
        'socket'     => $new,
        'handshaked' => false,
        'form_id'    => null,
        'uid'        => null,
        'name'       => '',
        'color'      => '#999999',
      );
      echo "[WS] Client connected (id:{$id})\n";
      continue;
    }

    // ---- Existing client ----
    $obj_id = spl_object_id($sock);
    if (!isset($sock_map[$obj_id])) { continue; }
    $id = $sock_map[$obj_id];

    $data = @socket_read($sock, 8192);

    if ($data === false || $data === '') {
      _ws_disconnect($id, $sockets, $clients, $sock_map, $colours);
      continue;
    }

    if (!$clients[$id]['handshaked']) {
      _ws_handshake($sock, $data);
      $clients[$id]['handshaked'] = true;
      continue;
    }

    $opcode = ord($data[0]) & 0x0F;
    if ($opcode === 0x08) { // close frame
      _ws_disconnect($id, $sockets, $clients, $sock_map, $colours);
      continue;
    }
    if ($opcode === 0x09) { // ping → pong
      @socket_write($sock, chr(0x8A) . chr(0x00));
      continue;
    }

    $msg = _ws_decode($data);
    if ($msg === false) { continue; }

    $payload = json_decode($msg, true);
    if (!is_array($payload)) { continue; }

    _ws_handle($id, $payload, $sockets, $clients, $sock_map, $colours);
  }
}

/* -----------------------------------------------------------------------
   WebSocket protocol helpers
   --------------------------------------------------------------------- */

function _ws_handshake($socket, $request) {
  if (!preg_match('/Sec-WebSocket-Key: (.+)\r\n/', $request, $m)) { return; }
  $accept   = base64_encode(sha1(trim($m[1]) . '258EAFA5-E914-47DA-95CA-C5AB0DC85B11', true));
  $response = "HTTP/1.1 101 Switching Protocols\r\n"
            . "Upgrade: websocket\r\n"
            . "Connection: Upgrade\r\n"
            . "Sec-WebSocket-Accept: {$accept}\r\n\r\n";
  @socket_write($socket, $response);
}

function _ws_decode($data) {
  if (strlen($data) < 2) { return false; }
  $masked = (ord($data[1]) & 0x80) !== 0;
  $len    = ord($data[1]) & 0x7F;
  $offset = 2;

  if ($len === 126) {
    if (strlen($data) < 4) { return false; }
    $len    = (ord($data[2]) << 8) | ord($data[3]);
    $offset = 4;
  } elseif ($len === 127) {
    return false; // 64-bit frames not needed for this experiment
  }

  if ($masked) {
    if (strlen($data) < $offset + 4 + $len) { return false; }
    $mask    = substr($data, $offset, 4);
    $offset += 4;
    $payload = '';
    for ($i = 0; $i < $len; $i++) {
      $payload .= $data[$offset + $i] ^ $mask[$i % 4];
    }
  } else {
    $payload = substr($data, $offset, $len);
  }
  return $payload;
}

function _ws_encode($text) {
  $len = strlen($text);
  if ($len <= 125)   { return chr(0x81) . chr($len) . $text; }
  if ($len <= 65535) { return chr(0x81) . chr(126) . pack('n', $len) . $text; }
  return chr(0x81) . chr(127) . pack('NN', 0, $len) . $text;
}

function _ws_send($socket, array $data) {
  @socket_write($socket, _ws_encode(json_encode($data)));
}

/* -----------------------------------------------------------------------
   Routing helpers
   --------------------------------------------------------------------- */

function _ws_broadcast_except($from_id, $form_id, array $data, &$clients) {
  foreach ($clients as $id => $c) {
    if ($id === $from_id)             { continue; }
    if ($c['form_id'] !== $form_id)   { continue; }
    if (!$c['handshaked'])            { continue; }
    _ws_send($c['socket'], $data);
  }
}

function _ws_broadcast_all($form_id, array $data, &$clients) {
  foreach ($clients as $c) {
    if ($c['form_id'] !== $form_id) { continue; }
    if (!$c['handshaked'])          { continue; }
    _ws_send($c['socket'], $data);
  }
}

function _ws_presence_list($form_id, &$clients) {
  $users = array();
  $seen  = array();
  foreach ($clients as $c) {
    if ($c['form_id'] !== $form_id || !$c['handshaked'] || !$c['uid']) { continue; }
    if (isset($seen[$c['uid']])) { continue; }
    $seen[$c['uid']] = true;
    $users[] = array('uid' => $c['uid'], 'name' => $c['name'], 'color' => $c['color']);
  }
  return $users;
}

/* -----------------------------------------------------------------------
   Message handler
   --------------------------------------------------------------------- */

function _ws_handle($from_id, array $payload, &$sockets, &$clients, &$sock_map, $colours) {
  $type = isset($payload['type']) ? $payload['type'] : '';

  if ($type === 'join') {
    $form_id = (int) (isset($payload['form_id']) ? $payload['form_id'] : 0);
    $uid     = (int) (isset($payload['uid'])     ? $payload['uid']     : 0);
    $name    = isset($payload['name'])
               ? htmlspecialchars(strip_tags($payload['name']), ENT_QUOTES)
               : 'Anonymous';
    $color   = $colours[$uid % count($colours)];

    $clients[$from_id]['form_id'] = $form_id;
    $clients[$from_id]['uid']     = $uid;
    $clients[$from_id]['name']    = $name;
    $clients[$from_id]['color']   = $color;

    echo "[WS] '{$name}' (uid:{$uid}) joined form #{$form_id}\n";

    _ws_broadcast_all($form_id, array(
      'type'  => 'presence',
      'users' => _ws_presence_list($form_id, $clients),
    ), $clients);
    return;
  }

  $form_id = isset($clients[$from_id]['form_id']) ? $clients[$from_id]['form_id'] : null;
  if (!$form_id) { return; }

  $allowed = array('section_renamed', 'content_changed', 'deleted');
  if (in_array($type, $allowed, true)) {
    $payload['sender_name'] = $clients[$from_id]['name'];
    _ws_broadcast_except($from_id, $form_id, $payload, $clients);
    echo "[WS] '{$clients[$from_id]['name']}' → '{$type}' on form #{$form_id}\n";
  }
}

/* -----------------------------------------------------------------------
   Disconnect
   --------------------------------------------------------------------- */

function _ws_disconnect($id, &$sockets, &$clients, &$sock_map, $colours) {
  if (!isset($clients[$id])) { return; }

  $name    = $clients[$id]['name'] ?: "client#{$id}";
  $form_id = $clients[$id]['form_id'];
  $sock    = $clients[$id]['socket'];

  // Remove from lookup map first.
  $obj_id = spl_object_id($sock);
  unset($sock_map[$obj_id]);
  unset($clients[$id]);

  // Remove from the sockets list using identity (===), never int-cast.
  $sockets = array_values(array_filter($sockets, function ($s) use ($sock) {
    return $s !== $sock;
  }));

  @socket_close($sock);

  echo "[WS] '{$name}' disconnected\n";

  if ($form_id) {
    _ws_broadcast_all($form_id, array(
      'type'  => 'presence',
      'users' => _ws_presence_list($form_id, $clients),
    ), $clients);
  }
}
