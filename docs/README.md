# Dynamic Form Builder — Documentation

A Drupal 7 platform for creating, managing, and collecting responses from dynamic online forms.

---

## Table of Contents

### Modules
- [dynamic_form module](module/dynamic_form_module.md) — core form builder, builder UI, permissions, dashboard
- [dynamic_form_response module](module/dynamic_form_response_module.md) — form submission, autosave, response dashboard

### Theme
- [dynamic_form theme](theme/dynamic_form_theme.md) — page templates, preprocess functions, assets

### Database
- [Schema reference](database/schema.md) — all tables, columns, and relationships

---

## Project Structure

```
sites/all/
├── modules/
│   ├── dynamic_form/               # Core form builder module
│   │   ├── dynamic_form.module
│   │   ├── dynamic_form.install
│   │   ├── dynamic_form.info
│   │   ├── css/                    # builder, dashboard, members, invitations, toast
│   │   ├── js/                     # builder, members, invitations, search, delete, toast
│   │   └── includes/               # 12 .inc files (auth, builder, dashboard, forms, …)
│   ├── dynamic_form_response/      # Form submission module
│   │   ├── dynamic_form_response.module
│   │   ├── css/
│   │   ├── js/
│   │   └── includes/               # submit + dashboard .inc
│   └── dynamic_form_i18n/          # Internationalisation support
└── themes/
    └── dynamic_form/               # Custom theme
        ├── template.php
        ├── css/style.css
        ├── js/main.js
        └── template/               # page.tpl.php, dashboard.tpl.php
```

---

## Architecture at a Glance

```
Form
 └── Sections  (ordered groups)
      └── Questions  (typed inputs)
           ├── Question Options  (for choice types)
           └── Question Validations  (rules + custom messages)

Response
 └── Answers  (one per question)
      └── Answer Files  (for file-upload questions)
```

---

## Data Layers

### Form Builder Layer

| Table | Purpose |
|---|---|
| `dynamic_form_forms` | Form definitions — title, slug, status, visibility, open/close timestamps |
| `dynamic_form_sections` | Ordered groups of questions within a form |
| `dynamic_form_questions` | Individual questions — type, label, width, ordering |
| `dynamic_form_question_options` | Answer choices for radio / checkbox / select questions |
| `dynamic_form_question_validations` | Validation rules (min/max length, regex, required, …) |
| `dynamic_form_members` | Per-form role assignments (owner, editor, viewer, reviewer, form_applicant) |
| `dynamic_form_invitations` | Token-based email invitations with 7-day expiry |
| `dynamic_form_tags` | Tag dictionary for the Tags question type |

### Form Submission Layer

| Table | Purpose |
|---|---|
| `dynamic_form_responses` | One row per submission attempt (tracks user, status, guest token) |
| `dynamic_form_answers` | One row per answer — maps a response to a question and its value |
| `dynamic_form_answer_files` | File metadata for file-upload answers |
| `dynamic_form_response_sessions` | Device / session metadata captured at submission time |
| `dynamic_form_tag_answers` | Normalised tag selections for Tags-type answers |

### Audit Layer

| Table | Purpose |
|---|---|
| `dynamic_form_audit_log` | Immutable append-only log of every create / edit / delete on forms, sections, and questions |

---

## Key Design Decisions

| Pattern | Detail |
|---|---|
| Soft delete | `deleted_at` / `deleted_by` columns on forms, sections, and questions |
| Audit trail | Every mutation writes a row to `dynamic_form_audit_log` with old and new JSON values |
| Email queue | Member notifications and invitations are queued via `DrupalQueue` and processed by `hook_cron` |
| Token invitations | Unregistered users receive a 7-day invite link; accepted after login/register via session redirect |
| Guest tracking | A `guest_token` cookie ties anonymous respondents to their in-progress response |
| AJAX feedback | Custom `dfbToast` AJAX command surfaces success/error toasts without page reload |
| jQuery version | jQuery 1.12.4 via the jQuery Update module — `.on()`, `.prop()`, and Select2 v4.x are all safe |

---

## Supported Question Types

