# Dynamic Form Module Documentation

## Overview

The `dynamic_form` module is the core of the Dynamic Form Builder platform. It owns the
form/section/question CRUD, the drag-and-drop builder UI, member and invitation management,
soft-delete + trash, audit logging, cron-based email dispatch, rate limiting on the REST
API, and a two-layer question cache.

| Property | Value |
|---|---|
| **Module name** | `dynamic_form` |
| **Drupal core** | 7.x |
| **Dependencies** | `file`, `field`, `field_ui`, `entity`, `restws` |

---

## File Structure

```
sites/all/modules/dynamic_form/
├── dynamic_form.info
├── dynamic_form.install        # Schema, enable/disable/uninstall hooks
├── dynamic_form.module         # All hooks + shared helpers
├── includes/
│   ├── dynamic_form.auth.inc       # Login / register page callbacks
│   ├── dynamic_form.builder.inc    # Form builder page + AJAX callbacks
│   ├── dynamic_form.dashboard.inc  # Dashboard pages, activity log, soft-delete AJAX
│   ├── dynamic_form.entity.inc     # DynamicFormSoftDeleteEntityController class
│   ├── dynamic_form.forms.inc      # Form create/edit/delete Drupal forms
│   ├── dynamic_form.front.inc      # Public forms listing + single-form view
│   ├── dynamic_form.invitations.inc # Invitation send/accept/resend/revoke
│   ├── dynamic_form.members.inc    # Member AJAX search + email notification
│   ├── dynamic_form.preview.inc    # Form preview page
│   ├── dynamic_form.profile.inc    # User profile page
│   ├── dynamic_form.questions.inc  # Question create/edit/delete forms
│   ├── dynamic_form.sections.inc   # Section delete form
│   ├── dynamic_form.tags.inc       # Tag autocomplete endpoint
│   └── dynamic_form.trash.inc      # Trash page + restore/permanent-delete
├── css/
│   ├── builder.css
│   ├── dashboard-delete.css
│   ├── dashboard-search.css
│   ├── filter-toggle.css
│   ├── invitations.css
│   ├── members.css
│   ├── preview.css
│   ├── profile.css
│   └── toast.css
└── js/
    ├── builder.js
    ├── collaborate.js
    ├── dashboard-delete.js
    ├── dashboard-search.js
    ├── filter-toggle.js
    ├── invitations.js
    ├── members.js
    └── toast.js
```

---

## Hooks Implemented

| Hook | File | Purpose |
|---|---|---|
| `hook_init` | `.module` | Applies REST API rate limiting before any output |
| `hook_schema` | `.install` | Creates all `dynamic_form_*` database tables |
| `hook_enable` | `.install` | Enables the `dynamic_form` theme on first install |
| `hook_disable` | `.install` | Reverts to the default theme |
| `hook_uninstall` | `.install` | Drops all tables and variables |
| `hook_entity_info` | `.module` | Registers 4 entity types for restws |
| `hook_entity_property_info` | `.module` | Declares typed properties for restws serialisation |
| `hook_restws_resource_info_alter` | `.module` | Replaces restws controller for soft-delete support |
| `hook_permission` | `.module` | Declares 10 granular permissions |
| `hook_menu` | `.module` | Registers all page + AJAX routes |
| `hook_theme` | `.module` | Registers `dynamic_form_dashboard` template |
| `hook_form_alter` | `.module` | Overrides core login error messages |
| `hook_cron` | `.module` | Processes email queues and expires invitations |
| `hook_mail` | `.module` | Builds email bodies for `member_added` and `invitation_sent` |

---

## Permissions

| Permission | Description |
|---|---|
| `access dashboard` | Access the form builder dashboard |
| `view forms` | View and fill out forms |
| `create new form` | Create new forms |
| `edit any form` | Edit any form regardless of ownership |
| `edit own form` | Edit only own forms |
| `delete any form` | Delete any form |
| `delete own form` | Delete only own forms |
| `view soft delete` | Access the Trash page |
| `restore deleted` | Restore or permanently delete trashed items |
| `view activity log` | Access the Activity Log page |

---

## Routes

### Authentication

| Path | Callback | Access |
|---|---|---|
| `login` | `dynamic_form_login_page` | Anonymous only |
| `register` | `dynamic_form_register_page` | Anonymous only |

### Form CRUD

| Path | Callback | Access |
|---|---|---|
| `form/create` | `dynamic_form_create_form` | `create new form` |
| `form/list` | `dynamic_form_list_page` | `view forms` |
| `form/%/edit` | `dynamic_form_edit_form` | `_dynamic_form_edit_access` |
| `form/%/delete` | `dynamic_form_delete_confirm` | `_dynamic_form_delete_access` |

### Dashboard

