# Dynamic Form Builder — Documentation Index

Drupal 7 enterprise dynamic form builder. Custom modules live under
`sites/all/modules/dynamic_form*`; the custom theme is at
`sites/all/themes/dynamic_form`.

---

## Modules

| Module | Purpose | Doc |
|---|---|---|
| `dynamic_form` | Core: form builder UI, permissions, AJAX endpoints, rate limiting, cron | [dynamic_form_module.md](module/dynamic_form_module.md) |
| `dynamic_form_response` | Public form submission, response dashboard | [dynamic_form_response.md](module/dynamic_form_response.md) |
| `dynamic_form_analytics` | D3 charts, word cloud, radar | [dynamic_form_analytics.md](module/dynamic_form_analytics.md) |
| `dynamic_form_export` | XLSX export of responses and audit log | [dynamic_form_export.md](module/dynamic_form_export.md) |
| `dynamic_form_solr` | Apache Solr tag + response indexing, full-text search | [dynamic_form_solr.md](module/dynamic_form_solr.md) |

## REST API

| Topic | Doc |
|---|---|
| Custom v1 response API (`/api/v1/forms/{id}/responses`) | [API.md](module/API.md) |
| Word cloud AJAX endpoints | [word_cloud.md](module/word_cloud.md) |

## Theme

| Topic | Doc |
|---|---|
| `dynamic_form` theme structure and templates | [dynamic_form_theme.md](theme/dynamic_form_theme.md) |

## Database

| Topic | Doc |
|---|---|
| Schema overview and table descriptions | [schema.md](database/schema.md) |

---

## Architecture at a Glance

```
Browser
  └─ dynamic_form theme (page.tpl.php, template.php)
       └─ dashboard sidebar (hook_dynamic_form_dashboard_sidebar_items_alter)
            ├─ dynamic_form          core builder, questions, sections, members, invitations
            ├─ dynamic_form_response submission form, response list, AJAX save/submit
            ├─ dynamic_form_analytics D3 charts, word cloud, radar
            ├─ dynamic_form_export   XLSX batch export
            └─ dynamic_form_solr     Solr tag autocomplete, response filtering

REST (Basic Auth)
  ├─ /api/v1/forms/{id}/responses[/{id}]   custom PHP handler
  └─ /{entity_type}/{id}.json              restws entities (read-only)
```

## Contribution Notes

- Drupal 7 — all PHP uses procedural hooks, no OOP outside entity controllers.
- jQuery 1.12.4 via the `jquery_update` module. Use `.on()` / `.prop()` (not `.live()` / `.attr()`).
- Tag normalisation: always call `_dynamic_form_normalize_tag()` before inserting to `dynamic_form_tags`.
- Cache: call `dynamic_form_invalidate_questions_cache($form_id)` after any question write.
- Audit: call `_dynamic_form_audit_log()` for every form/section/question mutation.
