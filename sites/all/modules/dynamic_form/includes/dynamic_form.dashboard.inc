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
  );

  $output = '<nav class="dfb-sidebar-nav">';
  $output .= '<div class="dfb-sidebar-header">';
  $output .= '<span class="dfb-sidebar-icon">&#9881;</span>';
  $output .= '<span class="dfb-sidebar-title">' . t('Manage') . '</span>';
  $output .= '</div>';
  $output .= '<ul class="dfb-sidebar-menu">';

  foreach ($items as $key => $item) {
    $classes = 'dfb-sidebar-item';
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

  $query = db_select('dynamic_form_forms', 'f')
    ->fields('f', array('id', 'title', 'slug', 'status', 'visibility', 'created_at', 'created_by'))
    ->condition('f.deleted_at', NULL, 'IS NULL')
    ->orderBy('f.created_at', 'DESC');

  if (!$is_admin) {
    $query->condition('f.created_by', $user->uid);
  }

  $results = $query->execute()->fetchAll();

  $output = '<div class="dfb-dashboard-header">';
  $output .= '<div class="dfb-dashboard-header-row">';
  $output .= '<div>';
  $output .= '<h2>' . t('Forms') . '</h2>';
  $output .= '<p class="dfb-dashboard-subtitle">' . t('Manage all your forms') . '</p>';
  $output .= '</div>';
  $output .= '<a href="' . url('dashboard/forms/create') . '" class="dfb-header-action-btn">' . t('+ New Form') . '</a>';
  $output .= '</div>';
  $output .= '</div>';

  if (empty($results)) {
    $output .= '<div class="dfb-empty-state">';
    $output .= '<div class="dfb-empty-icon">&#128196;</div>';
    $output .= '<p>' . t('You haven\'t created any forms yet.') . '</p>';
    $output .= '<a href="' . url('dashboard/forms/create') . '" class="dfb-empty-action">' . t('Create your first form') . '</a>';
    $output .= '</div>';
    return $output;
  }

  $status_labels = array(
    0 => '<span class="dfb-badge dfb-badge-draft">' . t('Draft') . '</span>',
    1 => '<span class="dfb-badge dfb-badge-published">' . t('Published') . '</span>',
    2 => '<span class="dfb-badge dfb-badge-closed">' . t('Closed') . '</span>',
  );

  $visibility_labels = array(
    'p' => t('Public'),
    'r' => t('Restricted'),
    'm' => t('Members Only'),
  );

  $header = array(t('Title'), t('Status'), t('Visibility'), t('Created'), t('Actions'));
  $rows = array();

  foreach ($results as $record) {
    $actions = array();
    $actions[] = l(t('Builder'), 'dashboard/forms/' . $record->id . '/builder', array('attributes' => array('class' => array('dfb-action-builder'))));
    $actions[] = l(t('Edit'), 'dashboard/forms/' . $record->id . '/edit');
    $actions[] = l(t('Delete'), 'form/' . $record->id . '/delete', array('attributes' => array('class' => array('dfb-action-delete'))));

    $rows[] = array(
      check_plain($record->title),
      array('data' => isset($status_labels[$record->status]) ? $status_labels[$record->status] : t('Unknown'), 'class' => array('status-cell')),
      isset($visibility_labels[$record->visibility]) ? $visibility_labels[$record->visibility] : $record->visibility,
      format_date($record->created_at, 'short'),
      implode(' &middot; ', $actions),
    );
  }

  $output .= theme('table', array(
    'header' => $header,
    'rows' => $rows,
    'attributes' => array('class' => array('dfb-dashboard-table')),
    'empty' => t('No forms found.'),
  ));

  return $output;
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
      $actions[] = l(t('Delete'), 'dashboard/questions/' . $record->id . '/delete', array('attributes' => array('class' => array('dfb-action-delete'))));
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
    $actions[] = l(t('Delete'), 'dashboard/sections/' . $record->id . '/delete', array('attributes' => array('class' => array('dfb-action-delete'))));

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
