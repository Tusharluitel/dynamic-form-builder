<?php

/**
 * @file
 * Dashboard page callbacks and rendering functions for the Dynamic Form module.
 *
 * Contains the main dashboard page callback, sidebar builder, and individual
 * panel renderers for overview, forms, questions, and sections.
 */

/* =====================================================================
   DASHBOARD — PAGE CALLBACK
   ===================================================================== */

/**
 * Page callback for /dashboard and /dashboard/*.
 *
 * @param string $section
 *   The active dashboard section: overview, forms, questions, or sections.
 *
 * @return string
 *   Rendered dashboard HTML via the dynamic_form_dashboard theme hook.
 */
function dynamic_form_dashboard_page($section = 'overview')
{
  drupal_set_title(t('Dashboard'));

  // Load Select2 + table-search assets on listing pages.
  if (in_array($section, array('forms', 'questions', 'sections'))) {
    $module_path  = drupal_get_path('module', 'dynamic_form');
    drupal_add_css($module_path . '/css/dashboard-search.css');
    drupal_add_js($module_path . '/js/dashboard-search.js');
    drupal_add_js(array('dynamicFormSearch' => array('section' => $section)), 'setting');
    drupal_add_css($module_path . '/css/dashboard-delete.css');
    drupal_add_js($module_path . '/js/dashboard-delete.js');
  }

  // Build sidebar.
  $sidebar_html = _dynamic_form_dashboard_sidebar($section);

  // Build content for the active section.
  switch ($section) {
    case 'forms':
      $content_html = _dynamic_form_dashboard_forms();
      break;

    case 'questions':
      $content_html = _dynamic_form_dashboard_questions();
      break;

    case 'sections':
      $content_html = _dynamic_form_dashboard_sections();
      break;

    case 'overview':
    default:
      $content_html = _dynamic_form_dashboard_overview();
      break;
  }

  // Append delete-confirmation modal to listing pages.
  if (in_array($section, array('forms', 'questions', 'sections'))) {
    $content_html .= _dynamic_form_delete_confirm_modal();
  }

  return theme('dynamic_form_dashboard', array(
    'sidebar' => $sidebar_html,
    'content' => $content_html,
  ));
}

/* =====================================================================
   DASHBOARD — SIDEBAR
   ===================================================================== */

/**
 * Builds the sidebar navigation HTML for the dashboard.
 *
 * @param string $active
 *   The currently active section key.
 *
 * @return string
 *   Rendered sidebar HTML.
 */
function _dynamic_form_dashboard_sidebar($active = 'overview')
{
  $items = array(
    'overview' => array(
      'label' => t('Overview'),
      'path' => 'dashboard',
      'icon' => '&#9783;',
    ),
    'forms' => array(
      'label' => t('Forms'),
      'path' => 'dashboard/forms',
      'icon' => '&#128196;',
    ),
    'questions' => array(
      'label' => t('Questions'),
      'path' => 'dashboard/questions',
      'icon' => '&#10067;',
    ),
    'sections' => array(
      'label' => t('Sections'),
      'path' => 'dashboard/sections',
      'icon' => '&#128193;',
    ),
    'browse' => array(
      'label' => t('Browse Forms'),
      'path' => 'dashboard/browse',
      'icon' => '&#128269;',
    ),
    'submissions' => array(
      'label' => t('My Submissions'),
      'path' => 'dashboard/submissions',
      'icon' => '&#128221;',
    ),
  );

  drupal_alter('dynamic_form_dashboard_sidebar_items', $items);

  $output = '<nav class="dfb-sidebar-nav">';
  $output .= '<div class="dfb-sidebar-header">';
  $output .= '<span class="dfb-sidebar-icon">&#9881;</span>';
  $output .= '<span class="dfb-sidebar-title">' . t('Manage') . '</span>';
  $output .= '</div>';
  $output .= '<ul class="dfb-sidebar-menu">';

  foreach ($items as $key => $item) {
    $classes = 'dfb-sidebar-item dfb-sidebar-item--' . $key;
    if ($key === $active) {
      $classes .= ' active';
    }
    $output .= '<li class="' . $classes . '">';
    $output .= '<a href="' . url($item['path']) . '">';
    $output .= '<span class="dfb-sidebar-item-icon">' . $item['icon'] . '</span>';
    $output .= '<span class="dfb-sidebar-item-label">' . $item['label'] . '</span>';
    $output .= '</a>';
    $output .= '</li>';
  }

  if (user_access('view activity log')) {
    $log_classes = 'dfb-sidebar-item dfb-sidebar-item--activity-log';
    if ($active === 'activity-log') {
      $log_classes .= ' active';
    }
    $output .= '<li class="' . $log_classes . '">';
    $output .= '<a href="' . url('dashboard/activity-log') . '">';
    $output .= '<span class="dfb-sidebar-item-icon">&#128203;</span>';
    $output .= '<span class="dfb-sidebar-item-label">' . t('Activity Log') . '</span>';
    $output .= '</a>';
    $output .= '</li>';
  }

  if (user_access('view soft delete')) {
    $trash_classes = 'dfb-sidebar-item dfb-sidebar-item--trash';
    if ($active === 'trash') {
      $trash_classes .= ' active';
    }
    $output .= '<li class="' . $trash_classes . '">';
    $output .= '<a href="' . url('dashboard/trash') . '">';
    $output .= '<span class="dfb-sidebar-item-icon">&#128465;</span>';
    $output .= '<span class="dfb-sidebar-item-label">' . t('Trash') . '</span>';
    $output .= '</a>';
    $output .= '</li>';
  }

  $output .= '</ul>';

  // Quick action button.
  $output .= '<div class="dfb-sidebar-action">';
  $output .= '<a href="' . url('dashboard/forms/create') . '" class="dfb-sidebar-create-btn">';
  $output .= '<span>+</span> ' . t('New Form');
  $output .= '</a>';
  $output .= '</div>';

  $output .= '</nav>';

  return $output;
}

/* =====================================================================
   DASHBOARD — OVERVIEW PANEL
   ===================================================================== */

/**
 * Renders the dashboard overview panel with summary statistics.
 *
 * @return string
 *   Rendered overview HTML.
 */
