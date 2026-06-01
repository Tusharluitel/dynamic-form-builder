# Dynamic Form Analytics Module Documentation

## Overview

The `dynamic_form_analytics` module renders D3-powered visualisations of form
response data. It provides a global analytics dashboard, a per-form analytics
page, a word cloud for tag-type questions, and a radar chart for numeric/choice
questions.

| Property | Value |
|---|---|
| **Module name** | `dynamic_form_analytics` |
| **Drupal core** | 7.x |
| **Depends on** | `dynamic_form`, `dynamic_form_response` |

For detailed word cloud implementation see [word_cloud.md](word_cloud.md).

---

## File Structure

```
sites/all/modules/dynamic_form_analytics/
├── dynamic_form_analytics.info
├── dynamic_form_analytics.module        # Permissions, routes, sidebar alter
├── includes/
│   ├── dynamic_form_analytics.global.inc  # Global analytics page (all forms)
│   ├── dynamic_form_analytics.form.inc    # Per-form analytics page
│   ├── dynamic_form_analytics.wordcloud.inc # Word cloud AJAX + data helpers
│   └── dynamic_form_analytics.radar.inc   # Radar chart page + AJAX
├── css/
│   └── analytics.css
└── js/
    └── analytics.js    # D3 charts: bar, pie, word cloud, radar
```

---

## Hooks Implemented

| Hook | Purpose |
|---|---|
| `hook_permission` | Declares `view form analytics` permission |
| `hook_menu` | Registers analytics page routes and AJAX endpoints |
| `hook_dynamic_form_dashboard_sidebar_items_alter` | Injects Analytics into sidebar nav (top position) |

---

## Permission

| Permission | Description |
|---|---|
| `view form analytics` | Access the analytics dashboard with aggregated response charts |

---

## Routes

### Pages

| Path | Callback | Access |
|---|---|---|
| `dashboard/analytics` | `dynamic_form_analytics_global_page` | `view form analytics` |
| `dashboard/forms/%/analytics` | `dynamic_form_analytics_form_page` | `_dynamic_form_edit_access` |
| `dashboard/analytics/radar` | `dynamic_form_analytics_radar_page` | `view form analytics` |

### AJAX / Data Endpoints

| Path | Callback | Access |
|---|---|---|
| `dashboard/analytics/wordcloud-data` | `dynamic_form_analytics_wordcloud_data` | `view form analytics` |
| `dashboard/analytics/wordcloud-questions/%` | `dynamic_form_analytics_wordcloud_questions` | `view form analytics` |
| `dashboard/analytics/wordcloud-tag-forms` | `dynamic_form_analytics_wordcloud_tag_forms` | `view form analytics` |
| `dashboard/analytics/radar-data` | `dynamic_form_analytics_radar_ajax` | `view form analytics` |

---

## Dashboard Pages

### Global Analytics (`dashboard/analytics`)

Rendered by `dynamic_form_analytics_global_page` in `dynamic_form_analytics.global.inc`.

Displays aggregated statistics across **all** forms the current user can access:
- Response counts and submission trends (bar / line charts).
- Word cloud for all tag-type questions across all forms.
- Filter dropdowns: form selector, question selector.

Initial data is embedded in the page as `Drupal.settings.dfAnalytics`. Filter
changes fetch updated data via the AJAX endpoints.

### Per-Form Analytics (`dashboard/forms/%/analytics`)

Rendered by `dynamic_form_analytics_form_page` in `dynamic_form_analytics.form.inc`.

Scoped to a single form. Shows:
- Per-question breakdown charts (bar for choice questions, line for date trends).
- Word cloud scoped to the form's tag questions.
- Question filter dropdown.

Access uses `_dynamic_form_edit_access` — the same gate as the form builder.

### Radar Chart (`dashboard/analytics/radar`)

Rendered by `dynamic_form_analytics_radar_page` in `dynamic_form_analytics.radar.inc`.

Spider/radar chart showing response distribution across multi-choice or numeric
questions. Supports a form-filter dropdown. Data is loaded via
`dashboard/analytics/radar-data` (JSON).

---

## Frontend (analytics.js)

The entire charting layer lives in `js/analytics.js` and uses **D3 v5** (loaded
from `sites/all/libraries/d3/d3.min.js`).

### Key functions

| Function | Location | Purpose |
|---|---|---|
| `renderWordCloud(el, words)` | ~L395 | Renders d3-cloud word cloud |
| `wcFetch(ajaxUrl, formId, questionId, mountEl)` | ~L469 | AJAX-fetches filtered word cloud data |
| `Drupal.behaviors.dfAnalytics.attach` | ~L490 | Wires filter dropdowns to chart functions |

See [word_cloud.md](word_cloud.md) for the full word cloud specification.

### D3 Library Loading Order

| Library | Path | Weight |
|---|---|---|
| D3 v5 | `sites/all/libraries/d3/d3.min.js` | −10 (JS_LIBRARY) |
| d3-cloud | `sites/all/libraries/d3-cloud/build/d3.layout.cloud.js` | −9 (JS_LIBRARY) |

d3-cloud attaches to `d3.layout.cloud` — D3 must be defined first.

---

## Sidebar Integration

The module implements `hook_dynamic_form_dashboard_sidebar_items_alter` to inject
an **Analytics** entry at the top of the sidebar navigation, gated by
`view form analytics`:

```php
$items = array('analytics' => array(
  'label' => t('Analytics'),
  'path'  => 'dashboard/analytics',
  'icon'  => '<i class="fa-solid fa-chart-line"></i>',
)) + $items;
```

Placing it first in the array ensures it appears above other sidebar items.

---

## Data Sources

All charts read from the following tables (submitted, non-deleted responses only):

| Table | Used for |
|---|---|
| `dynamic_form_responses` | Response counts, date trends, respondent breakdown |
| `dynamic_form_answers` | Per-question value distribution |
| `dynamic_form_tag_answers` + `dynamic_form_tags` | Word cloud frequency (`standardized_value`) |
| `dynamic_form_questions` | Question labels and types for axis labels |

Queries filter `r.status = 1` and `r.deleted_at IS NULL` throughout.