| Type | Input |
|---|---|
| `text` | Single-line text |
| `textarea` | Multi-line text |
| `text_editor` | Rich text (TinyMCE) |
| `radio` | Single choice |
| `checkbox` | Multiple choice |
| `select` | Dropdown (Select2) |
| `file` | File upload with dropzone UI |
| `date` | Date picker |
| `time` | Time picker |
| `rating` | Star rating (configurable 1–N scale) |
| `linear_scale` | Likert / linear scale with min and max labels |
| `tags` | Tag input with autocomplete (Select2) |

---

## Permissions

| Permission | Description |
|---|---|
| `access dashboard` | Access the builder dashboard |
| `view forms` | View and fill out published forms |
| `create new form` | Create new forms |
| `edit any form` / `edit own form` | Edit forms |
| `delete any form` / `delete own form` | Soft-delete forms |
| `view soft delete` | Access the Trash page |
| `restore deleted` | Restore or permanently remove trashed items |
| `view activity log` | Read the audit log |
| `submit forms` | Submit form responses |
| `view any form responses` | View all responses |
| `view own form responses` | View own responses |
| `delete form responses` | Soft-delete responses |

---

## AJAX Endpoints

All endpoints return Drupal AJAX command arrays.

| Method | Path | Purpose |
|---|---|---|
| POST | `dynamic-form/ajax/builder/reorder` | Reorder sections and questions via drag-drop |
| GET | `dynamic-form/ajax/builder/question/%/edit-form` | Load question edit modal |
| POST | `dynamic-form/ajax/builder/section/%/rename` | Inline section rename |
| GET | `dynamic-form/ajax/members/search` | User search for member picker |
| POST | `dynamic-form/ajax/invitations/send` | Send email invitation |
| POST | `dynamic-form/ajax/invitations/%/resend` | Resend an invitation |
| POST | `dynamic-form/ajax/invitations/%/revoke` | Revoke an invitation |
| GET | `dynamic-form/ajax/invitations/%/list` | List invitations for a form |
| GET | `dynamic-form/ajax/tags/autocomplete` | Tag autocomplete suggestions |
| POST | `dynamic-form/ajax/trash/%/%/restore` | Restore a trashed entity |
| POST | `dynamic-form/ajax/delete/%/%` | Soft-delete an entity |
| POST | `dynamic-form-response/ajax/save` | Autosave an answer |
| POST | `dynamic-form-response/ajax/upload` | Upload a file answer |
| POST | `dynamic-form-response/ajax/submit` | Final form submission |

---

## Routes Reference

### Authentication

| Path | Description |
|---|---|
| `/login` | Sign-in page (anonymous only) |
| `/register` | Registration page (anonymous only) |

### Dashboard

| Path | Description |
|---|---|
| `/dashboard` | Overview |
| `/dashboard/forms` | My forms list |
| `/dashboard/forms/create` | Create form |
| `/dashboard/forms/%/edit` | Edit form settings |
| `/dashboard/forms/%/builder` | Visual form builder |
| `/dashboard/forms/%/preview` | Form preview |
| `/dashboard/forms/%/responses` | Response list |
| `/dashboard/forms/%/responses/%` | Response detail |
| `/dashboard/questions` | Questions list |
| `/dashboard/questions/%/edit` | Edit question |
| `/dashboard/questions/%/delete` | Delete question |
| `/dashboard/sections` | Sections list |
| `/dashboard/sections/create` | Create section |
| `/dashboard/sections/%/edit` | Edit section |
| `/dashboard/sections/%/delete` | Delete section |
| `/dashboard/browse` | Browse all public forms |
| `/dashboard/submissions` | Submissions by current user |
| `/dashboard/trash` | Trash (soft-deleted items) |
| `/dashboard/trash/%/%/restore` | Restore confirmation |
| `/dashboard/trash/%/%/delete-permanent` | Permanent delete confirmation |
| `/dashboard/activity-log` | Audit log |

### Public

| Path | Description |
|---|---|
| `/forms` | Public forms listing |
| `/forms/%` | Fill out a form (identified by slug) |
| `/forms/%/thank-you` | Post-submission thank-you page |

### Invitation Accept

| Path | Description |
|---|---|
| `/dynamic-form/invite/accept/%` | Accept an email invitation token |