| Path | Callback | Access |
|---|---|---|
| `dashboard` | redirect → `dashboard/analytics` | Logged-in |
| `dashboard/forms` | `dynamic_form_dashboard_page` | Logged-in |
| `dashboard/questions` | `dynamic_form_dashboard_page` | Logged-in |
| `dashboard/sections` | `dynamic_form_dashboard_page` | Logged-in |
| `dashboard/forms/create` | `dynamic_form_dashboard_form_create_page` | `create new form` |
| `dashboard/forms/%/edit` | `dynamic_form_dashboard_form_edit_page` | `_dynamic_form_edit_access` |
| `dashboard/forms/%/builder` | `dynamic_form_builder_page` | `_dynamic_form_edit_access` |
| `dashboard/forms/%/preview` | `dynamic_form_preview_page` | `_dynamic_form_edit_access` |
| `dashboard/sections/create` | `dynamic_form_dashboard_section_create_page` | `create new form` |
| `dashboard/sections/%/edit` | `dynamic_form_dashboard_section_edit_page` | `edit any form` |
| `dashboard/sections/%/delete` | `dynamic_form_delete_section_confirm` | `delete any form` |
| `dashboard/questions/%/edit` | `dynamic_form_dashboard_question_edit_page` | `edit any form` |
| `dashboard/questions/%/delete` | `dynamic_form_delete_question_confirm` | `delete any form` |
| `dashboard/trash` | `dynamic_form_dashboard_trash_page` | `view soft delete` |
| `dashboard/trash/%/%/restore` | `dynamic_form_trash_restore_confirm` | `restore deleted` |
| `dashboard/trash/%/%/delete-permanent` | `dynamic_form_trash_delete_permanent_confirm` | `restore deleted` |
| `dashboard/activity-log` | `dynamic_form_activity_log_page` | `view activity log` |
| `dashboard/browse` | `dynamic_form_dashboard_browse_page` | Logged-in |
| `dashboard/profile` | `dynamic_form_profile_page` | Logged-in |

### AJAX Endpoints

| Path | Callback | Notes |
|---|---|---|
| `dynamic-form/ajax/builder/reorder` | `dynamic_form_ajax_reorder` | Drag-and-drop position save |
| `dynamic-form/ajax/builder/question/%/edit-form` | `dynamic_form_ajax_question_edit_form` | Inline question edit modal |
| `dynamic-form/ajax/builder/section/%/rename` | `dynamic_form_ajax_section_rename` | Inline section rename |
| `dynamic-form/ajax/builder/sections/%` | `dynamic_form_ajax_sections_reload` | Reload section list |
| `dynamic-form/ajax/members/search` | `dynamic_form_ajax_member_search` | User picker search |
| `dynamic-form/ajax/question/%/toggle-filter` | `dynamic_form_ajax_toggle_allow_filter` | Toggle `allow_filter` flag |
| `dynamic-form/ajax/invitations/send` | `dynamic_form_ajax_invitation_send` | Send invite email |
| `dynamic-form/ajax/invitations/%/resend` | `dynamic_form_ajax_invitation_resend` | Resend invite |
| `dynamic-form/ajax/invitations/%/revoke` | `dynamic_form_ajax_invitation_revoke` | Cancel invite |
| `dynamic-form/ajax/invitations/%/list` | `dynamic_form_ajax_invitation_list` | List invitations for a form |
| `dynamic-form/ajax/tags/autocomplete` | `dynamic_form_ajax_tags_autocomplete` | Tag typeahead (MySQL fallback) |
| `dynamic-form/ajax/trash/%/%/restore` | `dynamic_form_ajax_entity_restore` | Restore from trash |
| `dynamic-form/ajax/delete/%/%` | `dynamic_form_ajax_entity_delete` | Soft-delete entity |

### Public

| Path | Callback | Access |
|---|---|---|
| `forms` | `dynamic_form_public_forms_page` | Everyone |
| `forms/%` | `dynamic_form_public_form_view_page` | Everyone |
| `dynamic-form/invite/accept/%` | `dynamic_form_invite_accept_page` | Everyone (token-gated) |

---

## Rate Limiting

The module enforces per-IP and per-user flood limits on all `.json` (restws) API
requests via `hook_init`.

| Variable | Default | Description |
|---|---|---|
| `dynamic_form_rate_limit_ip_threshold` | `60` | Requests per window (per IP) |
| `dynamic_form_rate_limit_ip_window` | `60` | Window in seconds |
| `dynamic_form_rate_limit_user_threshold` | `120` | Requests per window (authenticated) |
| `dynamic_form_rate_limit_user_window` | `60` | Window in seconds |

