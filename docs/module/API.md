# Dynamic Form REST API — v1

Read-only REST API for fetching form responses and answers. All endpoints return JSON and require HTTP Basic Auth.

---

## Base URL

```
http://localhost/dynamic-form-builder/api/v1
```

---

## Authentication

The API uses **HTTP Basic Auth**. Every request must include an `Authorization` header with base64-encoded `username:password` credentials.

```
Authorization: Basic base64(username:password)
```

### Requirements

The authenticated Drupal account must have **both** of the following permissions:

| Permission | Where to grant |
|---|---|
| `Access Dynamic Form REST API` | `admin/people/permissions` |
| `view any form responses` or `view own form responses` | `admin/people/permissions` |

- **`view any form responses`** — can access responses for any form.
- **`view own form responses`** — can only access responses for forms they created or are a member of (owner/editor role).

### Security Note

Basic Auth transmits credentials on every request. Always use **HTTPS** in production to prevent credential exposure.

---

## Endpoints

### 1. List Responses

Returns a paginated list of submitted responses for a form, without answer details.

```
GET /api/v1/forms/{form_id}/responses
```

#### URL Parameters

| Parameter | Type | Description |
|---|---|---|
| `form_id` | integer | The form's primary key ID |

#### Query Parameters

| Parameter | Type | Default | Constraints | Description |
|---|---|---|---|---|
| `page` | integer | `1` | Min: 1 | Page number (1-indexed) |
| `per_page` | integer | `25` | Min: 1, Max: 100 | Results per page |

#### Example Request

```
GET /api/v1/forms/3/responses?page=1&per_page=10
Authorization: Basic dXNlcm5hbWU6cGFzc3dvcmQ=
```

#### Example Response — 200 OK

```json
{
  "api_version": "v1",
  "data": [
    {
      "id": 12,
      "form_id": 3,
      "respondent": {
        "type": "registered",
        "uid": 5,
        "name": "john",
        "email": "john@example.com"
      },
      "status": "submitted",
      "submitted_at": "2026-05-20T10:30:00+00:00",
      "created_at": "2026-05-20T10:28:00+00:00",
      "updated_at": "2026-05-20T10:30:00+00:00"
    }
  ],
  "meta": {
    "form_id": 3,
    "form_title": "Customer Survey",
    "total": 42,
    "page": 1,
    "per_page": 10,
    "total_pages": 5
  }
}
```

#### Respondent Types

| `type` | Description |
|---|---|
| `registered` | Logged-in Drupal user. `uid`, `name`, and `email` are populated. |
| `guest` | Submitted with an email but no account. Only `email` is populated. |
| `anonymous` | Submitted with no identifying information. All fields are `null`. |

---

### 2. Get Response Detail

Returns a single response including all answers grouped by question.

```
GET /api/v1/forms/{form_id}/responses/{response_id}
```

#### URL Parameters

| Parameter | Type | Description |
|---|---|---|
| `form_id` | integer | The form's primary key ID |
| `response_id` | integer | The response's primary key ID |

#### Access Rules

- Users with `view any form responses` can access any response on any form.
- Users with `view own form responses` can access:
  - All responses on forms they created or are an owner/editor of.
  - Their own response only, on all other forms.

#### Example Request

```
GET /api/v1/forms/3/responses/12
Authorization: Basic dXNlcm5hbWU6cGFzc3dvcmQ=
```

#### Example Response — 200 OK

```json
{
  "api_version": "v1",
  "data": {
    "id": 12,
    "form_id": 3,
    "form_title": "Customer Survey",
    "respondent": {
      "type": "registered",
      "uid": 5,
      "name": "john",
      "email": "john@example.com"
    },
    "status": "submitted",
    "submitted_at": "2026-05-20T10:30:00+00:00",
    "created_at": "2026-05-20T10:28:00+00:00",
    "updated_at": "2026-05-20T10:30:00+00:00",
    "answers": [
      {
        "question_id": 1,
        "question_label": "Full Name",
        "question_type": "text",
        "value": "John Doe"
      },
      {
        "question_id": 2,
        "question_label": "Preferred Contact",
        "question_type": "select",
        "value": ["email", "phone"]
      },
      {
        "question_id": 3,
        "question_label": "Resume",
        "question_type": "file",
        "value": {
          "fid": 88,
          "filename": "resume.pdf",
          "uri": "public://responses/resume.pdf"
        }
      },
      {
        "question_id": 4,
        "question_label": "Comments",
        "question_type": "textarea",
        "value": null
      }
    ]
  }
}
```

#### Answer Value Types

| Question type | `value` type | Notes |
|---|---|---|
| `text`, `textarea`, `email`, `number`, `date` | `string` or `null` | Plain string |
| `select`, `checkbox`, `tags` | `array` or `null` | JSON array of selected values |
| `radio` | `string` or `null` | Single selected value |
| `file` | `object` or `null` | `{ fid, filename, uri }` |

Unanswered questions are included with `"value": null`.

---

## Error Reference

All errors follow the same envelope:

```json
{
  "api_version": "v1",
  "error": {
    "code": 403,
    "message": "You do not have permission to view responses for this form."
  }
}
```

| Status | Meaning | Common Cause |
|---|---|---|
| `401 Unauthorized` | Authentication failed | Missing/wrong credentials, or account lacks `access rest api` permission |
| `403 Forbidden` | Authenticated but not authorized | Account lacks response-viewing permission for this form |
| `404 Not Found` | Resource does not exist | Invalid `form_id` / `response_id`, or soft-deleted |
| `405 Method Not Allowed` | Wrong HTTP method | Only `GET` is supported |

---

## Postman Setup

1. Create a new request and set the method to **GET**.
2. Enter the full URL, e.g. `http://localhost/dynamic-form-builder/api/v1/forms/3/responses`.
3. Go to the **Authorization** tab → select **Basic Auth**.
4. Enter your Drupal **Username** and **Password**.
5. Optionally add `page` and `per_page` in the **Params** tab.
6. Click **Send**.

### Finding your `form_id`

The form ID appears in the dashboard URL when you open a form:
```
http://localhost/dynamic-form-builder/dashboard/forms/3
                                                       ^
                                                    form_id = 3
```

---

## Module Files

| File | Purpose |
|---|---|
| `dynamic_form_restws.module` | Hook implementations, authentication, JSON helpers |
| `includes/dynamic_form_restws.v1.inc` | Page callbacks for both endpoints |
| `dynamic_form_restws.info` | Module metadata and dependencies |