function _dynamic_form_dashboard_overview()
{
  global $user;

  // Build conditions for user-scoped queries.
  $is_admin = user_access('edit any form');

  // Count forms.
  $forms_query = db_select('dynamic_form_forms', 'f')
    ->condition('f.deleted_at', NULL, 'IS NULL');
  if (!$is_admin) {
    $forms_query->condition('f.created_by', $user->uid);
  }
  $forms_query->addExpression('COUNT(*)', 'cnt');
  $total_forms = (int) $forms_query->execute()->fetchField();

  // Count published forms.
  $published_query = db_select('dynamic_form_forms', 'f')
    ->condition('f.deleted_at', NULL, 'IS NULL')
    ->condition('f.status', 1);
  if (!$is_admin) {
    $published_query->condition('f.created_by', $user->uid);
  }
  $published_query->addExpression('COUNT(*)', 'cnt');
  $published_forms = (int) $published_query->execute()->fetchField();

  // Count questions.
  $questions_query = db_select('dynamic_form_questions', 'q')
    ->condition('q.deleted_at', NULL, 'IS NULL');
  if (!$is_admin) {
    $questions_query->condition('q.created_by', $user->uid);
  }
  $questions_query->addExpression('COUNT(*)', 'cnt');
  $total_questions = (int) $questions_query->execute()->fetchField();

  // Count sections.
  $sections_query = db_select('dynamic_form_sections', 's')
    ->condition('s.deleted_at', NULL, 'IS NULL');
  if (!$is_admin) {
    $sections_query->condition('s.created_by', $user->uid);
  }
  $sections_query->addExpression('COUNT(*)', 'cnt');
  $total_sections = (int) $sections_query->execute()->fetchField();

  // Build stats cards.
  $output = '<div class="dfb-dashboard-header">';
  $output .= '<h2>' . t('Overview') . '</h2>';
  $output .= '<p class="dfb-dashboard-subtitle">' . t('Your form builder at a glance') . '</p>';
  $output .= '</div>';

  $output .= '<div class="dfb-stats-grid">';

  $output .= _dynamic_form_stat_card(t('Total Forms'), $total_forms, '&#128196;', 'primary');
  $output .= _dynamic_form_stat_card(t('Published'), $published_forms, '&#9989;', 'success');
  $output .= _dynamic_form_stat_card(t('Questions'), $total_questions, '&#10067;', 'accent');
  $output .= _dynamic_form_stat_card(t('Sections'), $total_sections, '&#128193;', 'warning');

  $output .= '</div>';

  // Recent forms.
  $recent_query = db_select('dynamic_form_forms', 'f')
    ->fields('f', array('id', 'title', 'status', 'updated_at'))
    ->condition('f.deleted_at', NULL, 'IS NULL')
    ->orderBy('f.updated_at', 'DESC')
    ->range(0, 5);
  if (!$is_admin) {
    $recent_query->condition('f.created_by', $user->uid);
  }
  $recent_forms = $recent_query->execute()->fetchAll();

  $output .= '<div class="dfb-dashboard-section">';
  $output .= '<h3>' . t('Recent Activity') . '</h3>';

  if (!empty($recent_forms)) {
    $status_labels = array(
      0 => '<span class="dfb-badge dfb-badge-draft">' . t('Draft') . '</span>',
      1 => '<span class="dfb-badge dfb-badge-published">' . t('Published') . '</span>',
      2 => '<span class="dfb-badge dfb-badge-closed">' . t('Closed') . '</span>',
    );

    $header = array(t('Form'), t('Status'), t('Last Updated'), t('Actions'));
    $rows = array();

    foreach ($recent_forms as $form) {
      $rows[] = array(
        check_plain($form->title),
        array('data' => isset($status_labels[$form->status]) ? $status_labels[$form->status] : t('Unknown'), 'class' => array('status-cell')),
        format_date($form->updated_at, 'short'),
        l(t('Edit'), 'dashboard/forms/' . $form->id . '/edit'),
      );
    }

    $output .= theme('table', array(
      'header' => $header,
      'rows' => $rows,
      'attributes' => array('class' => array('dfb-dashboard-table')),
    ));
  }
  else {
    $output .= '<div class="dfb-empty-state">';
    $output .= '<p>' . t('No forms yet. <a href="!url">Create your first form</a> to get started.', array('!url' => url('dashboard/forms/create'))) . '</p>';
    $output .= '</div>';
  }

  $output .= '</div>';

  return $output;
}

/**
 * Helper: Builds a single stat card.
 *
 * @param string $label
 *   The stat label.
 * @param int $value
 *   The stat value.
 * @param string $icon
 *   An HTML entity icon.
 * @param string $variant
 *   Color variant: primary, success, accent, warning.
 *
 * @return string
 *   Rendered stat card HTML.
 */
function _dynamic_form_stat_card($label, $value, $icon, $variant = 'primary')
{
  $output = '<div class="dfb-stat-card dfb-stat-' . $variant . '">';
  $output .= '<div class="dfb-stat-icon">' . $icon . '</div>';
  $output .= '<div class="dfb-stat-body">';
  $output .= '<span class="dfb-stat-value">' . $value . '</span>';
  $output .= '<span class="dfb-stat-label">' . $label . '</span>';
  $output .= '</div>';
  $output .= '</div>';

  return $output;
}

/* =====================================================================
   DASHBOARD — FORMS PANEL
   ===================================================================== */

/**
 * Renders the forms listing panel for the dashboard.
 *
 * @return string
 *   Rendered forms panel HTML.
 */