Exceeding either limit returns HTTP `429 Too Many Requests` with a JSON body and
a `Retry-After` header. The flood state is stored in Drupal's `{flood}` table —
no extra dependencies required.

Override defaults with `variable_set()` in `settings.php` or the database.

---

## Entity Types (restws)

Four read-only entity types are registered so the restws module exposes them
as `.json` endpoints. All four use `DynamicFormSoftDeleteEntityController`
(declared in `includes/dynamic_form.entity.inc`) which filters out
`deleted_at IS NOT NULL` rows before they reach the API.

| Entity type | Base table | URL pattern |
|---|---|---|
| `dynamic_form_form` | `dynamic_form_forms` | `/dynamic_form_form/{id}.json` |
| `dynamic_form_section` | `dynamic_form_sections` | `/dynamic_form_section/{id}.json` |
| `dynamic_form_question` | `dynamic_form_questions` | `/dynamic_form_question/{id}.json` |
| `dynamic_form_question_option` | `dynamic_form_question_options` | `/dynamic_form_question_option/{id}.json` |

All write operations (`create`, `update`, `delete`) are blocked via
`_dynamic_form_entity_readonly_guard()`. Access for `view` follows the same rule
as the v1 API: `view any form responses` OR form creator OR owner/editor member.

---

## Access Callbacks

| Callback | Used for | Logic |
|---|---|---|
| `_dynamic_form_edit_access($form_id)` | Edit/builder routes | `edit any form` OR (own form + `edit own form`) OR member with `editor`/`reviewer` role |
| `_dynamic_form_delete_access($form_id)` | Delete route | `delete any form` OR (own form + `delete own form`) |
| `_dynamic_form_api_can_view_form($form_id, $account)` | REST entity access | `view any form responses` OR creator OR owner/editor member |

---

## Question Cache

Questions are cached at two levels to avoid repeated DB queries in the builder and preview:

1. **Request-level static cache** — `drupal_static('dynamic_form_get_questions')` keyed by `form_id`.
2. **Persistent cache** — Drupal cache API under the bin key `dynamic_form:questions:{form_id}`.

```php
// Read
$questions = dynamic_form_get_questions($form_id);

// Invalidate after any write
dynamic_form_invalidate_questions_cache($form_id);
```

`dynamic_form_invalidate_questions_cache()` must be called after any insert, update,
delete, or reorder on `dynamic_form_questions` rows.

---

## Audit Logging

Every create/update/delete on forms, sections, and questions must be recorded:

```php
_dynamic_form_audit_log(
  $entity_type,  // 'form', 'section', or 'question'
  $entity_id,
  $action,       // 'created', 'updated', or 'deleted'
  $old_value,    // array|null  (before state)
  $new_value     // array|null  (after state)
);
```

The helper writes one row to `dynamic_form_audit_log` including the actor's uid and IP
address. `old_value` / `new_value` are stored as JSON. Pass `NULL` for the inapplicable
side (`old_value = NULL` on create, `new_value = NULL` on delete).

---

## Background Email System (Cron + Queue)

`hook_cron` processes two DrupalQueue queues in configurable batches:

| Queue | Worker | Batch variable | Default |
|---|---|---|---|
| `dynamic_form_invite_notify` | `_dynamic_form_send_invite_notification` | `dynamic_form_invite_batch_size` | `20` |
| `dynamic_form_member_notify` | `_dynamic_form_send_member_notification` | `dynamic_form_member_batch_size` | `20` |

Cron also auto-expires pending invitations whose `expires_at < REQUEST_TIME`.

### Email keys (`hook_mail`)

| Key | Trigger | Description |
|---|---|---|
| `member_added` | Direct user addition to a form | Notifies the user of their new role |
| `invitation_sent` | Token-based invite to unregistered user | Includes a 7-day accept link |

---

## Shared Helper Functions

| Function | Description |
|---|---|
| `_dynamic_form_generate_slug($title)` | Produces a unique URL-safe slug; appends `-N` suffix if taken |
| `_dynamic_form_normalize_tag($raw)` | Lowercases, strips non-letter/digit characters; returns `FALSE` on empty |
| `_dynamic_form_ws_secret()` | Derives an HMAC secret from Drupal's private key; writes to `sys_get_temp_dir()/.dfb_ws_secret` for `ws_server.php` |
| `ajax_command_dfb_toast($type, $message)` | Returns an AJAX command array that triggers a frontend DFBToast notification |

---

## DFBToast Notifications

AJAX callbacks return toast commands to give the user visual feedback without a page reload:

```php
return array(
  ajax_command_dfb_toast('success', t('Form saved.')),
  // … other AJAX commands
);
```

Valid `$type` values: `success`, `error`, `warning`, `info`.

The corresponding JavaScript is in `js/toast.js` and CSS in `css/toast.css`.
