# Word Cloud — Implementation Documentation

## Overview

The word cloud visualizes tag-question answer frequency across form responses. It uses **d3-cloud** (Jason Davies' layout library) on top of **D3 v5** for rendering. The feature appears on:

- **Global analytics page** (`dashboard/analytics`) — all tags across all forms, with form + question filter dropdowns.
- **Per-form analytics page** (`dashboard/forms/{id}/analytics`) — tags scoped to that form, with a question filter dropdown.

---

## Library Dependencies

| Library | File | Load order |
|---|---|---|
| D3 v5 | `sites/all/libraries/d3/d3.min.js` | weight -10 (JS_LIBRARY group) |
| d3-cloud | `sites/all/libraries/d3-cloud/build/d3.layout.cloud.js` | weight -9 (JS_LIBRARY group) |

d3-cloud attaches itself to the global `d3.layout.cloud` namespace. It is loaded **after** D3 so `d3` is already defined when the cloud module initialises.

---

## Data Flow

```
MySQL tables
  dynamic_form_tag_answers (ta)
  dynamic_form_tags        (t)
  dynamic_form_responses   (r)
        │
        ▼
_dynamic_form_analytics_wordcloud_query($form_id, $question_id)
        │  returns [['text' => string, 'count' => int], ...]
        │  max 150 rows, ordered by count DESC
        ▼
drupal_add_js(array('dfAnalytics' => ['wordcloud' => ...]), 'setting')
        │  embedded in page as Drupal.settings.dfAnalytics.wordcloud.words
        ▼
renderWordCloud(el, words)          ← initial render (all forms)
        │
        ▼  (on filter change)
wcFetch(ajaxUrl, formId, questionId, mountEl)
        │  GET dashboard/analytics/wordcloud-data?form_id=N&question_id=N
        │  returns {"words": [...]}
        ▼
renderWordCloud(el, data.words)     ← re-render with filtered data
```

---

## Backend

### Files

| File | Responsibility |
|---|---|
| [includes/dynamic_form_analytics.wordcloud.inc](includes/dynamic_form_analytics.wordcloud.inc) | AJAX endpoints + data query helpers |
| [dynamic_form_analytics.module](dynamic_form_analytics.module) | `hook_menu()` route registration |
| [includes/dynamic_form_analytics.global.inc](includes/dynamic_form_analytics.global.inc) | Page callback that seeds `Drupal.settings` |

### API Routes

#### `GET dashboard/analytics/wordcloud-data`

Returns tag frequency data for the word cloud.

**Query parameters**

| Param | Type | Default | Description |
|---|---|---|---|
| `form_id` | int | `0` | Filter to a single form. `0` = all forms. |
| `question_id` | int | `0` | Filter to a single tag question. `0` = all tag questions within the selected form. |

**Response**

```json
{
  "words": [
    { "text": "javascript", "count": 42 },
    { "text": "react",      "count": 27 }
  ]
}
```

Words are sorted by `count DESC`, capped at **150 entries**. Only responses with `status = 1` (submitted) are included. Soft-deleted responses (`deleted_at IS NOT NULL`) are excluded.

#### `GET dashboard/analytics/wordcloud-questions/{form_id}`

Returns all tag-type questions for the given form (for populating the question filter dropdown).

**Response**

```json
{
  "questions": [
    { "id": 26, "label": "Skills" },
    { "id": 31, "label": "Interests" }
  ]
}
```

Only questions with `type = 'tags'` and no `deleted_at` are returned, ordered by `section_id` then `position`.

### Database Query

```php
// _dynamic_form_analytics_wordcloud_query()
db_select('dynamic_form_tag_answers', 'ta')
  ->join('dynamic_form_tags',      't',  't.id = ta.tag_id')
  ->join('dynamic_form_responses', 'r',  'r.id = ta.response_id')
  ->addField('t', 'standardized_value', 'text')
  ->addExpression('COUNT(*)', 'count')
  ->condition('r.status', 1)
  // optional: ->condition('r.form_id',     $form_id)
  // optional: ->condition('ta.question_id', $question_id)
  ->groupBy('ta.tag_id')
  ->orderBy('count', 'DESC')
  ->range(0, 150)
```

`t.standardized_value` is used (not `t.name`) so that tag aliases and normalised spellings are collapsed into a single entry per canonical tag.

---

## Frontend

### `renderWordCloud(el, words)`

**Location:** [js/analytics.js:395](js/analytics.js#L395)

Renders the word cloud into a DOM element using d3-cloud.

**Parameters**

| Param | Type | Description |
|---|---|---|
| `el` | `HTMLElement` | Mount point. The function clears its contents before rendering. |
| `words` | `Array<{text: string, count: number}>` | Tag data sorted by count DESC. |

**Behaviour**

1. Clears `el` with `d3.select(el).selectAll('*').remove()`.
2. If `words` is empty or absent, inserts a `.dfb-analytics-empty` div with a "No tag data" message and returns early.
3. Computes a **square-root font scale** (`d3.scaleSqrt`) mapping `[minCount, maxCount]` to `[12px, 48px]`. When all counts are equal the font is fixed at `24px`.
4. Assigns a colour from the `COLORS` palette (`['#4f6ef7', '#22c55e', '#f59e0b', …]`) cycling by word index.
5. Invokes `d3.layout.cloud()` with:
   - **size** `[W − 60, H − 60]` (canvas padded 30px each side).
   - **padding** `5` px between words.
   - **rotate** `0` — all words are horizontal (no rotation).
   - **font** `'sans-serif'`.
6. On the `'end'` event, renders each placed word as an SVG `<text>` element centred in the SVG (`translate(W/2, H/2)`), positioned using the `(d.x, d.y)` coordinates computed by d3-cloud.
7. Attaches a **tooltip** (`showTooltip`) on `mouseover` showing `"<strong>word</strong>: N uses"`.

**SVG layout**

```
SVG  W × 300px
└── <g> translate(W/2, 150)   ← origin at centre
    └── <text> per word
          transform="translate(d.x, d.y) rotate(d.rotate)"
          font-size="${d.size}px"
          fill="${d.color}"
```

### `wcFetch(ajaxUrl, formId, questionId, mountEl)`

**Location:** [js/analytics.js:469](js/analytics.js#L469)

Fetches filtered word cloud data from the backend and re-renders.

```
wcFetch(ajaxUrl, formId, questionId, mountEl)
  ↓ shows loading spinner
  ↓ d3.json(ajaxUrl + '?form_id=N&question_id=N')   [D3 v5 Promise API]
  ↓ on success → renderWordCloud(mountEl, data.words)
  ↓ on error   → shows .dfb-analytics-empty "Could not load" message
```

### Drupal Behaviour wiring

**Location:** [js/analytics.js:490](js/analytics.js#L490)

`Drupal.behaviors.dfAnalytics.attach` reads `Drupal.settings.dfAnalytics` and wires up:

#### Global word cloud (`settings.dfAnalytics.wordcloud`)

| Setting key | Type | Description |
|---|---|---|
| `words` | `Array` | Initial word list (all forms). |
| `forms` | `Array<{id, title}>` | Form options for the `#dfa-wc-form` `<select>`. |
| `ajaxData` | `string` | URL for `wordcloud-data` endpoint. |
| `ajaxQuestions` | `string` | Base URL for `wordcloud-questions/{id}` endpoint. |

DOM element: `#dfa-chart-wordcloud`

Filter flow:

```
#dfa-wc-form change
  ├─ fid === 0  → renderWordCloud(el, wc.words)          (restore global)
  └─ fid > 0   → $.getJSON(ajaxQuestions/fid)           (populate #dfa-wc-question)
                  wcFetch(ajaxData, fid, 0, el)          (fetch form-scoped cloud)

#dfa-wc-question change
  └─ wcFetch(ajaxData, fid, qid, el)                    (fetch question-scoped cloud)
```

#### Per-form word cloud (`settings.dfAnalytics.wordcloudForm`)

| Setting key | Type | Description |
|---|---|---|
| `words` | `Array` | Initial word list for this form. |
| `questions` | `Array<{id, label}>` | Question options for `#dfa-wc-question-form`. |
| `formId` | `int` | The current form's node ID. |
| `ajaxData` | `string` | URL for `wordcloud-data` endpoint. |

DOM element: `#dfa-chart-wordcloud-form`

Filter flow:

```
#dfa-wc-question-form change
  └─ wcFetch(wcf.ajaxData, wcf.formId, qid, el)
```

---

## CSS Classes

| Class | Element | Purpose |
|---|---|---|
| `.dfb-analytics-empty` | `<div>` | Shown when no word data is available. |
| `.dfb-wc-loading` | `<div>` | Shown during AJAX fetch. |
| `.dfa-tooltip` | `<div>` | Shared tooltip injected into `<body>`. |

---

## Adding a New Filter Dimension

To add a third filter (e.g. date range):

1. Add a query parameter to `dynamic_form_analytics_wordcloud_data()` in [dynamic_form_analytics.wordcloud.inc](includes/dynamic_form_analytics.wordcloud.inc).
2. Pass it through `_dynamic_form_analytics_wordcloud_query()`.
3. Add the DOM `<select>` in the relevant page callback HTML.
4. Wire a `change` handler in `Drupal.behaviors.dfAnalytics.attach` that calls `wcFetch()` with the extra param appended to the URL.