function _dynamic_form_dashboard_forms()
{
  global $user;
  $is_admin = user_access('edit any form');

  // Owned / all-admin forms.
  $query = db_select('dynamic_form_forms', 'f')
    ->fields('f', array('id', 'title', 'slug', 'status', 'visibility', 'created_at', 'created_by'))
    ->condition('f.deleted_at', NULL, 'IS NULL')
    ->orderBy('f.created_at', 'DESC');

  if (!$is_admin) {
    $query->condition('f.created_by', $user->uid);
  }

  $owned = $query->execute()->fetchAll();

  // Build a keyed map so we can avoid duplicates when merging member forms.
  $forms_by_id = array();
  foreach ($owned as $r) {
    $r->member_role = 'owner';
    $forms_by_id[$r->id] = $r;
  }

  // Member forms (invited by someone else) — any role.
  if (!$is_admin) {
    $mq = db_select('dynamic_form_members', 'm')
      ->fields('m', array('role'))
      ->condition('m.user_id', $user->uid);
    $mq->join('dynamic_form_forms', 'f', 'm.form_id = f.id');
    $mq->fields('f', array('id', 'title', 'slug', 'status', 'visibility', 'created_at', 'created_by'));
    $mq->condition('f.deleted_at', NULL, 'IS NULL');
    $mq->condition('f.created_by', $user->uid, '<>');

    foreach ($mq->execute()->fetchAll() as $r) {
      if (!isset($forms_by_id[$r->id])) {
        $r->member_role = $r->role;
        $forms_by_id[$r->id] = $r;
      }
    }
  }

  // Sort merged list by created_at DESC.
  uasort($forms_by_id, '_dfb_sort_by_created_desc');

  // Batch-load submitted response counts for all displayed forms in one query.
  $response_counts = array();
  $form_ids = array_keys($forms_by_id);
  if (!empty($form_ids)) {
    $cq = db_select('dynamic_form_responses', 'r')
      ->fields('r', array('form_id'))
      ->condition('r.form_id', $form_ids, 'IN')
      ->condition('r.status', 1)
      ->isNull('r.deleted_at')
      ->groupBy('r.form_id');
    $cq->addExpression('COUNT(*)', 'cnt');
    foreach ($cq->execute()->fetchAll() as $row) {
      $response_counts[(int) $row->form_id] = (int) $row->cnt;
    }
  }

  $output = '<div class="dfb-dashboard-header">';
  $output .= '<div class="dfb-dashboard-header-row">';
  $output .= '<div>';
  $output .= '<h2>' . t('Forms') . '</h2>';
  $output .= '<p class="dfb-dashboard-subtitle">' . t('Manage all your forms') . '</p>';
  $output .= '</div>';
  $output .= '<a href="' . url('dashboard/forms/create') . '" class="dfb-header-action-btn">' . t('+ New Form') . '</a>';
  $output .= '</div>';
  $output .= '</div>';

  if (empty($forms_by_id)) {
    $output .= '<div class="dfb-empty-state">';
    $output .= '<div class="dfb-empty-icon">&#128196;</div>';
    $output .= '<p>' . t('You haven\'t created any forms yet.') . '</p>';
    $output .= '<a href="' . url('dashboard/forms/create') . '" class="dfb-empty-action">' . t('Create your first form') . '</a>';
    $output .= '</div>';
    return $output;
  }

  $status_labels = array(
    0 => '<span class="dfb-badge dfb-badge-draft">'      . t('Draft')     . '</span>',
    1 => '<span class="dfb-badge dfb-badge-published">'  . t('Published') . '</span>',
    2 => '<span class="dfb-badge dfb-badge-closed">'     . t('Closed')    . '</span>',
  );

  $visibility_labels = array(
    'p' => t('Public'),
    'r' => t('Restricted'),
    'm' => t('Members Only'),
  );

  $header = array(t('Title'), t('Status'), t('Visibility'), t('Created'), t('Responses'), t('Actions'));
  $rows = array();

  foreach ($forms_by_id as $record) {
    $is_owner = ($record->member_role === 'owner');
    $role     = $is_owner ? 'owner' : $record->member_role;

    // Title with a "Shared" badge for member forms.
    $title_cell = check_plain($record->title);
    if (!$is_owner) {
      $role_label = ucfirst($role);
      $title_cell .= ' <span class="dfb-badge dfb-badge-shared">' . t('Shared · @role', array('@role' => $role_label)) . '</span>';
    }

    $actions = array();

    if (in_array($role, array('owner', 'editor', 'reviewer'))) {
      $actions[] = l(t('Builder'), 'dashboard/forms/' . $record->id . '/builder',
        array('attributes' => array('class' => array('dfb-action-builder'))));
    }

    if (in_array($role, array('owner', 'editor'))) {
      $actions[] = l(t('Edit'), 'dashboard/forms/' . $record->id . '/edit');
    }

    // Delete only for owners.
    if ($is_owner) {
      $actions[] = l(t('Delete'), 'form/' . $record->id . '/delete', array(
        'attributes' => array(
          'class'            => array('dfb-action-delete', 'dfb-delete-trigger'),
          'data-entity-type' => 'form',
          'data-entity-id'   => (string) $record->id,
          'data-entity-name' => $record->title,
        ),
      ));
    }

    $count     = isset($response_counts[$record->id]) ? $response_counts[$record->id] : 0;
    $can_view  = $is_admin || in_array($role, array('owner', 'editor'));
    if ($count === 0) {
      $resp_cell = '<span class="dfb-response-zero">—</span>';
    } elseif ($can_view) {
      $resp_cell = l(
        format_plural($count, '1 response', '@count responses'),
        'dashboard/forms/' . $record->id . '/responses',
        array('attributes' => array('class' => array('dfb-response-count-link')))
      );
    } else {
      $resp_cell = format_plural($count, '1 response', '@count responses');
    }

    $rows[] = array(
      array('data' => $title_cell),
      array('data' => isset($status_labels[$record->status]) ? $status_labels[$record->status] : t('Unknown'), 'class' => array('status-cell')),
      isset($visibility_labels[$record->visibility]) ? $visibility_labels[$record->visibility] : $record->visibility,
      format_date($record->created_at, 'short'),
      array('data' => $resp_cell, 'class' => array('dfb-responses-cell')),
      implode(' &middot; ', $actions),
    );
  }

  $output .= theme('table', array(
    'header' => $header,
    'rows'   => $rows,
    'attributes' => array('class' => array('dfb-dashboard-table')),
    'empty'  => t('No forms found.'),
  ));

  return $output;
}

function _dfb_sort_by_created_desc($a, $b)
{
  return $b->created_at - $a->created_at;
}

/* =====================================================================
   DASHBOARD — QUESTIONS PANEL
   ===================================================================== */

/**
 * Renders the questions listing panel for the dashboard.
 *
 * @return string
 *   Rendered questions panel HTML.
 */
