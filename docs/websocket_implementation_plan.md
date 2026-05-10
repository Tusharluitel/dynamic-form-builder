# WebSocket Collaborative Presence for Form Builder

Add real-time collaborative presence to the form builder page — when multiple users edit the same form, each user sees **who else is online** and **what section/question they're interacting with**, with colored highlights and avatar cursors (like Google Docs or Figma).

---

## Table of Contents

- [Architecture](#architecture)
- [How Presence Works](#how-presence-works)
- [What Users See](#what-users-see)
- [New Module Structure](#new-module-structure)
- [File-by-File Breakdown](#file-by-file-breakdown)
- [Existing File Modifications](#existing-file-modifications)
- [Execution Plan](#execution-plan)
- [Verification Plan](#verification-plan)

---

## Architecture

```
┌───────────────────────┐      ┌───────────────────────┐
│  Browser A (User 1)   │      │  Browser B (User 2)   │
│  builder.js           │      │  builder.js           │
│  dfb-websocket.js     │      │  dfb-websocket.js     │
└──────────┬────────────┘      └──────────┬────────────┘
           │ ws://localhost:8080           │
           ▼                              ▼
     ┌─────────────────────────────────────────┐
     │   Ratchet WebSocket Server (CLI :8080)  │
     │   FormBuilderPresence.php               │
     │   - Manages rooms per form_id           │
     │   - Broadcasts cursor/presence events   │
     └──────────────────┬──────────────────────┘
                        │ reads tokens
                        ▼
                ┌───────────────┐
                │   MySQL DB    │
                │ (shared with  │
                │   Drupal 7)   │
                └───────────────┘
```

Drupal 7 runs on Apache (synchronous request/response) and **cannot** hold persistent
WebSocket connections. The solution is a **sidecar architecture**:

1. **Ratchet WebSocket Server** — a long-running PHP CLI process on port 8080 that
   manages persistent client connections.
2. **Drupal module** — generates auth tokens and injects the JS client on builder pages.
3. **Client JS** — connects to the WS server, tracks cursor/interaction, renders
   other users' presence.

---

## How Presence Works

1. **User opens builder** → JS connects to `ws://localhost:8080` with an auth token + form ID
2. **Server validates token** → looks up `dynamic_form_ws_tokens` table, maps connection to user
3. **Server sends current presence** → list of other users already on this form
4. **User interacts** → JS sends throttled updates:
   `{type: "cursor", section_id: 5, question_id: 12, action: "editing"}`
5. **Server broadcasts** → all other connections on the same form receive the update
6. **User disconnects** → server broadcasts "user left" to remaining connections

---

## What Users See

- **Presence bar** at the top of the builder showing colored avatars of active collaborators
- **Colored border highlights** on the section/question card that another user is currently
  interacting with
- **"Editing" tooltip** showing the collaborator's name on highlighted elements
- **Fade-out** when a user stops interacting (5-second idle timeout per element)

---

## New Module Structure

```
sites/all/modules/dynamic_form_ws/
├── dynamic_form_ws.info           # Module metadata
├── dynamic_form_ws.module         # Drupal hooks: menu, init, token generation
├── dynamic_form_ws.install        # Schema for ws_tokens table
├── composer.json                  # Ratchet dependency
├── server.php                     # CLI entry: `php server.php`
├── src/
│   └── FormBuilderPresence.php    # Ratchet MessageComponentInterface
├── js/
│   └── dfb-websocket.js           # Client-side presence manager
└── css/
    └── dfb-presence.css           # Presence bar, cursor highlights
```

---

## File-by-File Breakdown

### `dynamic_form_ws.info`

Module info declaring dependency on `dynamic_form`, PHP version requirement.

---

### `composer.json`

```json
{
  "name": "dynamic_form/websocket",
  "description": "Ratchet WebSocket server for collaborative form builder presence",
  "require": {
    "cboden/ratchet": "^0.4"
  }
}
```

Installed via `composer install` inside the module directory.

---

### `dynamic_form_ws.install`

`hook_schema()` — defines the `dynamic_form_ws_tokens` table for authentication:

| Column       | Type        | Purpose                                |
|-------------|-------------|----------------------------------------|
| `token`      | VARCHAR(64) | Random token (primary key)             |
| `uid`        | INT         | Drupal user ID                         |
| `username`   | VARCHAR(60) | Display name (cached for WS server)    |
| `form_id`    | INT         | Which form this token is for           |
| `created_at` | INT         | UNIX timestamp                         |
| `expires_at` | INT         | Token expiry (created + 5 minutes)     |

`hook_uninstall()` — drops the table.

---

### `dynamic_form_ws.module`

**`hook_menu()`** — registers one AJAX endpoint:

| Method | Path                      | Purpose                                        |
|--------|---------------------------|-------------------------------------------------|
| GET    | `dynamic-form-ws/token/%` | Generate a short-lived auth token for a form ID |

Only accessible to logged-in users who pass `_dynamic_form_edit_access`.

**`hook_init()`** — on builder pages (`dashboard/forms/%/builder`):

1. Generates a WS auth token for the current user + form
2. Injects `dfb-websocket.js` and `dfb-presence.css`
3. Passes `Drupal.settings.dfbWebSocket` with:
   - `wsUrl`: `ws://localhost:8080`
   - `token`: the generated auth token
   - `formId`: current form ID
   - `currentUser`: `{uid, name, color}`

**Color assignment** — deterministically derived from UID:
```php
$colors = ['#e74c3c', '#3498db', '#2ecc71', '#f39c12',
           '#9b59b6', '#1abc9c', '#e67e22', '#e84393'];
$color  = $colors[$uid % 8];
```

---

### `server.php`

CLI entry point that starts the Ratchet server:

```bash
$ cd sites/all/modules/dynamic_form_ws
$ php server.php
[2026-05-10 17:00:00] WebSocket server started on port 8080
```

- Creates a ReactPHP event loop
- Instantiates `FormBuilderPresence` as the WebSocket handler
- Listens on `0.0.0.0:8080`
- Bootstraps Drupal minimally (just enough to use `db_select`) for token validation
- Periodically purges expired tokens (every 60s via timer)

---

### `src/FormBuilderPresence.php`

Implements `Ratchet\MessageComponentInterface`.

**Data structures:**
```php
// Connections grouped by form_id (rooms)
$this->rooms = [
    42 => [
        $connResourceId => [
            'conn'     => $conn,
            'uid'      => 1,
            'name'     => 'Alice',
            'color'    => '#e74c3c',
            'cursor'   => [
                'section_id'  => 5,
                'question_id' => null,
                'action'      => 'viewing'
            ]
        ]
    ]
];
```

**Message protocol:**

| Client → Server         | Payload                                                       | Server Action                                      |
|--------------------------|---------------------------------------------------------------|-----------------------------------------------------|
| `auth`                   | `{type:"auth", token:"abc123"}`                               | Validate token, join room, broadcast `user_joined`   |
| `cursor`                 | `{type:"cursor", section_id:5, question_id:12, action:"editing"}` | Update stored cursor, broadcast to room          |
| `idle`                   | `{type:"idle"}`                                               | Mark user as idle, broadcast                        |

| Server → Client          | Payload                                                       | When                                                |
|--------------------------|---------------------------------------------------------------|-----------------------------------------------------|
| `auth_ok`                | `{type:"auth_ok", user:{uid,name,color}}`                     | After successful auth                               |
| `auth_error`             | `{type:"auth_error", message:"..."}`                          | Token invalid/expired                               |
| `presence_state`         | `{type:"presence_state", users:[{uid,name,color,cursor},...]}` | Sent to newly connected user                        |
| `user_joined`            | `{type:"user_joined", user:{uid,name,color}}`                 | Another user connected                              |
| `user_left`              | `{type:"user_left", uid:1}`                                   | Another user disconnected                           |
| `cursor_update`          | `{type:"cursor_update", uid:1, cursor:{...}}`                 | Another user's cursor/interaction changed            |

**Lifecycle:**

- `onOpen($conn)` — connection opened, waiting for auth message
- `onMessage($from, $msg)`:
  - If not authenticated → expect `auth` message, validate token via DB
  - If authenticated → handle `cursor` and `idle` messages
- `onClose($conn)` — remove from room, broadcast `user_left`
- `onError($conn, $e)` — log error, close connection

---

### `js/dfb-websocket.js`

**Drupal behavior:**
```javascript
Drupal.behaviors.dfbWebSocket = {
  attach: function(context, settings) {
    // Only on builder page, only once
    if (!settings.dfbWebSocket) return;
    $('#dfb-builder-container', context).once('dfb-ws', function() {
      DfbPresence.init(settings.dfbWebSocket);
    });
  }
};
```

**`DfbPresence` object:**

| Method                  | Purpose                                                          |
|-------------------------|------------------------------------------------------------------|
| `init(config)`          | Create WebSocket connection, set up event listeners               |
| `connect()`             | Open WS, send auth on open, set up reconnect on close            |
| `reconnect()`           | Exponential backoff (1s → 2s → 4s → 8s → max 30s)               |
| `sendCursor(data)`      | Throttled (100ms) cursor position broadcast                      |
| `handleMessage(msg)`    | Route incoming messages to appropriate handler                    |
| `renderPresenceBar()`   | Update the presence bar with current users                       |
| `highlightElement(uid, cursor)` | Add colored border/badge to the target card              |
| `clearHighlight(uid)`   | Remove a user's highlight (on idle/leave)                        |

**Cursor tracking (delegated, throttled):**
- `mouseenter` on `.dfb-section-card` → send cursor with `action: "hovering"`
- `click` on `.dfb-question-editable` → send cursor with `action: "editing"`
- `mouseleave` from `#dfb-builder-container` → send `idle`
- Modal open (add/edit question) → send cursor with `action: "editing"` for that question

**Presence bar rendering:**
- Colored 32px circles with 2-letter initials
- Tooltip with full username on hover
- Smooth scale+fade animation on enter/exit

**Element highlighting:**
- Add `data-dfb-presence-uid="1"` and a colored left-border to the card
- Small avatar badge positioned top-right
- Tooltip "Alice is editing"
- Auto-remove after 5s of no cursor updates from that user on that element

---

### `css/dfb-presence.css`

**Presence bar:**
```css
.dfb-presence-bar {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 10px 16px;
  margin-bottom: 12px;
  background: rgba(255,255,255,0.05);
  border-radius: 10px;
  min-height: 44px;
}
```

**Avatar circles:**
```css
.dfb-presence-avatar {
  width: 32px; height: 32px;
  border-radius: 50%;
  display: flex;
  align-items: center;
  justify-content: center;
  color: #fff;
  font-size: 12px;
  font-weight: 600;
  cursor: default;
  transition: transform 0.2s, opacity 0.2s;
  animation: dfb-presence-pop 0.3s ease;
}
```

**Element highlights:**
```css
.dfb-presence-highlight {
  border-left: 3px solid var(--dfb-presence-color);
  box-shadow: 0 0 8px rgba(var(--dfb-presence-color-rgb), 0.2);
  transition: border-color 0.3s, box-shadow 0.3s;
}
```

**Animations:**
```css
@keyframes dfb-presence-pop {
  0%   { transform: scale(0); opacity: 0; }
  70%  { transform: scale(1.15); }
  100% { transform: scale(1); opacity: 1; }
}
```

**Color palette** — 8 colors cycled by `uid % 8`:
```
#e74c3c  #3498db  #2ecc71  #f39c12
#9b59b6  #1abc9c  #e67e22  #e84393
```

---

## Existing File Modifications

### `dynamic_form.builder.inc`

**File:** `sites/all/modules/dynamic_form/includes/dynamic_form.builder.inc`

In `dynamic_form_builder_page()` — add a presence bar container `<div>` above the
builder header card:

```diff
   $output  = '<div class="dfb-builder-container" id="dfb-builder-container"';
   $output .= ' data-form-id="' . (int) $form_record->id . '">';

+  // Presence bar — populated by dfb-websocket.js when WebSocket module is active.
+  $output .= '<div id="dfb-presence-bar" class="dfb-presence-bar"></div>';
+
   // Form header card.
   $output .= '<div class="dfb-builder-header-card">';
```

**This is the only change to existing files.** The WebSocket module is fully self-contained.

---

## Execution Plan

| Step | Task                                             | Files                                      |
|------|--------------------------------------------------|--------------------------------------------|
| 1    | Create module skeleton                           | `.info`, `.module` (empty), `composer.json` |
| 2    | Install Ratchet via Composer                     | `vendor/` (auto-generated)                 |
| 3    | Create install schema                            | `.install`                                 |
| 4    | Build the WS server                              | `server.php`, `src/FormBuilderPresence.php` |
| 5    | Build Drupal integration (token + JS injection)  | `.module`                                  |
| 6    | Build client-side JS                             | `js/dfb-websocket.js`                      |
| 7    | Build CSS for presence UI                        | `css/dfb-presence.css`                     |
| 8    | Modify builder page (add presence bar div)       | `dynamic_form.builder.inc`                 |
| 9    | Enable module + test end-to-end                  | —                                          |

---

## Verification Plan

### Automated Tests

1. Start the WS server: `php sites/all/modules/dynamic_form_ws/server.php &`
2. Enable the module via Drupal admin → modules
3. Open a builder page in Chrome — check DevTools → Network → WS tab for connection
4. Open the same builder page in an incognito/second browser
5. Verify: both browsers show each other's presence avatar in the presence bar
6. Hover/click on sections/questions in one browser — verify highlights in the other

### Manual Verification

- **Auth rejection** — connect with an invalid/expired token → server closes connection
- **Reconnection** — kill WS server, verify client shows disconnected state,
  auto-reconnects when server restarts
- **Channel isolation** — open two different forms in two browsers, verify they do NOT
  see each other's presence
- **User disconnect** — close one tab, verify the other removes the presence avatar
  within ~2 seconds
- **Idle timeout** — stop moving the mouse for 5s, verify highlights fade out

---

## Running the WebSocket Server

### Development

```bash
cd /Applications/XAMPP/xamppfiles/htdocs/dynamic-form-builder/sites/all/modules/dynamic_form_ws
php server.php
```

Keep the terminal open. The server runs until you stop it (Ctrl+C).

### Production (macOS launchd)

Create `~/Library/LaunchAgents/com.dynamicform.websocket.plist` to auto-start
the server on login. Alternatively, use **Supervisor** if running on Linux.

---

## Dependencies

| Dependency          | Version | Purpose                           | Install                  |
|--------------------|---------|------------------------------------|--------------------------|
| PHP                | 8.2+    | Already available via XAMPP        | —                        |
| Composer           | 2.x     | PHP dependency manager             | `brew install composer`  |
| cboden/ratchet     | ^0.4    | PHP WebSocket server library       | `composer install`       |
| react/event-loop   | (auto)  | Async event loop (Ratchet dep)     | (auto via Ratchet)       |
