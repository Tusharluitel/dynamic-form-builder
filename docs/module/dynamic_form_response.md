# Dynamic Form Response Module Documentation

## Overview

The `dynamic_form_response` module handles the full lifecycle of a form submission:
public-facing form rendering, per-answer AJAX autosave, file upload, final submit,
a thank-you page, and a dashboard for owners to review responses.

| Property | Value |
|---|---|
| **Module name** | `dynamic_form_response` |
| **Drupal core** | 7.x |
| **Depends on** | `dynamic_form` |

---

## File Structure

```
sites/all/modules/dynamic_form_response/
├── dynamic_form_response.info
├── dynamic_form_response.module      # Hooks, permissions, routes, access callbacks
├── includes/
│   ├── dynamic_form_response.submit.inc    # AJAX save/upload/submit, thank-you page
│   └── dynamic_form_response.dashboard.inc # Response list, detail view, submissions page
├── css/
│   ├── response-form.css
│   └── response-list.css
└── js/
    ├── response-form.js    # AJAX save, file dropzone, pagination
    └── response-filter.js  # Response list filter bar
```

---

## Hooks Implemented

| Hook | Purpose |
|---|---|
| `hook_entity_info` | Registers `dynamic_form_response` and `dynamic_form_answer` entity types for restws |
| `hook_entity_property_info` | Declares typed properties for restws serialisation |
| `hook_permission` | Declares 5 response-specific permissions |
| `hook_menu` | Registers submission AJAX routes and dashboard pages |
| `hook_init` | Loads CSS on public form pages and response dashboard pages |

---

## Permissions

| Permission | Description |
|---|---|
| `submit forms` | Fill out and submit dynamic forms |
| `view any form responses` | View submissions for any form regardless of ownership |
| `view own form responses` | View submissions for forms the user created or edits |
| `delete form responses` | Soft-delete submitted responses |
| `view response panel` | View the response panel |

---

## Routes

### Public Submission

| Path | Callback | Access |
|---|---|---|
| `dynamic-form-response/ajax/save` | `dynamic_form_response_ajax_save` | Everyone (open) |
| `dynamic-form-response/ajax/upload` | `dynamic_form_response_ajax_upload` | Everyone (open) |
| `dynamic-form-response/ajax/guest-email` | `dynamic_form_response_ajax_guest_email` | Everyone (open) |
| `dynamic-form-response/ajax/submit` | `dynamic_form_response_ajax_submit` | Everyone (open) |
| `forms/%/thank-you` | `dynamic_form_response_thankyou_page` | Everyone (open) |

### Dashboard

| Path | Callback | Access |
|---|---|---|
| `dashboard/forms/%/responses` | `dynamic_form_response_list_page` | `_dynamic_form_response_view_access` |
| `dashboard/forms/%/responses/%` | `dynamic_form_response_view_page` | `_dynamic_form_response_detail_access` |
| `dashboard/submissions` | `dynamic_form_response_submissions_page` | Logged-in |
| `dynamic-form-response/ajax/filter-options` | `dynamic_form_response_ajax_filter_options` | Logged-in |

---

## Submission Flow

```
1. User opens  forms/{slug}
2. response-form.js auto-creates a draft response on first keystroke
      POST  dynamic-form-response/ajax/save
            → upserts dynamic_form_responses (status = 0 / draft)
            → upserts dynamic_form_answers

3. (Optional) Upload file question
      POST  dynamic-form-response/ajax/upload
            → saves file via Drupal file API
            → records in dynamic_form_answer_files

4. (Optional) Anonymous user provides email
      POST  dynamic-form-response/ajax/guest-email
            → validates email, stores in dynamic_form_responses.guest_email

5. User clicks Submit
      POST  dynamic-form-response/ajax/submit
            → validates all required questions server-side
            → sets status = 1 (submitted), submitted_at = now()
            → redirects browser to  forms/{slug}/thank-you
```

All AJAX endpoints return JSON. Validation errors return an array of field-level
error messages; the JS renders them inline without a page reload.

---

## Entity Types (restws)

Both types use `DynamicFormSoftDeleteEntityController` and are read-only via the API.

| Entity type | Base table | URL pattern |
|---|---|---|
| `dynamic_form_response` | `dynamic_form_responses` | `/dynamic_form_response/{id}.json` |
| `dynamic_form_answer` | `dynamic_form_answers` | `/dynamic_form_answer/{id}.json` |

### Properties — `dynamic_form_response`

| Field | Type |
|---|---|
| `id` | integer |
| `form_id` | integer |
| `user_id` | integer (0 for guest/anonymous) |
| `guest_email` | text |
| `status` | integer (0 = draft, 1 = submitted) |
| `submitted_at` | date |
| `created_at` | date |
| `updated_at` | date |

### Properties — `dynamic_form_answer`

| Field | Type |
|---|---|
| `id` | integer |
| `response_id` | integer |
| `question_id` | integer |
| `value` | text (JSON for multi-value types) |
| `created_at` | date |
| `updated_at` | date |

---

## Access Callbacks

### `_dynamic_form_response_view_access($form_id)`

Used by the response list page. Grants access to:
- Users with `view any form responses`.
- The form creator.
- Members with the `owner` or `editor` role on the form.

### `_dynamic_form_response_detail_access($form_id, $response_id)`

Extends the list check. Additionally grants access to the respondent who
submitted the specific response (`responses.user_id == current uid`).

---

## Response Statuses

| Status value | Meaning |
|---|---|
| `0` | Draft — autosaved but not submitted |
| `1` | Submitted |

Soft-deleted responses have a non-null `deleted_at` and are hidden from all
public and API views by `DynamicFormSoftDeleteEntityController`.

---

## Respondent Types

| Condition | Type label in API |
|---|---|
| `user_id > 0` | `registered` — logged-in Drupal user |
| `user_id = 0` and `guest_email` set | `guest` — email provided, no account |
| `user_id = 0` and `guest_email` empty | `anonymous` |

---

## CSS Loading

`hook_init` adds `css/response-form.css` on any path that starts with
`forms/` or `dashboard/forms/`. No explicit `drupal_add_css()` calls are needed
in individual page callbacks.