function _dynamic_form_dashboard_questions()
{
  global $user;
  $is_admin = user_access('edit any form');

  $query = db_select('dynamic_form_questions', 'q');
  $query->join('dynamic_form_forms', 'f', 'q.form_id = f.id');
  $query->fields('q', array('id', 'label', 'type', 'is_required', 'position', 'created_at', 'created_by'));
  $query->addField('f', 'title', 'form_title');
  $query->addField('f', 'id', 'form_id');
  $query->condition('q.deleted_at', NULL, 'IS NULL');
  $query->condition('f.deleted_at', NULL, 'IS NULL');
  $query->orderBy('q.created_at', 'DESC');

  if (!$is_admin) {
    $query->condition('q.created_by', $user->uid);
  }

  $results = $query->execute()->fetchAll();

  $output = '<div class="dfb-dashboard-header">';
  $output .= '<h2>' . t('Questions') . '</h2>';
  $output .= '<p class="dfb-dashboard-subtitle">' . t('All questions across your forms') . '</p>';
  $output .= '</div>';

  if (empty($results)) {
    $output .= '<div class="dfb-empty-state">';
    $output .= '<div class="dfb-empty-icon">&#10067;</div>';
    $output .= '<p>' . t('No questions found. Questions will appear here once you add them to your forms.') . '</p>';
    $output .= '</div>';
    return $output;
  }

  $header = array(t('Question'), t('Type'), t('Required'), t('Form'), t('Created'), t('Actions'));
  $rows = array();

  foreach ($results as $record) {
    $required = $record->is_required
      ? '<span class="dfb-badge dfb-badge-published">' . t('Yes') . '</span>'
      : '<span class="dfb-badge dfb-badge-draft">' . t('No') . '</span>';

    $actions = array();
    if (user_access('edit any form') || $record->created_by == $GLOBALS['user']->uid) {
      $actions[] = l(t('Edit'), 'dashboard/questions/' . $record->id . '/edit');
    }
    if (user_access('delete any form') || $record->created_by == $GLOBALS['user']->uid) {
      $actions[] = l(t('Delete'), 'dashboard/questions/' . $record->id . '/delete', array(
        'attributes' => array(
          'class'            => array('dfb-action-delete', 'dfb-delete-trigger'),
          'data-entity-type' => 'question',
          'data-entity-id'   => (string) $record->id,
          'data-entity-name' => truncate_utf8($record->label, 60, TRUE, TRUE),
        ),
      ));
    }

    $rows[] = array(
      check_plain(truncate_utf8($record->label, 60, TRUE, TRUE)),
      '<code class="dfb-type-badge">' . check_plain($record->type) . '</code>',
      array('data' => $required, 'class' => array('status-cell')),
      l(check_plain($record->form_title), 'dashboard/forms/' . $record->form_id . '/builder'),
      format_date($record->created_at, 'short'),
      implode(' &middot; ', $actions),
    );
  }

  $output .= theme('table', array(
    'header'     => $header,
    'rows'       => $rows,
    'attributes' => array('class' => array('dfb-dashboard-table')),
    'empty'      => t('No questions found.'),
  ));

  return $output;
}

/* =====================================================================
   DASHBOARD — SECTIONS PANEL
   ===================================================================== */

/**
 * Renders the sections listing panel for the dashboard.
 *
 * @return string
 *   Rendered sections panel HTML.
 */
function _dynamic_form_dashboard_sections()
{
  global $user;
  $is_admin = user_access('edit any form');

  $query = db_select('dynamic_form_sections', 's');
  $query->join('dynamic_form_forms', 'f', 's.form_id = f.id');
  $query->fields('s', array('id', 'name', 'description', 'position', 'created_at'));
  $query->addField('f', 'title', 'form_title');
  $query->addField('f', 'id', 'form_id');
  $query->condition('s.deleted_at', NULL, 'IS NULL');
  $query->condition('f.deleted_at', NULL, 'IS NULL');
  $query->orderBy('s.created_at', 'DESC');

  if (!$is_admin) {
    $query->condition('s.created_by', $user->uid);
  }

  $results = $query->execute()->fetchAll();

  $output = '<div class="dfb-dashboard-header">';
  $output .= '<div class="dfb-dashboard-header-row">';
  $output .= '<div>';
  $output .= '<h2>' . t('Sections') . '</h2>';
  $output .= '<p class="dfb-dashboard-subtitle">' . t('All sections across your forms') . '</p>';
  $output .= '</div>';
  $output .= '<a href="' . url('dashboard/sections/create') . '" class="dfb-header-action-btn">' . t('+ New Section') . '</a>';
  $output .= '</div>';
  $output .= '</div>';

  if (empty($results)) {
    $output .= '<div class="dfb-empty-state">';
    $output .= '<div class="dfb-empty-icon">&#128193;</div>';
    $output .= '<p>' . t('No sections found. Create a section to group your questions.') . '</p>';
    $output .= '<a href="' . url('dashboard/sections/create') . '" class="dfb-empty-action">' . t('Create your first section') . '</a>';
    $output .= '</div>';
    return $output;
  }

  $header = array(t('Section Name'), t('Description'), t('Position'), t('Form'), t('Created'), t('Actions'));
  $rows = array();

  foreach ($results as $record) {
    $description = !empty($record->description)
      ? check_plain(truncate_utf8($record->description, 80, TRUE, TRUE))
      : '<em class="dfb-text-muted">' . t('No description') . '</em>';

    $actions = array();
    $actions[] = l(t('Edit'), 'dashboard/sections/' . $record->id . '/edit');
    $actions[] = l(t('Delete'), 'dashboard/sections/' . $record->id . '/delete', array(
      'attributes' => array(
        'class'            => array('dfb-action-delete', 'dfb-delete-trigger'),
        'data-entity-type' => 'section',
        'data-entity-id'   => (string) $record->id,
        'data-entity-name' => $record->name,
      ),
    ));

    $rows[] = array(
      check_plain($record->name),
      $description,
      $record->position,
      check_plain($record->form_title),
      format_date($record->created_at, 'short'),
      implode(' &middot; ', $actions),
    );
  }

  $output .= theme('table', array(
    'header' => $header,
    'rows' => $rows,
    'attributes' => array('class' => array('dfb-dashboard-table')),
    'empty' => t('No sections found.'),
  ));

  return $output;
}

