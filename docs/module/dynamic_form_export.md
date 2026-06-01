# Dynamic Form Export Module Documentation

## Overview

The `dynamic_form_export` module provides XLSX download of form responses and
the audit log. It uses Drupal's Batch API to build large exports without hitting
PHP timeouts, then serves the file through a streaming download endpoint.

| Property | Value |
|---|---|
| **Module name** | `dynamic_form_export` |
| **Drupal core** | 7.x |
| **Depends on** | `dynamic_form`, `dynamic_form_response` |

---

## File Structure

```
sites/all/modules/dynamic_form_export/
├── dynamic_form_export.info
├── dynamic_form_export.module          # Permission, routes, sidebar alter, temp dir helper
├── includes/
│   ├── dynamic_form_export.hub.inc     # Export hub landing page
│   ├── dynamic_form_export.audit_log.inc # Audit log export form + batch
│   ├── dynamic_form_export.responses.inc # Response export batch
│   └── dynamic_form_export.helpers.inc   # Download + stream-file endpoints
├── css/
│   └── export-hub.css
└── js/
    └── export-hub.js
```

---

## Hooks Implemented

| Hook | Purpose |
|---|---|
| `hook_permission` | Declares `export dynamic form audit log` |
| `hook_menu` | Registers export hub and download routes |
| `hook_dynamic_form_dashboard_sidebar_items_alter` | Injects Export into the sidebar nav |

---

## Permission

| Permission | Description | Restricted |
|---|---|---|
| `export dynamic form audit log` | Download the full audit log as XLSX | Yes (`restrict access = TRUE`) |

This permission is intentionally narrow. Expand the sidebar alter check when
additional export types (e.g. response exports without the audit permission) are added.

---

## Routes

| Path | Callback | Access |
|---|---|---|
| `dashboard/export` | `dynamic_form_export_hub_page` | `export dynamic form audit log` |
| `dashboard/export/audit-log` | `dynamic_form_export_audit_log_form` | `export dynamic form audit log` |
| `dashboard/export/download` | `dynamic_form_export_download_page` | Logged-in |
| `dashboard/export/stream` | `dynamic_form_export_stream_file` | Logged-in |

---

## Export Hub (`dashboard/export`)

Landing page listing available export types. Currently shows the audit log
export. New export types should add a card here when the module is extended.

---

## Audit Log Export (`dashboard/export/audit-log`)

A Drupal form (`dynamic_form_export_audit_log_form`) that accepts optional date-range
filters and then kicks off a Batch API job to:

1. Query `dynamic_form_audit_log` in pages (avoiding memory exhaustion).
2. Write rows to an XLSX file in the temp directory.
3. On completion, redirect to `dashboard/export/download` with the filename in the
   session.

---

## Download and Streaming

### `dashboard/export/download`

Intermediate page (rendered by `dynamic_form_export_download_page`) that reads
the pending filename from the session and presents a "Download" link to the user.
Also clears the session key.

### `dashboard/export/stream`

Streams the XLSX file as an HTTP download with correct content-type headers.
Access is validated to ensure only the requesting user can stream their own file.

---

## Temp Directory

All export files are written to a temporary directory resolved by
`_dynamic_form_export_temp_dir()`:

```
{public://}/export-temp/
```

This function is defined in the `.module` file (not a `*.inc`) so it is always
available to batch workers regardless of which include Drupal has loaded at that
point in the batch lifecycle.

The directory is created with `drupal_mkdir()` and mode `0775` if it does not
exist. Files should be cleaned up after download.

---

## Sidebar Integration

`hook_dynamic_form_dashboard_sidebar_items_alter` adds an Export entry gated by
`export dynamic form audit log`:

```php
$items['export'] = array(
  'label' => t('Export'),
  'path'  => 'dashboard/export',
  'icon'  => '<i class="fa-solid fa-chart-bar"></i>',
);
```

Update the permission check here if new export types require different access.
