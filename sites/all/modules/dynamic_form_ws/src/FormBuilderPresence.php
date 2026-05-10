<?php

/**
 * @file
 * Ratchet MessageComponentInterface implementation for form builder presence.
 */

use Ratchet\MessageComponentInterface;
use Ratchet\ConnectionInterface;

class FormBuilderPresence implements MessageComponentInterface {

  /**
   * Rooms indexed by form_id.
   * Each room is an array keyed by connection resource ID:
   *   [ resourceId => ['conn'=>$conn, 'uid'=>int, 'name'=>str, 'color'=>str, 'cursor'=>array] ]
   *
   * @var array
   */
  protected $rooms = array();

  /**
   * Connections waiting for auth, keyed by resource ID.
   *
   * @var array
   */
  protected $pending = array();

  /**
   * Map from resource ID → form_id for quick room lookup on close.
   *
   * @var array
   */
  protected $connRoom = array();

  /**
   * PDO connection to the shared Drupal MySQL database.
   *
   * @var \PDO
   */
  protected $pdo;

  /**
   * 8-color palette; color is assigned as $colors[$uid % 8].
   *
   * @var array
   */
  protected $colors = array(
    '#e74c3c', '#3498db', '#2ecc71', '#f39c12',
    '#9b59b6', '#1abc9c', '#e67e22', '#e84393',
  );

  public function __construct(\PDO $pdo) {
    $this->pdo = $pdo;
    echo '[' . date('Y-m-d H:i:s') . '] FormBuilderPresence ready.' . PHP_EOL;
  }

  // ---------------------------------------------------------------------------
  // Ratchet interface
  // ---------------------------------------------------------------------------

  public function onOpen(ConnectionInterface $conn) {
    $id = $conn->resourceId;
    $this->pending[$id] = $conn;
    echo '[' . date('H:i:s') . '] Connection opened: ' . $id . PHP_EOL;
  }

  public function onMessage(ConnectionInterface $from, $raw) {
    $id  = $from->resourceId;
    $msg = json_decode($raw, TRUE);

    if (!is_array($msg) || empty($msg['type'])) {
      return;
    }

    if (isset($this->pending[$id])) {
      // Not yet authenticated — only accept auth messages.
      if ($msg['type'] === 'auth') {
        $this->handleAuth($from, $msg);
      }
      return;
    }

    switch ($msg['type']) {
      case 'cursor':
        $this->handleCursor($from, $msg);
        break;

      case 'idle':
        $this->handleIdle($from);
        break;
    }
  }

  public function onClose(ConnectionInterface $conn) {
    $id = $conn->resourceId;
    $this->cleanupConnection($id);
    echo '[' . date('H:i:s') . '] Connection closed: ' . $id . PHP_EOL;
  }

  public function onError(ConnectionInterface $conn, \Exception $e) {
    echo '[' . date('H:i:s') . '] Error on ' . $conn->resourceId . ': ' . $e->getMessage() . PHP_EOL;
    $conn->close();
  }

  // ---------------------------------------------------------------------------
  // Periodic maintenance (called from server.php timer)
  // ---------------------------------------------------------------------------

  public function purgeExpiredTokens() {
    $stmt = $this->pdo->prepare('DELETE FROM dynamic_form_ws_tokens WHERE expires_at < ?');
    $stmt->execute(array(time()));
  }

  // ---------------------------------------------------------------------------
  // Message handlers
  // ---------------------------------------------------------------------------