/* =====================================================================
   DASHBOARD — FORM CREATE/EDIT WRAPPERS
   ===================================================================== */

/**
 * Page callback for dashboard/forms/create.
 *
 * Renders the create form inside the dashboard layout.
 *
 * @return string
 *   Rendered dashboard HTML with the create form in the content area.
 */
function dynamic_form_dashboard_form_create_page()
{
  drupal_set_title(t('Create Form'));

  // Load the forms include so the form builder is available.
  module_load_include('inc', 'dynamic_form', 'includes/dynamic_form.forms');

  $sidebar_html = _dynamic_form_dashboard_sidebar('forms');

  $content_html = '<div class="dfb-dashboard-header">';
  $content_html .= '<div class="dfb-dashboard-header-row">';
  $content_html .= '<div>';
  $content_html .= '<h2>' . t('Create New Form') . '</h2>';
  $content_html .= '<p class="dfb-dashboard-subtitle">' . t('Set up your form details, configuration, and schedule') . '</p>';
  $content_html .= '</div>';
  $content_html .= '</div>';
  $content_html .= '</div>';

  $content_html .= drupal_render(drupal_get_form('dynamic_form_create_form'));

  return theme('dynamic_form_dashboard', array(
    'sidebar' => $sidebar_html,
    'content' => $content_html,
  ));
}

/**
 * Page callback for dashboard/forms/%/edit.
 *
 * Renders the edit form inside the dashboard layout.
 *
 * @param int $form_id
 *   The form ID to edit.
 *
 * @return string
 *   Rendered dashboard HTML with the edit form in the content area.
 */
function dynamic_form_dashboard_form_edit_page($form_id)
{
  drupal_set_title(t('Edit Form'));

  // Load the forms include so the form builder is available.
  module_load_include('inc', 'dynamic_form', 'includes/dynamic_form.forms');

  $sidebar_html = _dynamic_form_dashboard_sidebar('forms');

  $content_html = '<div class="dfb-dashboard-header">';
  $content_html .= '<div class="dfb-dashboard-header-row">';
  $content_html .= '<div>';
  $content_html .= '<h2>' . t('Edit Form') . '</h2>';
  $content_html .= '<p class="dfb-dashboard-subtitle">' . t('Update your form details, configuration, and schedule') . '</p>';
  $content_html .= '</div>';
  $content_html .= '</div>';
  $content_html .= '</div>';

  $content_html .= drupal_render(drupal_get_form('dynamic_form_edit_form', $form_id));

  return theme('dynamic_form_dashboard', array(
    'sidebar' => $sidebar_html,
    'content' => $content_html,
  ));
}

/* =====================================================================
   DASHBOARD — SECTION CREATE/EDIT WRAPPERS
   ===================================================================== */

/**
 * Page callback for dashboard/sections/create.
 *
 * Renders the section create form inside the dashboard layout.
 *
 * @return string
 */
function dynamic_form_dashboard_section_create_page()
{
  drupal_set_title(t('Create Section'));

  module_load_include('inc', 'dynamic_form', 'includes/dynamic_form.sections');

  $sidebar_html = _dynamic_form_dashboard_sidebar('sections');

  $content_html = '<div class="dfb-dashboard-header">';
  $content_html .= '<div class="dfb-dashboard-header-row">';
  $content_html .= '<div>';
  $content_html .= '<h2>' . t('Create New Section') . '</h2>';
  $content_html .= '<p class="dfb-dashboard-subtitle">' . t('Group questions together within a form') . '</p>';
  $content_html .= '</div>';
  $content_html .= '</div>';
  $content_html .= '</div>';

  $content_html .= drupal_render(drupal_get_form('dynamic_form_create_section_form'));

  return theme('dynamic_form_dashboard', array(
    'sidebar' => $sidebar_html,
    'content' => $content_html,
  ));
}

/**
 * Page callback for dashboard/sections/%/edit.
 *
 * Renders the section edit form inside the dashboard layout.
 *
 * @param int $section_id
 *
 * @return string
 */
function dynamic_form_dashboard_section_edit_page($section_id)
{
  drupal_set_title(t('Edit Section'));

  module_load_include('inc', 'dynamic_form', 'includes/dynamic_form.sections');

  $sidebar_html = _dynamic_form_dashboard_sidebar('sections');

  $content_html = '<div class="dfb-dashboard-header">';
  $content_html .= '<div class="dfb-dashboard-header-row">';
  $content_html .= '<div>';
  $content_html .= '<h2>' . t('Edit Section') . '</h2>';
  $content_html .= '<p class="dfb-dashboard-subtitle">' . t('Update section details and configuration') . '</p>';
  $content_html .= '</div>';
  $content_html .= '</div>';
  $content_html .= '</div>';

  $content_html .= drupal_render(drupal_get_form('dynamic_form_edit_section_form', $section_id));

  return theme('dynamic_form_dashboard', array(
    'sidebar' => $sidebar_html,
    'content' => $content_html,
  ));
}

/* =====================================================================
   DASHBOARD — QUESTION EDIT WRAPPER
   ===================================================================== */

/**
 * Page callback for dashboard/questions/%/edit.
 *
 * @param int $question_id
 *
 * @return string
 */
function dynamic_form_dashboard_question_edit_page($question_id)
{
  drupal_set_title(t('Edit Question'));

  module_load_include('inc', 'dynamic_form', 'includes/dynamic_form.questions');

  // Load the same CSS/JS the builder page uses so all styles and the
  // type-picker/states behaviour work correctly on this standalone page.
  $module_path = drupal_get_path('module', 'dynamic_form');
  drupal_add_css($module_path . '/css/builder.css');
  drupal_add_js($module_path . '/js/builder.js');
  // Provide the minimal settings object builder.js expects.
  drupal_add_js(array(
    'dynamicFormBuilder' => array(
      'formId'   => 0,
      'basePath' => url('dynamic-form/ajax/builder'),
    ),
  ), 'setting');

  $sidebar_html = _dynamic_form_dashboard_sidebar('questions');

  $content_html  = '<div class="dfb-dashboard-header">';
  $content_html .= '<div class="dfb-dashboard-header-row">';
  $content_html .= '<div>';
  $content_html .= '<h2>' . t('Edit Question') . '</h2>';
  $content_html .= '<p class="dfb-dashboard-subtitle">' . t('Update question label, type, and settings') . '</p>';
  $content_html .= '</div>';
  $content_html .= '</div>';
  $content_html .= '</div>';

  $content_html .= drupal_render(drupal_get_form('dynamic_form_edit_question_form', $question_id));

  return theme('dynamic_form_dashboard', array(
    'sidebar' => $sidebar_html,
    'content' => $content_html,
  ));
}

