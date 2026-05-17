# WebSocket Collaboration — Technical Documentation

Real-time multi-user collaboration for the Dynamic Form Builder.  
Allows multiple users editing the same form to see each other's presence and changes live.

---

## Table of Contents

1. [Overview](#overview)
2. [Files Involved](#files-involved)
3. [Architecture](#architecture)
4. [Server: ws_server.php](#server-ws_serverphp)
   - [Startup](#startup)
   - [Data Structures](#data-structures)
   - [WebSocket Handshake](#websocket-handshake)
   - [Frame Encoding and Decoding](#frame-encoding-and-decoding)
   - [Disconnect and Cleanup](#disconnect-and-cleanup)
5. [Client: collaborate.js](#client-collaboratejs)
   - [Initialization](#initialization)
   - [Connection and Reconnection](#connection-and-reconnection)
   - [Presence Bar](#presence-bar)
6. [Integration: builder.js](#integration-builderjs)
7. [Backend Integration](#backend-integration)
   - [Page Load](#page-load)
   - [Sections Reload Endpoint](#sections-reload-endpoint)
8. [Message Protocol](#message-protocol)
   - [join](#join)
   - [presence](#presence)
   - [section_renamed](#section_renamed)
   - [deleted](#deleted)
   - [content_changed](#content_changed)
9. [Action Flows](#action-flows)
   - [Section Rename](#section-rename-flow)
   - [Delete](#delete-flow)
   - [Reorder](#reorder-flow)
   - [Add or Edit Question](#add-or-edit-question-flow)
10. [Message Flow Diagram](#message-flow-diagram)
11. [PHP 8 Compatibility Notes](#php-8-compatibility-notes)
12. [Running the Server](#running-the-server)
13. [Limitations](#limitations)

---

## Overview

This is a lightweight, dependency-free WebSocket collaboration layer built on top of the existing form builder. It does not use any third-party WebSocket library (no Ratchet, no Node.js). The server is a single plain PHP script that manages its own socket I/O loop.

**What it enables:**

| Feature | Description |
|---|---|
| Live presence | Avatar bar showing all users currently on the same form |
| Section rename sync | Renaming a section updates all other users' titles in place |
| Delete sync | Deleting a section or question removes the card from all other users' views |
| Content sync | Adding, editing, or reordering content triggers a sections reload on all other clients |

---

## Files Involved

| File | Language | Role |
|---|---|---|
| `ws_server.php` | PHP | Standalone WebSocket server process |
| `sites/all/modules/dynamic_form/js/collaborate.js` | JavaScript | Browser WebSocket client, presence bar, DOM sync |
| `sites/all/modules/dynamic_form/js/builder.js` | JavaScript | Fires custom jQuery events after user actions succeed |
| `sites/all/modules/dynamic_form/includes/dynamic_form.builder.inc` | PHP | Loads collaborate.js, passes config to browser, serves sections-reload AJAX endpoint |
| `sites/all/modules/dynamic_form/dynamic_form.module` | PHP | Registers the `dynamic-form/ajax/builder/sections/%` Drupal menu route |
| `sites/all/modules/dynamic_form/css/builder.css` | CSS | Presence bar styles |

---

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                     Browser (User A)                    │
│                                                         │
│  builder.js          collaborate.js                     │
│  ──────────          ────────────────                   │
│  Fires events  ───►  Sends WS message                   │
│  after AJAX          Receives WS message                │
│  succeeds            Updates DOM / presence bar         │
└────────────────────────────┬────────────────────────────┘
                             │ ws://localhost:8080
                             │ (WebSocket, persistent)
                ┌────────────▼────────────┐
                │      ws_server.php      │
                │                         │
                │  socket_select() loop   │
                │  Routes messages by     │
                │  form_id                │
                └────────────┬────────────┘
                             │ ws://localhost:8080
                             │
┌────────────────────────────▼────────────────────────────┐
│                     Browser (User B)                    │
│                                                         │
│  collaborate.js receives message → updates DOM          │
│  If content_changed: GET /dynamic-form/ajax/builder/    │
│    sections/{form_id} → replaces #dfb-sections-wrapper  │
└─────────────────────────────────────────────────────────┘
```

The Drupal PHP application (Apache/XAMPP) and the WebSocket server (`ws_server.php`) are two separate processes. Drupal handles all form data persistence; the WebSocket server only passes messages between connected browsers and never touches the database.

---

## Server: ws_server.php

### Startup

```bash
php ws_server.php
```

The script:

1. Creates a TCP socket on `0.0.0.0:8080`
2. Sets `SO_REUSEADDR` so restarts do not fail on TIME_WAIT
3. Sets the server socket to **non-blocking** mode
4. Enters an infinite `while (true)` poll loop

### Data Structures

```php
$sockets  = [$server, $clientA, $clientB, ...];
// Flat array of Socket objects passed to socket_select() each iteration.

$clients  = [
  0 => ['socket' => $sock, 'handshaked' => true, 'form_id' => 42,
         'uid' => 7, 'name' => 'Alice', 'color' => '#3498DB'],
  1 => [...],
];
// Indexed by an auto-increment integer id ($next_id).

$sock_map = [
  spl_object_id($clientA) => 0,
  spl_object_id($clientB) => 1,
];
// Reverse lookup: PHP object ID → client id.
// Used to find the right $clients entry from a socket returned by socket_select().
```

**Why `spl_object_id` instead of `(int)$sock`:**  
PHP 8 changed socket handles from integer resources to `Socket` objects. Casting a `Socket` object to `int` produces the internal object handle, which can be reused after an object is destroyed — making it an unreliable key. `spl_object_id()` is the correct PHP 8 way to get a unique integer identifier for a live object. See [PHP 8 Compatibility Notes](#php-8-compatibility-notes).

### WebSocket Handshake

When a browser first connects it sends an HTTP Upgrade request:

```
GET / HTTP/1.1
Host: localhost:8080
Upgrade: websocket
Connection: Upgrade
Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==
Sec-WebSocket-Version: 13
```

`_ws_handshake()` processes this:

1. Extracts `Sec-WebSocket-Key`
2. Appends the fixed magic GUID specified by RFC 6455:  
   `258EAFA5-E914-47DA-95CA-C5AB0DC85B11`
3. SHA-1 hashes the concatenation and base64-encodes the result
4. Writes back:

```
HTTP/1.1 101 Switching Protocols
Upgrade: websocket
Connection: Upgrade
Sec-WebSocket-Accept: <base64-encoded-hash>
```

After this response the TCP connection stays open and both sides switch to WebSocket frame protocol. All subsequent reads and writes go through `_ws_decode()` / `_ws_encode()`.

### Frame Encoding and Decoding

WebSocket messages are not sent as plain text — they are wrapped in a binary frame.

**Frame structure (simplified for text frames):**

```
Byte 0:  FIN bit (1) | RSV (000) | Opcode (0001 = text)
Byte 1:  MASK bit | Payload length (7 bits)
         If length == 126: next 2 bytes are the real length (uint16)
         If length == 127: next 8 bytes are the real length (uint64, not supported here)
[4 bytes]: Masking key (only present if MASK bit is set)
[N bytes]: Payload XOR'd with the masking key byte-by-byte
```

Browser → server frames are **always masked** (RFC 6455 requirement).  
Server → browser frames are **never masked**.

| Opcode | Meaning | Handling |
|---|---|---|
| `0x01` | Text frame | Decode, parse JSON, route |
| `0x08` | Connection close | Call `_ws_disconnect()` |
| `0x09` | Ping | Send pong (`0x8A`) immediately |

`_ws_encode()` produces outgoing frames:
- Payload ≤ 125 bytes: 2-byte header
- Payload 126–65535 bytes: 4-byte header (`126` sentinel + uint16 length)

### Disconnect and Cleanup

`_ws_disconnect($id, &$sockets, &$clients, &$sock_map, $colours)`:

```
1. Look up the Socket object via $clients[$id]['socket']
2. Remove from $sock_map  (unset by spl_object_id)
3. Remove from $clients   (unset by id)
4. Remove from $sockets   (array_filter using $s !== $sock  identity, then array_values)
5. socket_close($sock)
6. Broadcast updated presence list to remaining clients on the same form_id
```

Step 4 uses `$s !== $sock` (object identity) rather than integer comparison — see [PHP 8 Compatibility Notes](#php-8-compatibility-notes).

`array_values()` is called after the filter to re-index the array so there are no gaps; `socket_select()` works on the values, not the keys, so gaps are harmless, but re-indexing keeps the array predictable.

---

## Client: collaborate.js

### Initialization

`Drupal.behaviors.dfbCollaborate` runs when Drupal calls `attachBehaviors`. It guards with:

```javascript
if (!document.getElementById('dfb-builder-container')) { return; }
```

So it only runs on the builder page. `.once('dfb-collaborate')` ensures it runs once per page load even if `attachBehaviors` is called multiple times (which happens after every Drupal AJAX rebuild).

After attaching, it calls `dfbCollab.init(formId, currentUser, wsUrl)` with values from `Drupal.settings.dynamicFormBuilder`.

### Connection and Reconnection

```javascript
dfbCollab.connect = function () {
  this.ws = new WebSocket('ws://localhost:8080');

  this.ws.onopen    = function () { /* send join message */ };
  this.ws.onmessage = function (e) { /* parse JSON, call _handleMessage */ };
  this.ws.onclose   = function () { setTimeout(connect, 3000); };
  this.ws.onerror   = function () { ws.close(); /* triggers onclose */ };
};
```

If the connection drops for any reason, `onclose` schedules a reconnect in 3 seconds. The `onopen` handler always re-sends the `join` message so the server restores this client's presence entry.

### Presence Bar

On init, a `<div id="dfb-collab-bar">` is prepended to `#dfb-builder-container`. It shows a "Connecting…" spinner until the first `presence` message arrives.

When a `presence` message is received, `_updatePresenceBar(users)` replaces the avatars:

```javascript
users.forEach(function (u) {
  var initial = u.name.charAt(0).toUpperCase();
  html += '<span class="dfb-collab-avatar" style="background:' + u.color + '"
            title="' + u.name + '">' + initial + '</span>';
});
```

The current user's avatar gets an extra `dfb-collab-avatar-me` class (purple border).  
Colours are assigned server-side as `$colours[$uid % 8]` — deterministic, so the same user always gets the same colour across reconnects.

---

## Integration: builder.js

`builder.js` is not aware of WebSockets directly. After each successful AJAX operation it fires a custom jQuery event on `<body>`. `collaborate.js` listens for these events and sends the corresponding WebSocket message.

| User action | builder.js trigger | collaborate.js sends |
|---|---|---|
| Section renamed | `$('body').trigger('dfb:section_renamed', [sectionId, name])` | `{ type: 'section_renamed', ... }` |
| Section or question deleted | `$('body').trigger('dfb:deleted', [entityType, entityId])` | `{ type: 'deleted', ... }` |
| Drag-to-reorder saved | `$('body').trigger('dfb:content_changed')` | `{ type: 'content_changed', ... }` |
| Question add/edit saved | triggered in `ajaxComplete` when `system/ajax` URL and modals are closed | `{ type: 'content_changed', ... }` |

This decoupling means `builder.js` has no dependency on the WebSocket layer — if the WebSocket server is not running, the builder works exactly as before.

---

## Backend Integration

### Page Load

`dynamic_form_builder_page()` in `dynamic_form.builder.inc` adds the following when rendering the builder page:

```php
global $user;

drupal_add_js($module_path . '/js/collaborate.js');
drupal_add_js(array(
  'dynamicFormBuilder' => array(
    'formId'      => (int) $form_record->id,
    'wsUrl'       => 'ws://localhost:8080',
    'currentUser' => array(
      'uid'  => (int) $user->uid,
      'name' => format_username($user),
    ),
  ),
), 'setting');
```

`format_username($user)` returns the display name as Drupal would show it (respects real name modules if installed).

### Sections Reload Endpoint

**Route:** `GET /dynamic-form/ajax/builder/sections/{form_id}`  
**Registered in:** `dynamic_form.module` → `hook_menu()`  
**Handler:** `dynamic_form_ajax_sections_reload()` in `dynamic_form.builder.inc`

This endpoint exists solely for the `content_changed` sync path. When User B receives a `content_changed` message, their browser calls this endpoint to get fresh HTML without doing a full page reload.

The handler:

1. Queries `dynamic_form_sections` ordered by `position ASC` for the given `form_id`
2. Queries all `dynamic_form_questions` for the form, groups them by `section_id`
3. Calls `_dynamic_form_render_builder_section()` for each section (same function the builder page itself uses)
4. Wraps the result in `<div id="dfb-sections-wrapper">` and returns a Drupal AJAX command:

```php
$commands[] = ajax_command_replace('#dfb-sections-wrapper', $html);
drupal_json_output($commands);
```

Back in `collaborate.js`, `_reloadSections()` applies these commands using Drupal's own `ajax.prototype.commands` machinery (the same `fakeAjax` pattern used elsewhere in `builder.js` for the edit-question modal), then calls `Drupal.attachBehaviors()` to re-initialize SortableJS on the new DOM.

---

## Message Protocol

All messages are JSON text frames. The server never stores messages — it only routes them.

### join

Sent by the browser immediately after the WebSocket connection opens.

```json
{
  "type":    "join",
  "form_id": 42,
  "uid":     7,
  "name":    "Alice"
}
```

The server stores `form_id`, `uid`, `name`, and assigns a colour. It then broadcasts a `presence` message to **all** clients on that form (including the sender).

### presence

Sent by the server to all clients on a form whenever anyone joins or leaves.

```json
{
  "type": "presence",
  "users": [
    { "uid": 7,  "name": "Alice", "color": "#3498DB" },
    { "uid": 12, "name": "Bob",   "color": "#E74C3C" }
  ]
}
```

The client replaces the entire presence bar contents on receipt.

### section_renamed

Sent by the client whose user performed the rename. Routed by the server to all **other** clients on the same form.

```json
{
  "type":       "section_renamed",
  "form_id":    42,
  "section_id": 9,
  "name":       "Personal Details"
}
```

The server appends `"sender_name"` before forwarding. Receiving clients call `_applySectionRename()` which does a targeted DOM text update — no page reload.

### deleted

Sent after a section or question is deleted.

```json
{
  "type":        "deleted",
  "form_id":     42,
  "entity_type": "question",
  "entity_id":   55
}
```

Receiving clients call `_applyDelete()` which removes the matching `.dfb-section-card` or `.dfb-question-card` from the DOM.

### content_changed

Sent after any structural change that requires a layout refresh (add question, edit question, reorder).

```json
{
  "type":    "content_changed",
  "form_id": 42
}
```

Receiving clients call `_reloadSections()` which fetches the sections-reload endpoint and replaces `#dfb-sections-wrapper`.

---

## Action Flows

### Section Rename Flow

```
User A types new name, presses Enter or blurs the input
  │
  ▼
builder.js: $.ajax POST /dynamic-form/ajax/builder/section/{id}/rename
  │
  ▼ (on success)
builder.js: updates $h2 text, shows toast
builder.js: $('body').trigger('dfb:section_renamed', [sectionId, name])
  │
  ▼
collaborate.js: dfbCollab.send({ type: 'section_renamed', section_id, name })
  │
  ▼  (WebSocket frame to ws_server.php)
ws_server.php: _ws_handle() → _ws_broadcast_except()
  │
  ▼  (WebSocket frame to User B)
collaborate.js (User B): _applySectionRename()
  → $('.dfb-section-title').text('New Name')
  → $('[data-section-name]').attr('data-section-name', 'New Name')
  → DFBToast.info('Alice renamed a section.')
```

### Delete Flow

```
User A clicks trash, confirms modal
  │
  ▼
builder.js: $.ajax POST /dynamic-form/ajax/delete/{type}/{id}
  │
  ▼ (on success)
builder.js: $card.remove(), shows toast
builder.js: $('body').trigger('dfb:deleted', [entityType, entityId])
  │
  ▼
collaborate.js: dfbCollab.send({ type: 'deleted', entity_type, entity_id })
  │
  ▼  ws_server.php → broadcast to others
  │
  ▼  User B: _applyDelete()
  → $('.dfb-question-card[data-question-id="55"]').remove()
  → DFBToast.info('Alice deleted a question.')
```

### Reorder Flow

```
User A drags a section or question to a new position
  │
  ▼
SortableJS: onEnd fires
builder.js: $.ajax POST /dynamic-form/ajax/builder/reorder
  │
  ▼ (on success)
builder.js: $('body').trigger('dfb:content_changed')
  │
  ▼
collaborate.js: dfbCollab.send({ type: 'content_changed', form_id })
  │
  ▼  ws_server.php → broadcast to others
  │
  ▼  User B: _reloadSections()
  → GET /dynamic-form/ajax/builder/sections/42
  → dynamic_form_ajax_sections_reload() queries DB, renders HTML
  → ajax_command_replace('#dfb-sections-wrapper', html)
  → Drupal.attachBehaviors() re-initializes SortableJS
```

### Add or Edit Question Flow

```
User A fills in question modal, clicks Save
  │
  ▼
Drupal AJAX: POST /system/ajax (native Drupal form submit)
  → runs dynamic_form_builder_sections_ajax_callback()
  → replaces #dfb-sections-wrapper with rebuilt form
  │
  ▼
$(document).ajaxComplete fires in builder.js
  → checks: URL contains 'system/ajax'
             AND add modal is not visible
             AND edit modal is not visible
             AND #dfb-sections-wrapper exists
  → $('body').trigger('dfb:content_changed')
  │
  ▼  same path as Reorder Flow above
```

---

## Message Flow Diagram

```
Browser A                    ws_server.php (port 8080)       Browser B
─────────                    ─────────────────────────       ─────────

[page load]
  │── WS connect ──────────────────────────────────────────────────►│ (no B yet)
  │── { join, form_id:42, uid:7 } ──────────────────────────────►│
  │◄─ { presence, users:[Alice] } ─────────────────────────────────│
  │                                                                 │
                                                          [page load]
                               │◄── WS connect ────────────────────│
                               │◄── { join, form_id:42, uid:12 } ──│
  │◄─ { presence, users:[Alice, Bob] } ─────────────────────────────│
                               │──── { presence, users:[A,B] } ────►│

  │  [renames a section]
  │── { section_renamed, section_id:9, name:"Contact" } ──────────►│
                               │──── { section_renamed, sender:"Alice" } ──►│
                                                                    │ [updates title in DOM]
                                                                    │ toast: "Alice renamed a section."

  │  [adds a question]
  │── { content_changed, form_id:42 } ─────────────────────────────►│
                               │──── { content_changed, sender:"Alice" } ──►│
                                                                    │ [GET /ajax/builder/sections/42]
                                                          ◄──────── │ Drupal returns ajax_command_replace
                                                                    │ [replaces #dfb-sections-wrapper]

  │  [closes tab]
  │── (close frame) ────────────────────────────────────────────────►│
                               │──── { presence, users:[Bob] } ────►│
                                                                    │ [Alice's avatar removed]
```

---

## PHP 8 Compatibility Notes

PHP 8.0 changed `socket_*()` functions to return `Socket` objects instead of integer resource handles. Two bugs arise if you treat them as integers:

**Bug 1 — Unreliable key lookup with `(int)$sock`**

In PHP 7, `(int)$resource` gave a stable resource ID. In PHP 8, `(int)$socketObject` gives the internal object handle from `spl_object_id()`. Object IDs are reused after the original object is garbage-collected. This means a new client socket could receive the same integer as a previously-disconnected client, causing the wrong client's metadata to be looked up or the wrong socket to be closed.

**Fix:** Use `spl_object_id($sock)` explicitly as the key in `$sock_map`. IDs are still reused, but the map entry is removed during disconnect before the object can be garbage-collected and its ID reused.

**Bug 2 — `array_filter` removing the server socket**

The original code filtered `$sockets` with `(int) $s !== $id`. If the server socket's object handle happened to equal a client's handle (due to ID reuse), the server socket would be silently removed from `$sockets`. On the next loop iteration, `$sockets` would be empty, and `socket_select([])` would throw:

```
ValueError: socket_select(): At least one array argument must be passed
```

**Fix:** Use object identity (`$s !== $sock`) instead of integer comparison. In PHP 8, `===` on two distinct `Socket` objects is always `false`, regardless of their internal IDs.

---

## Running the Server

**Start:**
```bash
php /Applications/XAMPP/xamppfiles/htdocs/dynamic-form-builder/ws_server.php
```

**Expected output:**
```
[WS] WebSocket server running on ws://0.0.0.0:8080
[WS] Press Ctrl+C to stop.

[WS] Client connected (id:0)
[WS] 'Alice' (uid:7) joined form #42
[WS] Client connected (id:1)
[WS] 'Bob' (uid:12) joined form #42
[WS] 'Alice' → 'section_renamed' on form #42
[WS] 'Bob' disconnected
```

**Stop:** `Ctrl+C`

After starting the server, clear Drupal's menu cache via  
`Admin → Configuration → Performance → Clear all caches`  
so the new `dynamic-form/ajax/builder/sections/%` route is registered.

---

## Limitations

- **No authentication on the WebSocket server.** Any process on localhost that can reach port 8080 can send messages. This is acceptable for a local experiment — do not expose port 8080 externally.
- **Single server, no persistence.** If `ws_server.php` restarts, all clients reconnect (automatic after 3 s) but no message history is replayed.
- **No conflict resolution.** If two users rename the same section simultaneously, the last rename wins for both — the client who renamed second sees the other's rename arrive and overwrite their own display.
- **`content_changed` causes a full sections reload.** All add/edit/reorder events result in a fresh DB query and DOM replacement on receiving clients. For large forms with many questions this adds latency. A future improvement would send a diff instead.
- **64-bit WebSocket frames are not supported.** `_ws_decode()` returns `false` for frames with a 127-length prefix (payload > 65 535 bytes). This is not a concern for JSON messages of this size.
- **XAMPP only — no process manager.** The server must be started manually and dies when the terminal closes. For a persistent setup, use `launchd`, `supervisor`, or `screen`.
