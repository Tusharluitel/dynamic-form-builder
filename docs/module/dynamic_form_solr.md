# Dynamic Form Solr Module Documentation

## Overview

The `dynamic_form_solr` module indexes form tags and submitted responses into
Apache Solr, enabling full-text search, per-question filtering, faceted counts,
and typo-tolerant tag autocomplete. It is an optional enhancement — the core
system works without it; falling back to MySQL autocomplete when the module is
absent.

| Property | Value |
|---|---|
| **Module name** | `dynamic_form_solr` |
| **Drupal core** | 7.x |
| **Depends on** | `dynamic_form`, `dynamic_form_response` |
| **External service** | Apache Solr (tested with 8.x) |

---

## File Structure

```
sites/all/modules/dynamic_form_solr/
├── dynamic_form_solr.info
├── dynamic_form_solr.module    # All Solr logic: index, delete, search, filter, facets
├── dynamic_form_solr.admin.inc # Admin form: Solr URL config + re-index triggers
├── dynamic_form_solr.pages.inc # Tag autocomplete page callback (overrides core endpoint)
├── dynamic_form_solr.css       # Loading indicator for tag autocomplete
```

---

## Configuration

| Drupal variable | Default | Description |
|---|---|---|
| `dynamic_form_solr_url` | `http://localhost:8983/solr/dynamic-form-builder` | Solr core base URL |

Set via the admin UI at `admin/config/search/dynamic-form-solr`, or with
`variable_set('dynamic_form_solr_url', '...')` in `settings.php`.

---

## Hooks Implemented

| Hook | Purpose |
|---|---|
| `hook_menu` | Registers autocomplete AJAX + admin config routes |
| `hook_menu_alter` | Replaces `dynamic-form/ajax/tags/autocomplete` with the Solr handler |
| `hook_init` | Loads CSS on public form pages |
| `hook_dynamic_form_form_predelete` | Removes all Solr response documents for a form before DB deletion |
| `hook_dynamic_form_response_predelete` | Removes a single Solr response document before DB deletion |

---

## Routes

| Path | Callback | Access |
|---|---|---|
| `dynamic-form/ajax/tags/solr-autocomplete` | `dynamic_form_solr_tags_autocomplete` | Everyone |
| `admin/config/search/dynamic-form-solr` | `dynamic_form_solr_admin_form` | `administer site configuration` |

`hook_menu_alter` redirects the core MySQL autocomplete path
(`dynamic-form/ajax/tags/autocomplete`) to the Solr handler when this module
is enabled. Disabling the module restores the MySQL fallback automatically.

---

## Document Types

The Solr core stores two document types, distinguished by the `doc_type` field.

### Tag Documents

One document per row in `dynamic_form_tags`.

| Solr field | Type prefix | Description |
|---|---|---|
| `id` | — | `"tag-{id}"` (unique doc ID) |
| `doc_type` | — | `"tag"` |
| `is_tag_id` | `is_*` (long, single) | Integer tag ID |
| `ts_value` | `ts_*` (text, single) | Normalised tag string; full-text searchable |
| `created` | — | ISO-8601 UTC creation date |

### Response Documents

One flat document per submitted response. All answer values are denormalised
into the document to allow cross-question full-text search without Solr joins.

| Solr field | Type prefix | Description |
|---|---|---|
| `id` | — | `"response-{id}"` |
| `doc_type` | — | `"response"` |
| `is_response_id` | `is_*` (long) | Response primary key |
| `is_form_id` | `is_*` (long) | Parent form ID |
| `is_user_id` | `is_*` (long) | Respondent UID (0 = anonymous) |
| `ds_submitted_at` | `ds_*` (date, single) | Submission timestamp |
| `tm_answers` | `tm_*` (text, multi) | All text/textarea/tags answers merged — enables cross-answer keyword search |
| `tm_q_{qid}` | `tm_*` (text, multi) | Text / textarea / text_editor / tags answers per question |
| `sm_q_{qid}` | `sm_*` (string, multi) | Checkbox / select / tags answers (exact-match faceting) |
| `ss_q_{qid}` | `ss_*` (string, single) | Scalar answers (radio, number, date, etc.) |

Field naming follows the **apachesolr dynamic field** convention so the Solr
schema auto-types them without manual schema changes.

---

## Tag Indexing API

| Function | Description |
|---|---|
| `dynamic_form_solr_index_all($clear)` | (Re-)indexes all tags. Pass `TRUE` to wipe the index first. Returns count. |
| `dynamic_form_solr_index_tag($id, $value, $created_at)` | Index a single tag (call after insert/update). |
| `dynamic_form_solr_delete_tag($id)` | Remove a single tag document. |