/* =====================================================================
   DASHBOARD — DELETE MODAL HTML HELPER
   ===================================================================== */

/**
 * Returns the HTML for the shared dashboard delete-confirmation modal.
 */
function _dynamic_form_delete_confirm_modal() {
  return '
<div id="dfb-delete-modal" role="alertdialog" aria-modal="true" aria-labelledby="dfb-delete-modal-title" style="display:none;">
  <div class="dfb-delete-modal-dialog">
    <span class="dfb-delete-modal-icon">&#128465;</span>
    <h3 class="dfb-delete-modal-title" id="dfb-delete-modal-title">'
      . t('Delete this') . ' <span class="dfb-delete-modal-type"></span>?
    </h3>
    <p class="dfb-delete-modal-body">'
      . t('You are about to delete') . ' <strong class="dfb-delete-modal-name"></strong>. '
      . t('The item will be moved to Trash and can be restored later.') . '
    </p>
    <div class="dfb-delete-modal-actions">
      <button class="dfb-delete-confirm-btn" type="button">' . t('Delete') . '</button>
      <button class="dfb-delete-cancel-btn"  type="button">' . t('Cancel') . '</button>
    </div>
  </div>
</div>';
}

/* =====================================================================
   DASHBOARD — AJAX ENTITY SOFT-DELETE
   ===================================================================== */

/**
 * AJAX callback: soft-deletes a form, section, or question.
 *
 * Route: POST dynamic-form/ajax/delete/{entity_type}/{entity_id}
 * Returns JSON: {status: "ok"|"error", message: "..."}.
 *
 * @param string $entity_type  One of: form, section, question.
 * @param int    $entity_id    Primary key of the entity.
 */
function dynamic_form_ajax_entity_delete($entity_type, $entity_id) {
  global $user;

  if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    drupal_json_output(array('status' => 'error', 'message' => t('Invalid request method.')));
    drupal_exit();
  }

  if (!in_array($entity_type, array('form', 'section', 'question', 'response'))) {
    drupal_json_output(array('status' => 'error', 'message' => t('Invalid entity type.')));
    drupal_exit();
  }

  $entity_id = (int) $entity_id;
  $table     = _dynamic_form_dashboard_entity_table($entity_type);

  $record = db_select($table, 'e')
    ->fields('e')
    ->condition('id', $entity_id)
    ->condition('deleted_at', NULL, 'IS NULL')
    ->execute()
    ->fetchObject();

  if (!$record) {
    drupal_json_output(array('status' => 'error', 'message' => t('Item not found.')));
    drupal_exit();
  }

  if ($entity_type === 'response') {
    // Response access: admin OR owner/editor of the form.
    $can_delete = user_access('edit any form');
    if (!$can_delete) {
      $member_role = db_select('dynamic_form_members', 'm')
        ->fields('m', array('role'))
        ->condition('form_id', $record->form_id)
        ->condition('user_id', $user->uid)
        ->execute()
        ->fetchField();
      if (!$member_role) {
        $form_owner = db_select('dynamic_form_forms', 'f')
          ->fields('f', array('created_by'))
          ->condition('id', $record->form_id)
          ->execute()
          ->fetchField();
        if ((int) $form_owner === (int) $user->uid) {
          $member_role = 'owner';
        }
      }
      $can_delete = in_array($member_role, array('owner', 'editor'));
    }
  }
  else {
    // Access: 'delete any form' OR user owns the item and has 'delete own form'.
    $is_owner   = isset($record->created_by) && (int) $record->created_by === (int) $user->uid;
    $can_delete = user_access('delete any form') || ($is_owner && user_access('delete own form'));
  }

  if (!$can_delete) {
    drupal_json_output(array('status' => 'error', 'message' => t('Access denied.')));
    drupal_exit();
  }

  $now = REQUEST_TIME;
  db_update($table)
    ->fields(array(
      'deleted_at' => $now,
      'deleted_by' => $user->uid,
      'updated_at' => $now,
    ))
    ->condition('id', $entity_id)
    ->execute();

  _dynamic_form_audit_log($entity_type, $entity_id, 'deleted', (array) $record, NULL);

  if ($entity_type === 'response' && module_exists('dynamic_form_solr')) {
    dynamic_form_solr_delete_response($entity_id);
  }

  switch ($entity_type) {
    case 'form':     $label = $record->title; break;
    case 'section':  $label = $record->name;  break;
    case 'question': $label = truncate_utf8($record->label, 60, TRUE, TRUE); break;
    case 'response': $label = t('Response #@id', array('@id' => $entity_id)); break;
    default:         $label = 'Item';
  }

  drupal_json_output(array(
    'status'  => 'ok',
    'message' => t('"@label" has been deleted.', array('@label' => $label)),
  ));
  drupal_exit();
}

/**
 * Maps entity type string to its database table name.
 */
function _dynamic_form_dashboard_entity_table($entity_type) {
  $map = array(
    'form'     => 'dynamic_form_forms',
    'section'  => 'dynamic_form_sections',
    'question' => 'dynamic_form_questions',
    'response' => 'dynamic_form_responses',
  );
  return isset($map[$entity_type]) ? $map[$entity_type] : NULL;
}

/* =====================================================================
   BROWSE FORMS PAGE
   ===================================================================== */

/**
 * Page callback: dashboard/browse
 *
 * Lists all published, currently-open forms the user is eligible to fill out:
 * Public forms and Members-Only forms where the user has a membership record.
 */