  protected function handleAuth(ConnectionInterface $conn, array $msg) {
    $id    = $conn->resourceId;
    $token = isset($msg['token']) ? trim($msg['token']) : '';

    if (empty($token)) {
      $conn->send(json_encode(array('type' => 'auth_error', 'message' => 'Missing token.')));
      $conn->close();
      return;
    }

    // Validate token against DB.
    $stmt = $this->pdo->prepare(
      'SELECT uid, username, form_id FROM dynamic_form_ws_tokens WHERE token = ? AND expires_at > ?'
    );
    $stmt->execute(array($token, time()));
    $row = $stmt->fetch(\PDO::FETCH_ASSOC);

    if (!$row) {
      $conn->send(json_encode(array('type' => 'auth_error', 'message' => 'Invalid or expired token.')));
      $conn->close();
      return;
    }

    $uid     = (int) $row['uid'];
    $formId  = (int) $row['form_id'];
    $name    = $row['username'];
    $color   = $this->colors[$uid % 8];

    // Move from pending into the appropriate room.
    unset($this->pending[$id]);
    if (!isset($this->rooms[$formId])) {
      $this->rooms[$formId] = array();
    }
    $this->rooms[$formId][$id] = array(
      'conn'   => $conn,
      'uid'    => $uid,
      'name'   => $name,
      'color'  => $color,
      'cursor' => array('section_id' => NULL, 'question_id' => NULL, 'action' => 'viewing'),
    );
    $this->connRoom[$id] = $formId;

    // Confirm auth to the connecting user.
    $conn->send(json_encode(array(
      'type' => 'auth_ok',
      'user' => array('uid' => $uid, 'name' => $name, 'color' => $color),
    )));

    // Send current presence state to the new user (everyone else in the room).
    $others = array();
    foreach ($this->rooms[$formId] as $rid => $peer) {
      if ($rid !== $id) {
        $others[] = array(
          'uid'    => $peer['uid'],
          'name'   => $peer['name'],
          'color'  => $peer['color'],
          'cursor' => $peer['cursor'],
        );
      }
    }
    $conn->send(json_encode(array('type' => 'presence_state', 'users' => $others)));

    // Broadcast user_joined to existing room members.
    $this->broadcastToRoom($formId, array(
      'type' => 'user_joined',
      'user' => array('uid' => $uid, 'name' => $name, 'color' => $color),
    ), $exclude = $id);

    echo '[' . date('H:i:s') . "] User $name (uid=$uid) joined form $formId" . PHP_EOL;
  }

  protected function handleCursor(ConnectionInterface $from, array $msg) {
    $id     = $from->resourceId;
    $formId = $this->connRoom[$id];
    $peer   = &$this->rooms[$formId][$id];

    $peer['cursor'] = array(
      'section_id'  => isset($msg['section_id'])  ? (int) $msg['section_id']  : NULL,
      'question_id' => isset($msg['question_id']) ? (int) $msg['question_id'] : NULL,
      'action'      => isset($msg['action'])      ? $msg['action']            : 'hovering',
    );

    $this->broadcastToRoom($formId, array(
      'type'   => 'cursor_update',
      'uid'    => $peer['uid'],
      'cursor' => $peer['cursor'],
    ), $exclude = $id);
  }

  protected function handleIdle(ConnectionInterface $from) {
    $id     = $from->resourceId;
    $formId = $this->connRoom[$id];
    $peer   = &$this->rooms[$formId][$id];

    $peer['cursor'] = array('section_id' => NULL, 'question_id' => NULL, 'action' => 'idle');

    $this->broadcastToRoom($formId, array(
      'type'   => 'cursor_update',
      'uid'    => $peer['uid'],
      'cursor' => $peer['cursor'],
    ), $exclude = $id);
  }

  // ---------------------------------------------------------------------------
  // Helpers
  // ---------------------------------------------------------------------------

  protected function broadcastToRoom($formId, array $payload, $exclude = NULL) {
    if (empty($this->rooms[$formId])) {
      return;
    }
    $json = json_encode($payload);
    foreach ($this->rooms[$formId] as $rid => $peer) {
      if ($rid !== $exclude) {
        $peer['conn']->send($json);
      }
    }
  }

  protected function cleanupConnection($id) {
    if (isset($this->pending[$id])) {
      unset($this->pending[$id]);
      return;
    }

    if (!isset($this->connRoom[$id])) {
      return;
    }

    $formId = $this->connRoom[$id];
    $uid    = isset($this->rooms[$formId][$id]) ? $this->rooms[$formId][$id]['uid'] : NULL;

    unset($this->rooms[$formId][$id]);
    unset($this->connRoom[$id]);

    if (empty($this->rooms[$formId])) {
      unset($this->rooms[$formId]);
    }

    if ($uid !== NULL) {
      $this->broadcastToRoom($formId, array('type' => 'user_left', 'uid' => $uid));
    }
  }
}