---

## Response Indexing API

| Function | Description |
|---|---|
| `dynamic_form_solr_index_response($response_id)` | Index one submitted response. Idempotent — deletes the old doc first. Returns `FALSE` for drafts or missing rows. |
| `dynamic_form_solr_delete_response($response_id)` | Remove a single response document. |
| `dynamic_form_solr_delete_form_responses($form_id)` | Remove all response documents belonging to a form (used on permanent form deletion). |
| `dynamic_form_solr_index_all_responses()` | Full re-index of all submitted responses in batches of 50. Returns total indexed. |

Tag answers are sourced from `dynamic_form_tag_answers` (canonical) rather than
`dynamic_form_answers` (display fallback). The internal helper
`_dynamic_form_solr_inject_tag_answers()` merges these into the answer set
before building each response document.

---

## Response Filtering API

### `dynamic_form_solr_filter_responses($form_id, $criteria)`

Returns an array of matching response IDs, or:
- `NULL` — no criteria supplied; caller should use the database directly.
- `FALSE` — Solr is unreachable.

#### Supported criteria keys

| Key | Type | Description |
|---|---|---|
| `search` | string | Full-text keyword matched against `tm_answers` |
| `filters` | array | Per-question filters (see below) |
| `date_from` | string | `YYYY-MM-DD` — start of submission date range |
| `date_to` | string | `YYYY-MM-DD` — end of submission date range |
| `respondent` | string | `"registered"`, `"anonymous"`, or `""` (any) |

#### Per-question filter structure

```php
array(
  'question_id'   => 42,
  'value'         => 'php',
  'question_type' => 'tags',  // or text, select, checkbox, radio, …
  'op'            => 'AND',   // 'AND' (default) or 'OR' relative to previous filter
)
```

Filters within an `OR` run are grouped with `(clause1 OR clause2)` and then
AND-ed against the rest.

#### Solr field mapping per question type

| Question type | Solr field queried |
|---|---|
| `text`, `textarea`, `text_editor`, `tags` | `tm_q_{qid}` (full-text) |
| `checkbox` | `sm_q_{qid}:"value"` (exact multi-string) |
| `select` | `sm_q_{qid}:"v" OR ss_q_{qid}:"v"` (covers single and multi-select) |
| `radio`, `number`, `date`, and all others | `ss_q_{qid}:"value"` (exact scalar) |

---

## Facet API

### `dynamic_form_solr_get_response_facets($form_id, $criteria, $facet_qids)`

Returns facet value counts for the given question IDs, scoped to the same
criteria used for filtering. Useful for rendering filter bar option counts.

```php
// Returns: array( question_id => array(['value' => 'php', 'count' => 12], ...) )
$facets = dynamic_form_solr_get_response_facets(3, $criteria, [26, 31]);
```

Returns `FALSE` if Solr is unreachable, or an empty array per question when
no values are found. Facets are sorted by count descending, capped at 100.

---

## Tag Search API

| Function | Description |
|---|---|
| `dynamic_form_solr_search_tags($term, $limit)` | Prefix search on `ts_value`. Returns `[{id, text}, …]` for Select2. |
| `dynamic_form_solr_fuzzy_search_tags($term, $limit)` | Fuzzy (typo-tolerant) search. Fuzziness: 1 edit for 3–4 char terms, 2 edits for 5+. Never fails — returns empty array on error. |

The autocomplete endpoint in `dynamic_form_solr.pages.inc` calls both functions:
exact/prefix matches first, fuzzy suggestions appended if exact results are sparse.

---

## Admin UI

`admin/config/search/dynamic-form-solr` (in `dynamic_form_solr.admin.inc`) provides:

- **Solr URL field** — saves to `dynamic_form_solr_url`.
- **Re-index tags** button — triggers `dynamic_form_solr_index_all(TRUE)`.
- **Re-index responses** button — triggers `dynamic_form_solr_index_all_responses()`.
- Connection test — sends a `GET /select?q=*:*&rows=0` to verify reachability.

---

## Error Handling

All Solr HTTP calls go through `_dynamic_form_solr_request()`. Failures are
logged via `watchdog('dynamic_form_solr', …, WATCHDOG_ERROR)` and return `FALSE`.
Calling code checks the return value and falls back gracefully (e.g. returns all
response IDs unfiltered when Solr is unavailable).