function dynamic_form_dashboard_browse_page() {
  global $user;

  drupal_set_title(t('Browse Forms'));

  $now = REQUEST_TIME;

  // Sub-query: form IDs where this user is a member.
  $member_sub = db_select('dynamic_form_members', 'm')
    ->fields('m', array('form_id'))
    ->condition('m.user_id', $user->uid);

  // Visibility: public OR (members-only AND user is a member).
  $members_only = db_and()
    ->condition('f.visibility', 'm')
    ->condition('f.id', $member_sub, 'IN');

  $vis_cond = db_or()
    ->condition('f.visibility', 'p')
    ->condition($members_only);

  // opens_at window: NULL means no delay.
  $opens_cond = db_or()
    ->isNull('f.opens_at')
    ->condition('f.opens_at', $now, '<=');

  // closes_at window: NULL means never closes.
  $closes_cond = db_or()
    ->isNull('f.closes_at')
    ->condition('f.closes_at', $now, '>=');

  $query = db_select('dynamic_form_forms', 'f')
    ->fields('f', array('id', 'title', 'slug', 'description', 'visibility', 'closes_at'))
    ->condition('f.status', 1)
    ->isNull('f.deleted_at')
    ->condition($vis_cond)
    ->condition($opens_cond)
    ->condition($closes_cond)
    ->orderBy('f.created_at', 'DESC');

  $forms = $query->execute()->fetchAll();

  $visibility_labels = array(
    'p' => '<span class="dfb-badge dfb-badge-published">' . t('Public')       . '</span>',
    'm' => '<span class="dfb-badge dfb-badge-shared">'   . t('Members Only') . '</span>',
  );

  $out  = '<div class="dfb-dashboard-header">';
  $out .= '<h2>' . t('Browse Forms') . '</h2>';
  $out .= '<p class="dfb-dashboard-subtitle">' . t('Published forms you can fill out') . '</p>';
  $out .= '</div>';

  if (empty($forms)) {
    $out .= '<div class="dfb-empty-state">';
    $out .= '<div class="dfb-empty-icon">&#128269;</div>';
    $out .= '<p>' . t('No forms are currently available for you to fill out.') . '</p>';
    $out .= '</div>';
  }
  else {
    $header = array(t('Form'), t('Visibility'), t('Closes'), t('Actions'));
    $rows   = array();

    foreach ($forms as $f) {
      $vis_badge = isset($visibility_labels[$f->visibility])
        ? $visibility_labels[$f->visibility]
        : check_plain($f->visibility);

      $closes = $f->closes_at
        ? format_date($f->closes_at, 'custom', 'M j, Y')
        : '<em class="dfb-text-muted">' . t('No deadline') . '</em>';

      $rows[] = array(
        array('data' => check_plain($f->title)),
        array('data' => $vis_badge, 'class' => array('status-cell')),
        $closes,
        l(t('Fill Out'), 'forms/' . $f->slug),
      );
    }

    $out .= theme('table', array(
      'header'     => $header,
      'rows'       => $rows,
      'attributes' => array('class' => array('dfb-dashboard-table')),
      'empty'      => t('No forms found.'),
    ));
  }

  $sidebar_html = _dynamic_form_dashboard_sidebar('browse');

  return theme('dynamic_form_dashboard', array(
    'sidebar' => $sidebar_html,
    'content' => $out,
  ));
}

/* =====================================================================
   ACTIVITY LOG PAGE
   ===================================================================== */

/**
 * Page callback: dashboard/activity-log
 *
 * Displays a paginated table of all audit log entries showing who
 * created, edited, or deleted forms, sections, and questions.
 */
function dynamic_form_activity_log_page() {
  drupal_set_title(t('Activity Log'));

  $module_path = drupal_get_path('module', 'dynamic_form');
  drupal_add_css($module_path . '/css/builder.css');

  $sidebar_html = _dynamic_form_dashboard_sidebar('activity-log');

  $action_labels = array(
    'created' => t('Created'),
    'updated' => t('Updated'),
    'deleted' => t('Deleted'),
  );

  $action_classes = array(
    'created' => 'dfb-log-action--created',
    'updated' => 'dfb-log-action--updated',
    'deleted' => 'dfb-log-action--deleted',
  );

  $per_page = 50;
  $total    = db_select('dynamic_form_audit_log', 'l')->countQuery()->execute()->fetchField();

  pager_default_initialize($total, $per_page);

  $logs = db_select('dynamic_form_audit_log', 'l')
    ->fields('l')
    ->orderBy('created_at', 'DESC')
    ->extend('PagerDefault')
    ->limit($per_page)
    ->execute()
    ->fetchAll();

  // Pre-load actor usernames in one query.
  $actor_ids = array_filter(array_unique(array_map(function($r) { return (int) $r->actor_uid; }, $logs)));
  $usernames  = array();
  if (!empty($actor_ids)) {
    $users = db_select('users', 'u')
      ->fields('u', array('uid', 'name'))
      ->condition('uid', $actor_ids, 'IN')
      ->execute()
      ->fetchAll();
    foreach ($users as $u) {
      $usernames[(int) $u->uid] = $u->name;
    }
  }

  $output  = '<div class="dfb-activity-log-page">';
  $output .= '<div class="dfb-page-header">';
  $output .= '<h1 class="dfb-page-title">' . t('Activity Log') . '</h1>';
  $output .= '<p class="dfb-page-subtitle">' . t('A record of all create, edit, and delete actions on forms, sections, and questions.') . '</p>';
  $output .= '</div>';

  if (empty($logs)) {
    $output .= '<p class="dfb-empty-state">' . t('No activity has been recorded yet.') . '</p>';
  }
  else {
    // Truly internal fields never worth showing.
    $diff_skip = array('id', 'created', 'changed', 'actor_uid', 'ip_address',
      'form_id', 'section_id', 'question_id', 'serial',
      'created_by', 'published_by', 'deleted_by', 'deleted_at', 'published_at');

    // Fields whose values are Unix timestamps — format them as dates.
    $timestamp_fields = array('created_at', 'updated_at', 'closes_at', 'opens_at',
      'token_expires', 'accepted_at');

    // Human-readable labels for common field names.
    $field_labels = array(
      'title'          => t('Title'),
      'name'           => t('Name'),
      'label'          => t('Label'),
      'description'    => t('Description'),
      'status'         => t('Status'),
      'type'           => t('Type'),
      'required'       => t('Required'),
      'weight'         => t('Weight'),
      'allow_multiple' => t('Allow Multiple'),
      'options'        => t('Options'),
      'settings'       => t('Settings'),
      'role'           => t('Role'),
      'email'          => t('Email'),
      'token_expires'  => t('Token Expiry'),
      'accepted_at'    => t('Accepted At'),
      'updated_at'     => t('Updated At'),
      'created_at'     => t('Created At'),
      'closes_at'      => t('Closes At'),
      'opens_at'       => t('Opens At'),
      'visibility'     => t('Visibility'),
      'slug'           => t('Slug'),
      'question_per_page' => t('Questions / Page'),
    );

    $output .= '<div class="dfb-log-table-wrap">';
    $output .= '<table class="dfb-log-table">';
    $output .= '<thead><tr>';
    $output .= '<th>' . t('When') . '</th>';
    $output .= '<th>' . t('Who') . '</th>';
    $output .= '<th>' . t('Action') . '</th>';
    $output .= '<th>' . t('Entity') . '</th>';
    $output .= '<th>' . t('Detail') . '</th>';
    $output .= '<th>' . t('Old Value') . '</th>';
    $output .= '<th>' . t('New Value') . '</th>';
    $output .= '</tr></thead>';
    $output .= '<tbody>';

    foreach ($logs as $log) {
      $actor_name = isset($usernames[(int) $log->actor_uid])
        ? check_plain($usernames[(int) $log->actor_uid])
        : t('System');

      $action_label = isset($action_labels[$log->action])
        ? $action_labels[$log->action]
        : check_plain($log->action);

      $action_class = isset($action_classes[$log->action])
        ? $action_classes[$log->action]
        : '';

      // Extract a human-readable label from the JSON snapshot.
      $detail = '';
      $snapshot = $log->new_value ?: $log->old_value;
      if ($snapshot) {
        $data = json_decode($snapshot, TRUE);
        if (is_array($data)) {
          if (!empty($data['title'])) {
            $detail = check_plain($data['title']);
          }
          elseif (!empty($data['name'])) {
            $detail = check_plain($data['name']);
          }
          elseif (!empty($data['label'])) {
            $detail = check_plain(truncate_utf8($data['label'], 60, TRUE, TRUE));
          }
        }
      }

      // Build old/new value columns — only meaningful for 'updated' rows.
      $old_col = '<span class="dfb-log-na">—</span>';
      $new_col = '<span class="dfb-log-na">—</span>';

      if ($log->action === 'updated' && $log->old_value && $log->new_value) {
        $old_data = json_decode($log->old_value, TRUE);
        $new_data = json_decode($log->new_value, TRUE);
        if (is_array($old_data) && is_array($new_data)) {
          $old_items = array();
          $new_items = array();
          foreach ($new_data as $field => $new_val) {
            if (in_array($field, $diff_skip)) {
              continue;
            }
            $old_val = array_key_exists($field, $old_data) ? $old_data[$field] : NULL;
            // Normalise for comparison — cast both to string so "0" == 0.
            $old_str = is_array($old_val) ? json_encode($old_val) : (string) $old_val;
            $new_str = is_array($new_val) ? json_encode($new_val) : (string) $new_val;
            $changed  = ($old_str !== $new_str);

            $label = isset($field_labels[$field])
              ? $field_labels[$field]
              : ucwords(str_replace('_', ' ', $field));

            // Format Unix timestamps as human-readable dates.
            if (in_array($field, $timestamp_fields)) {
              $old_display = ($old_val !== NULL && is_numeric($old_val) && $old_val > 0)
                ? format_date((int) $old_val, 'short')
                : '<em class="dfb-log-empty">' . t('(none)') . '</em>';
              $new_display = (is_numeric($new_val) && $new_val > 0)
                ? format_date((int) $new_val, 'short')
                : '<em class="dfb-log-empty">' . t('(none)') . '</em>';
            }
            else {
              $old_display = $old_val !== NULL && $old_str !== ''
                ? check_plain(truncate_utf8($old_str, 120, TRUE, TRUE))
                : '<em class="dfb-log-empty">' . t('(empty)') . '</em>';
              $new_display = $new_str !== ''
                ? check_plain(truncate_utf8($new_str, 120, TRUE, TRUE))
                : '<em class="dfb-log-empty">' . t('(empty)') . '</em>';
            }

            $li_class = $changed ? ' dfb-log-item--changed' : '';
            $old_items[] = '<li class="dfb-log-item' . $li_class . '"><span class="dfb-log-field">' . check_plain($label) . ':</span> ' . $old_display . '</li>';
            $new_items[] = '<li class="dfb-log-item' . $li_class . '"><span class="dfb-log-field">' . check_plain($label) . ':</span> ' . $new_display . '</li>';
          }
          if (!empty($old_items)) {
            $old_col = '<ul class="dfb-log-changes">' . implode('', $old_items) . '</ul>';
            $new_col = '<ul class="dfb-log-changes">' . implode('', $new_items) . '</ul>';
          }
        }
      }

      $output .= '<tr>';
      $output .= '<td class="dfb-log-time">' . format_date($log->created_at, 'short') . '</td>';
      $output .= '<td class="dfb-log-actor">' . $actor_name . '</td>';
      $output .= '<td><span class="dfb-log-action ' . $action_class . '">' . $action_label . '</span></td>';
      $output .= '<td class="dfb-log-entity">' . check_plain($log->entity_type) . ' #' . (int) $log->entity_id . '</td>';
      $output .= '<td class="dfb-log-detail">' . $detail . '</td>';
      $output .= '<td class="dfb-log-oldval">' . $old_col . '</td>';
      $output .= '<td class="dfb-log-newval">' . $new_col . '</td>';
      $output .= '</tr>';
    }

    $output .= '</tbody></table>';
    $output .= '</div>';
    $output .= theme('pager');
  }

  $output .= '</div>';

  return theme('dynamic_form_dashboard', array(
    'sidebar' => $sidebar_html,
    'content' => $output,
  ));
}
