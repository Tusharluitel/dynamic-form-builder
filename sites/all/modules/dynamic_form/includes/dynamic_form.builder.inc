<?php

/**
 * @file
 * Form Builder page, Drupal Form API section form, and AJAX endpoints.
 */

/* =====================================================================
   WIDTH PICKER HELPER
   ===================================================================== */

/**
 * Returns HTML for the visual width-picker button strip.
 *
 * Each button shows a mini column-layout icon (N evenly-spaced bars with the
 * leftmost bar filled to represent this question's span) plus a text label.
 * The button whose $value matches $current_width gets the dfb-width-active
 * class; JS keeps this state in sync while the modal is open.
 *
 * @param string $current_width  One of: full | half | third | quarter.
 * @return string HTML.
 */
function _dfb_width_picker_markup($current_width = 'full') {
  $allowed = array('full', 'half', 'third', 'quarter');
  if (!in_array($current_width, $allowed)) {
    $current_width = 'full';
  }
  $options = array(
    'full'    => array('label' => t('Full'),    'cols' => 1),
    'half'    => array('label' => t('Half'),    'cols' => 2),
    'third'   => array('label' => t('Third'),   'cols' => 3),
    'quarter' => array('label' => t('Quarter'), 'cols' => 4),
  );
  $html  = '<div class="dfb-width-picker">';
  $html .= '<p class="dfb-subsection-label">' . t('Column Width') . '</p>';
  $html .= '<div class="dfb-width-options">';
  foreach ($options as $value => $opt) {
    $active = ($value === $current_width) ? ' dfb-width-active' : '';
    $html .= '<button type="button" class="dfb-width-btn' . $active . '" data-width="' . $value . '">';
    $html .= '<span class="dfb-width-icon">';
    for ($i = 0; $i < $opt['cols']; $i++) {
      $cls = ($i === 0) ? 'dfb-width-col dfb-width-col-filled' : 'dfb-width-col';
      $html .= '<span class="' . $cls . '"></span>';
    }
    $html .= '</span>';
    $html .= '<span class="dfb-width-label">' . check_plain($opt['label']) . '</span>';
    $html .= '</button>';
  }
  $html .= '</div>';
  $html .= '</div>';
  return $html;
}

/* =====================================================================
   BUILDER PAGE CALLBACK
   ===================================================================== */

/**
 * Page callback: renders the form builder shell.
 *
 * The sections list and "Add Section" form are handled by the embedded
 * Drupal Form API form (dynamic_form_builder_sections_form). Questions
 * are still managed via the custom AJAX endpoint and modal.
 */
function dynamic_form_builder_page($form_id) {
  $form_record = db_select('dynamic_form_forms', 'f')
    ->fields('f')
    ->condition('id', $form_id)
    ->condition('deleted_at', NULL, 'IS NULL')
    ->execute()
    ->fetchObject();

  if (!$form_record) {
    drupal_set_message(t('Form not found.'), 'error');
    drupal_goto('dashboard/forms');
  }

  drupal_set_title(t('Builder: @title', array('@title' => $form_record->title)));

  $module_path = drupal_get_path('module', 'dynamic_form');
  drupal_add_css($module_path . '/css/builder.css');
  drupal_add_js('https://cdn.jsdelivr.net/npm/sortablejs@latest/Sortable.min.js', 'external');
  drupal_add_js($module_path . '/js/builder.js');
  drupal_add_js(array(
    'dynamicFormBuilder' => array(
      'formId'      => (int) $form_record->id,
      'basePath'    => url('dynamic-form/ajax/builder'),
      'deletePath'  => url('dynamic-form/ajax/delete'),
    ),
  ), 'setting');

  module_load_include('inc', 'dynamic_form', 'includes/dynamic_form.dashboard');
  $sidebar_html = _dynamic_form_dashboard_sidebar('forms');

  $output  = '<div class="dfb-builder-container" id="dfb-builder-container"';
  $output .= ' data-form-id="' . (int) $form_record->id . '">';

  // Form header card.
  $output .= '<div class="dfb-builder-header-card">';
  $output .= '<div class="dfb-builder-header-top">';
  $output .= '<div class="dfb-builder-header-text">';
  $output .= '<h1 class="dfb-builder-title">' . check_plain($form_record->title) . '</h1>';
  if (!empty($form_record->description)) {
    $output .= '<p class="dfb-builder-desc">' . check_markup($form_record->description) . '</p>';
  }
  $output .= '</div>';
  $output .= '<a href="' . url('dashboard/forms/' . (int) $form_record->id . '/preview') . '"'
    . ' target="_blank" class="dfb-btn-preview" title="' . t('Open preview in a new tab') . '">'
    . t('Preview') . '</a>';
  $output .= '</div>';
  $output .= '</div>';

  // Drupal Form API form: sections list + Add Section.
  $output .= drupal_render(drupal_get_form('dynamic_form_builder_sections_form', (int) $form_record->id));

  // Question modal shell — the body is a Drupal Form API form.
  $output .= '<div id="dfb-question-modal" class="dfb-modal-overlay" style="display:none;">';
  $output .= '<div class="dfb-modal-content">';
  $output .= '<div class="dfb-modal-header">';
  $output .= '<div class="dfb-modal-header-text">';
  $output .= '<h2>' . t('Add Question') . '</h2>';
  $output .= '<p class="dfb-modal-header-sub">' . t('Choose a question type to add to this section') . '</p>';
  $output .= '</div>';
  $output .= '<button type="button" class="dfb-modal-close" data-action="close-modal">&times;</button>';
  $output .= '</div>';
  $output .= drupal_render(drupal_get_form('dynamic_form_builder_question_form', (int) $form_record->id));
  $output .= '</div>';
  $output .= '</div>';

  // Delete confirmation modal.
  $output .= '<div id="dfb-delete-confirm-modal" class="dfb-modal-overlay" style="display:none;">';
  $output .= '<div class="dfb-delete-confirm-box">';
  $output .= '<div class="dfb-delete-confirm-icon">&#128465;</div>';
  $output .= '<p class="dfb-delete-confirm-msg">' . t('Delete') . ' "<strong id="dfb-delete-confirm-name"></strong>"?</p>';
  $output .= '<p class="dfb-delete-confirm-sub">' . t('This will move it to Trash. You can restore it later.') . '</p>';
  $output .= '<div class="dfb-delete-confirm-actions">';
  $output .= '<button type="button" class="dfb-btn-cancel" data-action="close-delete-modal">' . t('Cancel') . '</button>';
  $output .= '<button type="button" class="dfb-btn-delete-confirm" id="dfb-delete-confirm-btn">' . t('Delete') . '</button>';
  $output .= '</div>';
  $output .= '</div>';
  $output .= '</div>';

  // Edit question modal shell — content is loaded on demand via AJAX.
  $output .= '<div id="dfb-edit-question-modal" class="dfb-modal-overlay" style="display:none;">';
  $output .= '<div class="dfb-modal-content">';
  $output .= '<div id="dfb-edit-form-placeholder">';
  $output .= '<div class="dfb-modal-header">';
  $output .= '<div class="dfb-modal-header-text"><h2>' . t('Edit Question') . '</h2></div>';
  $output .= '<button type="button" class="dfb-modal-close" data-action="close-edit-modal">&times;</button>';
  $output .= '</div>';
  $output .= '<div class="dfb-edit-form-loading"><p>' . t('Loading...') . '</p></div>';
  $output .= '</div>';
  $output .= '</div>';
  $output .= '</div>';

  $output .= '</div>';

  return theme('dynamic_form_dashboard', array(
    'sidebar'  => $sidebar_html,
    'content'  => $output,
  ));
}

/* =====================================================================
   SECTIONS — DRUPAL FORM API (add-more pattern)
   ===================================================================== */

/**
 * Form builder: sections list + inline "Add Section" card.
 *
 * On initial load, sections are pulled from the DB and stored in
 * $form_state['sections']. On every rebuild triggered by the submit
 * handler, the in-memory list (already updated) is used — no extra
 * DB query for newly-added sections.
 *
 * $form_state['build_info']['files'] is set so Drupal's system/ajax
 * endpoint can locate this function during AJAX rebuilds.
 *
 * @param array $form
 * @param array &$form_state
 * @param int   $form_id  The dynamic_form_forms record ID.
 */
function dynamic_form_builder_sections_form($form, &$form_state, $form_id) {
  // Required: tells Drupal where to find this function for AJAX rebuilds.
  $form_state['build_info']['files'][] = array(
    'type'   => 'inc',
    'module' => 'dynamic_form',
    'name'   => 'includes/dynamic_form.builder',
  );

  // First load: pull sections from DB and store form context.
  if (!isset($form_state['sections'])) {
    $form_state['dfb_form_id'] = $form_id;
    $form_state['sections']    = db_select('dynamic_form_sections', 's')
      ->fields('s')
      ->condition('form_id', $form_id)
      ->condition('deleted_at', NULL, 'IS NULL')
      ->orderBy('position', 'ASC')
      ->execute()
      ->fetchAll();
  }

  // Load questions from DB on every build so newly-added ones appear.
  $questions_by_section = array();
  $all_questions = db_select('dynamic_form_questions', 'q')
    ->fields('q')
    ->condition('form_id', $form_state['dfb_form_id'])
    ->condition('deleted_at', NULL, 'IS NULL')
    ->orderBy('position', 'ASC')
    ->execute()
    ->fetchAll();
  foreach ($all_questions as $q) {
    $sid = $q->section_id ? (int) $q->section_id : 0;
    $questions_by_section[$sid][] = $q;
  }

  // -----------------------------------------------------------------
  // Sections wrapper — this element is replaced by the AJAX callback.
  // -----------------------------------------------------------------
  $form['sections_wrapper'] = array(
    '#type'       => 'container',
    '#attributes' => array(
      'id'    => 'dfb-sections-wrapper',
      'class' => array('dfb-sections-wrapper'),
    ),
  );

  if (empty($form_state['sections'])) {
    $form['sections_wrapper']['empty_state'] = array(
      '#markup' => '<div class="dfb-builder-empty">'
        . '<p>' . t('No sections yet. Add your first section below.') . '</p>'
        . '</div>',
    );
  }
  else {
    foreach ($form_state['sections'] as $section) {
      $q = isset($questions_by_section[$section->id])
        ? $questions_by_section[$section->id]
        : array();
      $form['sections_wrapper']['section_' . $section->id] = array(
        '#markup' => _dynamic_form_render_builder_section($section, $q),
      );
    }
  }

  // -----------------------------------------------------------------
  // Add Section card — always rendered (fixes the "one section" bug).
  // -----------------------------------------------------------------
  $form['add_section_card'] = array(
    '#type'       => 'container',
    '#attributes' => array('class' => array('dfb-add-section-card')),
  );

  $form['add_section_card']['section_name'] = array(
    '#type'        => 'textfield',
    '#title'       => t('Section Name'),
    '#maxlength'   => 255,
    '#attributes'  => array('placeholder' => t('e.g. Personal Details')),
    '#description' => t('A clear, descriptive name for this section.'),
  );

  $form['add_section_card']['section_description'] = array(
    '#type'       => 'textarea',
    '#title'      => t('Description'),
    '#rows'       => 2,
    '#required'   => FALSE,
    '#attributes' => array('placeholder' => t('Optional — shown to respondents.')),
  );

  $form['add_section_card']['add_section_submit'] = array(
    '#type'       => 'submit',
    '#value'      => t('+ Add Section'),
    '#attributes' => array('class' => array('dfb-btn-add-section')),
    // Only validate the two add-section fields so stale state elsewhere
    // never blocks a legitimate add-section submit.
    '#limit_validation_errors' => array(
      array('section_name'),
      array('section_description'),
    ),
    '#ajax' => array(
      'callback' => 'dynamic_form_builder_sections_ajax_callback',
      'wrapper'  => 'dfb-sections-wrapper',
      'method'   => 'replace',
      'effect'   => 'fade',
    ),
  );

  return $form;
}

/**
 * Validate handler for dynamic_form_builder_sections_form.
 */
function dynamic_form_builder_sections_form_validate($form, &$form_state) {
  $name = trim($form_state['values']['section_name']);

  if ($name === '') {
    form_set_error('section_name', t('Section name is required.'));
  }
  elseif (drupal_strlen($name) < 2) {
    form_set_error('section_name', t('Section name must be at least 2 characters.'));
  }
}

/**
 * Submit handler for dynamic_form_builder_sections_form.
 *
 * Inserts the new section into the database, appends it to
 * $form_state['sections'], then sets $form_state['rebuild'] = TRUE
 * so Drupal re-runs the form builder and the AJAX callback returns
 * a freshly-rendered sections wrapper.
 */
function dynamic_form_builder_sections_form_submit($form, &$form_state) {
  global $user;
  $now  = REQUEST_TIME;
  $name = trim($form_state['values']['section_name']);
  $desc = trim($form_state['values']['section_description']);

  // Position = number of existing (non-deleted) sections for this form.
  $position = count($form_state['sections']);

  $fields = array(
    'form_id'     => (int) $form_state['dfb_form_id'],
    'name'        => $name,
    'description' => $desc,
    'position'    => $position,
    'created_by'  => (int) $user->uid,
    'created_at'  => $now,
    'updated_at'  => $now,
  );

  $section_id = db_insert('dynamic_form_sections')
    ->fields($fields)
    ->execute();

  // Append the new section to in-memory state so the form builder
  // renders it on the next pass without a DB round-trip.
  $new_section       = (object) $fields;
  $new_section->id   = (int) $section_id;
  $form_state['sections'][] = $new_section;

  // Clear the add-section input fields for the next render.
  $form_state['input']['section_name']        = '';
  $form_state['input']['section_description'] = '';

  // Signal Drupal to re-run the form builder before the AJAX callback.
  $form_state['rebuild'] = TRUE;
}

/**
 * AJAX callback for dynamic_form_builder_sections_form.
 *
 * Returns the sections_wrapper element. Because $form_state['rebuild']
 * was set to TRUE in the submit handler, Drupal has already re-run the
 * form builder; $form['sections_wrapper'] now includes the new section.
 * Drupal wraps the return value in ajax_command_replace targeting
 * #dfb-sections-wrapper.
 */
function dynamic_form_builder_sections_ajax_callback($form, &$form_state) {
  $commands   = array();
  $commands[] = ajax_command_replace('#dfb-sections-wrapper', render($form['sections_wrapper']));
  // Clear the add-section fields in the browser after a successful submit.
  $commands[] = ajax_command_invoke('input[name="section_name"]', 'val', array(''));
  $commands[] = ajax_command_invoke('textarea[name="section_description"]', 'val', array(''));
  return array('#type' => 'ajax', '#commands' => $commands);
}

/* =====================================================================
   BUILDER RENDERING HELPERS
   ===================================================================== */

/**
 * Renders a single section card for the builder UI.
 *
 * @param object $section   Section DB row.
 * @param array  $questions Questions belonging to this section.
 *
 * @return string HTML.
 */
function _dynamic_form_render_builder_section($section, array $questions) {
  $output  = '<div class="dfb-section-card" data-section-id="' . (int) $section->id . '">';

  // Section header with drag handle.
  $output .= '<div class="dfb-section-header">';
  $output .= '<div class="dfb-drag-handle dfb-section-drag-handle"'
    . ' title="' . t('Drag to reorder') . '">&#8942;&#8942;</div>';
  $output .= '<div class="dfb-section-title-wrap">';
  $output .= '<h2 class="dfb-section-title">' . check_plain($section->name) . '</h2>';
  $output .= '<button class="dfb-section-edit-icon" data-action="inline-rename-section"'
    . ' title="' . t('Rename section') . '">&#9998;</button>';
  $output .= '<button class="dfb-section-delete-icon" data-action="open-delete-modal"'
    . ' data-entity-type="section"'
    . ' data-entity-id="' . (int) $section->id . '"'
    . ' data-entity-name="' . check_plain($section->name) . '"'
    . ' title="' . t('Delete section') . '">'
    . '<svg xmlns="http://www.w3.org/2000/svg" width="14" height="14" viewBox="0 0 24 24"'
    . ' fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">'
    . '<polyline points="3 6 5 6 21 6"/><path d="M19 6l-1 14H6L5 6"/>'
    . '<path d="M10 11v6"/><path d="M14 11v6"/><path d="M9 6V4h6v2"/>'
    . '</svg></button>';
  if (!empty($section->description)) {
    $output .= '<p class="dfb-section-desc">' . check_plain($section->description) . '</p>';
  }
  $output .= '</div>';
  $output .= '</div>';

  // Questions list (sortable within this section).
  $output .= '<div class="dfb-questions-wrapper" data-section-id="' . (int) $section->id . '">';
  foreach ($questions as $question) {
    $output .= _dynamic_form_render_builder_question($question);
  }
  $output .= '</div>';

  // Footer: add-question button.
  $output .= '<div class="dfb-section-footer">';
  $output .= '<button class="dfb-btn-add-question"'
    . ' data-action="open-question-modal"'
    . ' data-section-id="' . (int) $section->id . '"'
    . ' data-section-name="' . check_plain($section->name) . '">';
  $output .= '<span class="icon">+</span> ' . t('Add Question');
  $output .= '</button>';
  $output .= '</div>';

  $output .= '</div>';

  return $output;
}

/**
 * Renders a single question card for the builder UI.
 *
 * @param object $question Question DB row.
 *
 * @return string HTML.
 */
function _dynamic_form_render_builder_question($question) {
  $req_star = $question->is_required
    ? ' <span class="dfb-req">*</span>'
    : '';

  $output  = '<div class="dfb-question-card" data-question-id="' . (int) $question->id . '">';
  $output .= '<div class="dfb-drag-handle dfb-question-drag-handle"'
    . ' title="' . t('Drag to reorder') . '">&#8942;&#8942;</div>';

  $output .= '<div class="dfb-question-content dfb-question-editable"'
    . ' title="' . t('Click to edit') . '">';
  $output .= '<div class="dfb-question-header">';
  $output .= '<h3 class="dfb-question-label">' . check_plain($question->label) . $req_star . '</h3>';
  $width_labels = array('half' => '&#189;', 'third' => '&#8531;', 'quarter' => '&#188;');
  $q_width = (!empty($question->width) && isset($width_labels[$question->width])) ? $question->width : '';

  $output .= '<div class="dfb-question-header-right">';
  if ($q_width) {
    $output .= '<span class="dfb-question-width-badge">' . $width_labels[$q_width] . '</span>';
  }
  $output .= '<span class="dfb-question-type-badge">' . check_plain($question->type) . '</span>';
  $output .= '<span class="dfb-edit-hint">&#9998;</span>';
  $output .= '<button class="dfb-question-delete-icon" data-action="open-delete-modal"'
    . ' data-entity-type="question"'
    . ' data-entity-id="' . (int) $question->id . '"'
    . ' data-entity-name="' . check_plain($question->label) . '"'
    . ' title="' . t('Delete question') . '">'
    . '<svg xmlns="http://www.w3.org/2000/svg" width="13" height="13" viewBox="0 0 24 24"'
    . ' fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">'
    . '<polyline points="3 6 5 6 21 6"/><path d="M19 6l-1 14H6L5 6"/>'
    . '<path d="M10 11v6"/><path d="M14 11v6"/><path d="M9 6V4h6v2"/>'
    . '</svg></button>';
  $output .= '</div>';
  $output .= '</div>';

  if (!empty($question->description)) {
    $output .= '<div class="dfb-question-desc">' . check_plain($question->description) . '</div>';
  }

  $output .= '</div>';
  $output .= '</div>';

  return $output;
}

/* =====================================================================
   QUESTION FORM — DRUPAL FORM API (add/remove pattern)
   ===================================================================== */

/**
 * Form builder: "Add Question" modal.
 *
 * Layout: type-picker grid (fixed) → separator → scrollable body → fixed
 * action footer. The body scrolls independently; type grid and actions
 * stay visible at all times.
 *
 * $form_state['options']     - array of ['label' => ''] for each option row.
 * $form_state['validations'] - array of ['rule_type','rule_value','error_message'].
 *
 * @param array  $form
 * @param array &$form_state
 * @param int    $form_id  The dynamic_form_forms record ID.
 */
function dynamic_form_builder_question_form($form, &$form_state, $form_id) {
  $form_state['build_info']['files'][] = array(
    'type'   => 'inc',
    'module' => 'dynamic_form',
    'name'   => 'includes/dynamic_form.builder',
  );

  if (!isset($form_state['options'])) {
    $form_state['dfb_q_form_id']  = $form_id;
    $form_state['options']        = array(array('label' => ''), array('label' => ''));
    $form_state['validations']    = array();
  }

  $form['#prefix'] = '<div id="dfb-question-form-wrapper">';
  $form['#suffix'] = '</div>';

  // section_id is set by JS when the modal opens.
  $form['section_id'] = array('#type' => 'hidden', '#default_value' => 0);

  // -----------------------------------------------------------------
  // Zone 1 — Type picker (fixed, does not scroll).
  // Uses #type => 'radios' styled as cards; #states watches this element.
  // -----------------------------------------------------------------
  $form['type_section'] = array(
    '#type'       => 'container',
    '#attributes' => array('class' => array('dfb-type-section')),
  );
  $form['type_section']['type'] = array(
    '#type'          => 'radios',
    '#title'         => t('Question Type'),
    '#title_display' => 'invisible',
    '#required'      => TRUE,
    '#default_value' => 'text',
    '#options'       => array(
      'text'         => t('Short Text'),
      'textarea'     => t('Long Text'),
      'text_editor'  => t('Text Editor'),
      'radio'        => t('Single Choice'),
      'checkbox'     => t('Multi Choice'),
      'select'       => t('Dropdown'),
      'tags'         => t('Tags'),
      'file'         => t('File Upload'),
      'date'         => t('Date'),
      'time'         => t('Time'),
      'rating'       => t('Rating'),
      'linear_scale' => t('Scale'),
    ),
    '#attributes'    => array('class' => array('dfb-type-picker')),
  );

  $form['sep'] = array('#markup' => '<hr class="dfb-modal-separator">');

  // -----------------------------------------------------------------
  // Zone 2 — Scrollable body.
  // All input fields live here so the type grid and footer stay fixed.
  // -----------------------------------------------------------------
  $form['scrollable'] = array(
    '#type'       => 'container',
    '#attributes' => array('class' => array('dfb-form-scrollable')),
  );

  // Question label + section name — 2-column row.
  $form['scrollable']['label'] = array(
    '#type'       => 'textfield',
    '#title'      => t('Question Label'),
    '#required'   => TRUE,
    '#maxlength'  => 512,
    '#attributes' => array('placeholder' => t('e.g. What is your preferred department?')),
    '#prefix'     => '<div class="dfb-form-row">',
  );
  $form['scrollable']['section_display'] = array(
    '#type'   => 'item',
    '#title'  => t('Section'),
    '#markup' => '<span id="dfb-section-name-display" class="dfb-section-name-val"></span>',
    '#suffix' => '</div>',
  );

  // Required + Read-only toggles — side by side.
  $form['scrollable']['is_required'] = array(
    '#type'   => 'checkbox',
    '#title'  => t('Required'),
    '#prefix' => '<div class="dfb-toggle-row">',
  );
  $form['scrollable']['is_readonly'] = array(
    '#type'   => 'checkbox',
    '#title'  => t('Read-only'),
    '#suffix' => '</div>',
  );

  // Help text.
  $form['scrollable']['description'] = array(
    '#type'       => 'textarea',
    '#title'      => t('Help Text'),
    '#rows'       => 2,
    '#required'   => FALSE,
    '#attributes' => array('placeholder' => t('Optional — shown to respondents.')),
  );

  // Width picker — visual segmented buttons + hidden field.
  // On rebuild (add/remove option AJAX), $form_state['input']['width'] carries
  // the user's in-progress choice so the active button survives the re-render.
  $current_width = (isset($form_state['input']['width']) && in_array($form_state['input']['width'], array('full', 'half', 'third', 'quarter')))
    ? $form_state['input']['width']
    : 'full';
  $form['scrollable']['width_picker'] = array(
    '#markup' => _dfb_width_picker_markup($current_width),
  );
  $form['scrollable']['width'] = array(
    '#type'          => 'hidden',
    '#default_value' => $current_width,
    '#attributes'    => array('class' => array('dfb-width-value')),
  );

  // Text editor info — shown only when text_editor type is selected.
  $form['scrollable']['text_editor_info'] = array(
    '#markup' =>
      '<div class="dfb-q-subsection dfb-text-editor-info" data-dfb-show-for="text_editor">' .
        '<p class="dfb-subsection-label">' . t('Rich Text Editor') . '</p>' .
        '<p class="dfb-text-editor-hint">' .
          t('Respondents will see a TinyMCE rich text editor for their answer. No additional configuration needed.') .
        '</p>' .
      '</div>',
  );

  // Tags info — shown only when tags type is selected.
  $form['scrollable']['tags_info'] = array(
    '#markup' =>
      '<div class="dfb-q-subsection dfb-tags-info" data-dfb-show-for="tags">' .
        '<p class="dfb-subsection-label">' . t('Tags') . '</p>' .
        '<p class="dfb-text-editor-hint">' .
          t('Respondents can search existing tags or type new ones. Tags are automatically normalized (lowercase, symbols removed) for analytics.') .
        '</p>' .
      '</div>',
  );

  // Select settings — allow multiple choice. Shown only for select type.
  $form['scrollable']['select_settings'] = array(
    '#type'       => 'container',
    '#attributes' => array('class' => array('dfb-q-subsection'), 'data-dfb-show-for' => 'select'),
  );
  $form['scrollable']['select_settings']['allow_multiple'] = array(
    '#type'  => 'checkbox',
    '#title' => t('Allow Multiple Choice'),
  );

  // -----------------------------------------------------------------
  // Options sub-section — radio / checkbox / select / rating / scale.
  // #states keeps this hidden/shown; no AJAX needed for type changes.
  // -----------------------------------------------------------------
  $choice_states = array(
    'visible' => array(
      array(':input[name="type"]' => array('value' => 'radio')),
      array(':input[name="type"]' => array('value' => 'checkbox')),
      array(':input[name="type"]' => array('value' => 'select')),
    ),
  );
  $form['scrollable']['options_wrapper'] = array(
    '#type'       => 'container',
    '#attributes' => array('id' => 'dfb-eq-options-wrapper', 'class' => array('dfb-q-subsection'), 'data-dfb-show-for' => 'radio checkbox select'),
  );
  $form['scrollable']['options_wrapper']['_title'] = array(
    '#markup' => '<p class="dfb-subsection-label">' . t('Options') . '</p>',
  );
  foreach ($form_state['options'] as $i => $opt) {
    $form['scrollable']['options_wrapper']['option_' . $i] = array(
      '#type'          => 'textfield',
      '#title'         => t('Option @n', array('@n' => $i + 1)),
      '#title_display' => 'invisible',
      '#default_value' => isset($opt['label']) ? $opt['label'] : '',
      '#attributes'    => array('placeholder' => t('Option @n', array('@n' => $i + 1))),
      '#prefix'        => '<div class="dfb-option-row">',
    );
    $form['scrollable']['options_wrapper']['remove_option_' . $i] = array(
      '#type'                    => 'submit',
      '#value'                   => t('×'),
      '#name'                    => 'remove_option_' . $i,
      '#submit'                  => array('_dfb_question_remove_option_submit'),
      '#limit_validation_errors' => array(),
      '#ajax'                    => array(
        'callback' => '_dfb_question_options_ajax_callback',
        'wrapper'  => 'dfb-eq-options-wrapper',
        'effect'   => 'none',
      ),
      '#attributes'              => array('class' => array('dfb-btn-remove'), 'title' => t('Remove')),
      '#suffix'                  => '</div>',
    );
  }
  $form['scrollable']['options_wrapper']['add_option'] = array(
    '#type'                    => 'submit',
    '#value'                   => t('+ Add Option'),
    '#name'                    => 'add_option',
    '#submit'                  => array('_dfb_question_add_option_submit'),
    '#limit_validation_errors' => array(),
    '#ajax'                    => array(
      'callback' => '_dfb_question_options_ajax_callback',
      'wrapper'  => 'dfb-eq-options-wrapper',
      'effect'   => 'none',
    ),
    '#attributes'              => array('class' => array('dfb-btn-add-more')),
  );

  // -----------------------------------------------------------------
  // Scale settings — rating / linear_scale only.
  // -----------------------------------------------------------------
  $form['scrollable']['scale_settings'] = array(
    '#type'       => 'container',
    '#attributes' => array('class' => array('dfb-q-subsection'), 'data-dfb-show-for' => 'rating linear_scale'),
  );
  $form['scrollable']['scale_settings']['_title'] = array(
    '#markup' => '<p class="dfb-subsection-label">' . t('Scale Range') . '</p>',
  );
  $form['scrollable']['scale_settings']['scale_min'] = array(
    '#type'          => 'select',
    '#title'         => t('Min'),
    '#options'       => drupal_map_assoc(range(0, 5)),
    '#default_value' => 1,
    '#prefix'        => '<div class="dfb-form-row dfb-scale-row">',
  );
  $form['scrollable']['scale_settings']['scale_max'] = array(
    '#type'          => 'select',
    '#title'         => t('Max'),
    '#options'       => drupal_map_assoc(range(2, 10)),
    '#default_value' => 5,
    '#suffix'        => '</div>',
  );
  $form['scrollable']['scale_settings']['scale_min_label'] = array(
    '#type'       => 'textfield',
    '#title'      => t('Min Label'),
    '#attributes' => array('placeholder' => t('e.g. Not at all')),
    '#prefix'     => '<div class="dfb-form-row dfb-scale-row">',
  );
  $form['scrollable']['scale_settings']['scale_max_label'] = array(
    '#type'       => 'textfield',
    '#title'      => t('Max Label'),
    '#attributes' => array('placeholder' => t('e.g. Extremely likely')),
    '#suffix'     => '</div>',
  );

  // -----------------------------------------------------------------
  // File settings — file type only.
  // -----------------------------------------------------------------
  $form['scrollable']['file_settings'] = array(
    '#type'       => 'container',
    '#attributes' => array('class' => array('dfb-q-subsection'), 'data-dfb-show-for' => 'file'),
  );
  $form['scrollable']['file_settings']['_title'] = array(
    '#markup' => '<p class="dfb-subsection-label">' . t('File Settings') . '</p>',
  );
  $form['scrollable']['file_settings']['allow_file_types'] = array(
    '#type'        => 'textfield',
    '#title'       => t('Allowed Extensions'),
    '#description' => t('Comma-separated, e.g. pdf,docx,jpg'),
    '#attributes'  => array('placeholder' => 'pdf,docx,jpg'),
  );
  $form['scrollable']['file_settings']['max_file_size_mb'] = array(
    '#type'       => 'textfield',
    '#title'      => t('Max Size (MB)'),
    '#size'       => 6,
    '#attributes' => array('placeholder' => '10'),
    '#prefix'     => '<div class="dfb-form-row dfb-scale-row">',
  );
  $form['scrollable']['file_settings']['max_files'] = array(
    '#type'    => 'select',
    '#title'   => t('Max Files'),
    '#options' => drupal_map_assoc(range(1, 10)),
    '#suffix'  => '</div>',
  );

  // -----------------------------------------------------------------
  // Validation rules — visible only for text-input types.
  // -----------------------------------------------------------------
  $validation_states = array(
    'visible' => array(
      array(':input[name="type"]' => array('value' => 'text')),
      array(':input[name="type"]' => array('value' => 'textarea')),
      array(':input[name="type"]' => array('value' => 'date')),
      array(':input[name="type"]' => array('value' => 'time')),
    ),
  );
  $form['scrollable']['validations_wrapper'] = array(
    '#type'       => 'container',
    '#attributes' => array('id' => 'dfb-q-validations-wrapper', 'class' => array('dfb-q-subsection'), 'data-dfb-show-for' => 'text textarea date time'),
  );
  $rule_types = array(
    ''           => t('— Rule —'),
    'min_length' => t('Min Length'),
    'max_length' => t('Max Length'),
    'min_value'  => t('Min Value'),
    'max_value'  => t('Max Value'),
    'regex'      => t('Regex'),
    'email'      => t('Email'),
    'url'        => t('URL'),
    'numeric'    => t('Numeric Only'),
  );
  // Always show the Validation Rules title.
  $form['scrollable']['validations_wrapper']['_title'] = array(
    '#markup' => '<p class="dfb-subsection-label">' . t('Validation Rules') . '</p>',
  );
  foreach ($form_state['validations'] as $i => $v) {
    $form['scrollable']['validations_wrapper']['val_type_' . $i] = array(
      '#type'          => 'select',
      '#title'         => t('Rule'),
      '#title_display' => 'invisible',
      '#options'       => $rule_types,
      '#default_value' => isset($v['rule_type']) ? $v['rule_type'] : '',
      '#prefix'        => '<div class="dfb-validation-row">',
      '#attributes'    => array('class' => array('dfb-val-type')),
    );
    $form['scrollable']['validations_wrapper']['val_value_' . $i] = array(
      '#type'          => 'textfield',
      '#title'         => t('Value'),
      '#title_display' => 'invisible',
      '#default_value' => isset($v['rule_value']) ? $v['rule_value'] : '',
      '#size'          => 10,
      '#attributes'    => array('placeholder' => t('Value'), 'class' => array('dfb-val-value')),
    );
    $form['scrollable']['validations_wrapper']['val_msg_' . $i] = array(
      '#type'          => 'textfield',
      '#title'         => t('Error Message'),
      '#title_display' => 'invisible',
      '#default_value' => isset($v['error_message']) ? $v['error_message'] : '',
      '#attributes'    => array('placeholder' => t('Error message'), 'class' => array('dfb-val-msg')),
    );
    $form['scrollable']['validations_wrapper']['remove_validation_' . $i] = array(
      '#type'                    => 'submit',
      '#value'                   => t('×'),
      '#name'                    => 'remove_validation_' . $i,
      '#submit'                  => array('_dfb_question_remove_validation_submit'),
      '#limit_validation_errors' => array(),
      '#ajax'                    => array(
        'callback' => '_dfb_question_validations_ajax_callback',
        'wrapper'  => 'dfb-q-validations-wrapper',
        'effect'   => 'none',
      ),
      '#attributes'              => array('class' => array('dfb-btn-remove'), 'title' => t('Remove')),
      '#suffix'                  => '</div>',
    );
  }
  $form['scrollable']['validations_wrapper']['add_validation'] = array(
    '#type'                    => 'submit',
    '#value'                   => t('+ Add Validation'),
    '#name'                    => 'add_validation',
    '#submit'                  => array('_dfb_question_add_validation_submit'),
    '#limit_validation_errors' => array(),
    '#ajax'                    => array(
      'callback' => '_dfb_question_validations_ajax_callback',
      'wrapper'  => 'dfb-q-validations-wrapper',
      'effect'   => 'none',
    ),
    '#attributes'              => array('class' => array('dfb-btn-add-more')),
  );

  // -----------------------------------------------------------------
  // Zone 3 — Action footer (fixed, does not scroll).
  // -----------------------------------------------------------------
  $form['actions'] = array(
    '#type'       => 'container',
    '#attributes' => array('class' => array('dfb-modal-actions')),
  );
  $form['actions']['cancel'] = array(
    '#markup' => '<button type="button" class="dfb-btn-cancel" data-action="close-modal">' . t('Cancel') . '</button>',
  );
  $form['actions']['save'] = array(
    '#type'       => 'submit',
    '#value'      => t('Add Question →'),
    '#attributes' => array('class' => array('dfb-btn-save-question')),
    '#ajax'       => array(
      'callback' => '_dfb_question_form_save_ajax_callback',
    ),
  );

  return $form;
}

/* -----------------------------------------------------------------
   Helpers: persist typed values from $form_state['input'] into the
   options / validations arrays before any structural modification.
   This prevents user-typed content from being lost on add / remove.
   ----------------------------------------------------------------- */

function _dfb_persist_option_values(&$form_state) {
  $input = isset($form_state['input']) ? $form_state['input'] : array();
  foreach ($form_state['options'] as $i => &$opt) {
    if (isset($input['option_' . $i])) {
      $opt['label'] = $input['option_' . $i];
    }
  }
  unset($opt);
}

function _dfb_persist_validation_values(&$form_state) {
  $input = isset($form_state['input']) ? $form_state['input'] : array();
  foreach ($form_state['validations'] as $i => &$v) {
    if (isset($input['val_type_'  . $i])) { $v['rule_type']     = $input['val_type_'  . $i]; }
    if (isset($input['val_value_' . $i])) { $v['rule_value']    = $input['val_value_' . $i]; }
    if (isset($input['val_msg_'   . $i])) { $v['error_message'] = $input['val_msg_'   . $i]; }
  }
  unset($v);
}

/* -----------------------------------------------------------------
   Add / Remove submit handlers — Options
   ----------------------------------------------------------------- */

function _dfb_question_add_option_submit($form, &$form_state) {
  _dfb_persist_option_values($form_state);
  $form_state['options'][] = array('label' => '');
  $form_state['rebuild']   = TRUE;
}

function _dfb_question_remove_option_submit($form, &$form_state) {
  $name = $form_state['triggering_element']['#name'];
  $idx  = (int) str_replace('remove_option_', '', $name);

  _dfb_persist_option_values($form_state);
  array_splice($form_state['options'], $idx, 1);

  // Rewrite $form_state['input'] so the rebuilt fields show the shifted values.
  foreach ($form_state['options'] as $j => $opt) {
    $form_state['input']['option_' . $j] = $opt['label'];
  }
  unset($form_state['input']['option_' . count($form_state['options'])]);

  $form_state['rebuild'] = TRUE;
}

function _dfb_question_options_ajax_callback($form, &$form_state) {
  return $form['scrollable']['options_wrapper'];
}

/* -----------------------------------------------------------------
   Add / Remove submit handlers — Validation rules
   ----------------------------------------------------------------- */

function _dfb_question_add_validation_submit($form, &$form_state) {
  _dfb_persist_validation_values($form_state);
  $form_state['validations'][] = array('rule_type' => '', 'rule_value' => '', 'error_message' => '');
  $form_state['rebuild']       = TRUE;
}

function _dfb_question_remove_validation_submit($form, &$form_state) {
  $name = $form_state['triggering_element']['#name'];
  $idx  = (int) str_replace('remove_validation_', '', $name);

  _dfb_persist_validation_values($form_state);
  array_splice($form_state['validations'], $idx, 1);

  // Rewrite input so rebuilt fields show the shifted values.
  foreach ($form_state['validations'] as $j => $v) {
    $form_state['input']['val_type_'  . $j] = $v['rule_type'];
    $form_state['input']['val_value_' . $j] = $v['rule_value'];
    $form_state['input']['val_msg_'   . $j] = $v['error_message'];
  }
  $last = count($form_state['validations']);
  unset($form_state['input']['val_type_' . $last],
        $form_state['input']['val_value_' . $last],
        $form_state['input']['val_msg_'   . $last]);

  $form_state['rebuild'] = TRUE;
}

function _dfb_question_validations_ajax_callback($form, &$form_state) {
  return $form['scrollable']['validations_wrapper'];
}

/* -----------------------------------------------------------------
   Validate & Submit
   ----------------------------------------------------------------- */

function dynamic_form_builder_question_form_validate($form, &$form_state) {
  if (trim($form_state['values']['label']) === '') {
    form_set_error('label', t('Question label is required.'));
  }
  if (!(int) $form_state['values']['section_id']) {
    form_set_error('section_id', t('No section selected. Close and re-open the dialog.'));
  }
}

function dynamic_form_builder_question_form_submit($form, &$form_state) {
  global $user;
  $now        = REQUEST_TIME;
  $values     = $form_state['values'];
  $section_id = (int) $values['section_id'];
  $form_id    = (int) $form_state['dfb_q_form_id'];
  $type       = $values['type'];

  $q = db_select('dynamic_form_questions', 'q');
  $q->addExpression('MAX(position)', 'max_p');
  $q->condition('form_id', $form_id)
    ->condition('section_id', $section_id)
    ->condition('deleted_at', NULL, 'IS NULL');
  $max_pos  = $q->execute()->fetchField();
  $position = ($max_pos !== NULL) ? ((int) $max_pos + 1) : 0;

  // File settings.
  $allow_file_types = NULL;
  $max_file_size    = NULL;
  $max_files        = NULL;
  if ($type === 'file') {
    $allow_file_types = isset($values['allow_file_types']) ? trim($values['allow_file_types']) : NULL;
    $size_mb          = isset($values['max_file_size_mb'])  ? (int) $values['max_file_size_mb']  : 0;
    $max_file_size    = $size_mb ? $size_mb * 1048576 : NULL;
    $max_files        = isset($values['max_files'])         ? (int) $values['max_files']         : NULL;
  }

  $width_allowed = array('full', 'half', 'third', 'quarter');
  $width = (isset($values['width']) && in_array($values['width'], $width_allowed))
    ? $values['width']
    : 'full';

  $q_fields = array(
    'form_id'          => $form_id,
    'section_id'       => $section_id,
    'label'            => trim($values['label']),
    'description'      => trim(isset($values['description']) ? $values['description'] : ''),
    'type'             => $type,
    'is_required'      => !empty($values['is_required'])  ? 1 : 0,
    'is_readonly'      => !empty($values['is_readonly'])  ? 1 : 0,
    'allow_file_types' => $allow_file_types ?: NULL,
    'max_file_size'    => $max_file_size,
    'max_files'        => $max_files,
    'width'            => $width,
    'position'         => $position,
    'created_by'       => (int) $user->uid,
    'created_at'       => $now,
    'updated_at'       => $now,
  );

  $question_id = db_insert('dynamic_form_questions')->fields($q_fields)->execute();

  _dynamic_form_audit_log('question', $question_id, 'created', NULL, $q_fields);

  // Options for choice types.
  $choice_types = array('radio', 'checkbox', 'select', 'rating', 'linear_scale');
  if (in_array($type, $choice_types)) {
    foreach ($form_state['options'] as $i => $opt) {
      $label = isset($values['option_' . $i]) ? trim($values['option_' . $i]) : '';
      if ($label !== '') {
        db_insert('dynamic_form_question_options')->fields(array(
          'question_id'  => (int) $question_id,
          'option_label' => $label,
          'position'     => $i,
          'created_by'   => (int) $user->uid,
          'created_at'   => $now,
          'updated_at'   => $now,
        ))->execute();
      }
    }
  }

  // Scale settings stored as validation-style meta-rules.
  if (in_array($type, array('rating', 'linear_scale'))) {
    $scale_fields = array(
      'scale_min'       => isset($values['scale_min'])       ? (string) $values['scale_min']       : NULL,
      'scale_max'       => isset($values['scale_max'])       ? (string) $values['scale_max']       : NULL,
      'scale_min_label' => isset($values['scale_min_label']) ? trim($values['scale_min_label'])    : NULL,
      'scale_max_label' => isset($values['scale_max_label']) ? trim($values['scale_max_label'])    : NULL,
    );
    foreach ($scale_fields as $rule_type => $rule_value) {
      if ($rule_value !== NULL && $rule_value !== '') {
        db_insert('dynamic_form_question_validations')->fields(array(
          'question_id' => (int) $question_id,
          'rule_type'   => $rule_type,
          'rule_value'  => $rule_value,
          'created_by'  => (int) $user->uid,
          'created_at'  => $now,
          'updated_at'  => $now,
        ))->execute();
      }
    }
  }

  // Allow-multiple flag for select questions.
  if ($type === 'select' && !empty($values['allow_multiple'])) {
    db_insert('dynamic_form_question_validations')->fields(array(
      'question_id' => (int) $question_id,
      'rule_type'   => 'allow_multiple',
      'rule_value'  => '1',
      'created_by'  => (int) $user->uid,
      'created_at'  => $now,
      'updated_at'  => $now,
    ))->execute();
  }

  // User-defined validation rules.
  foreach ($form_state['validations'] as $i => $v) {
    $rule_type = isset($values['val_type_' . $i]) ? $values['val_type_' . $i] : '';
    if ($rule_type === '') { continue; }
    $rule_value    = isset($values['val_value_' . $i]) ? trim($values['val_value_' . $i]) : NULL;
    $error_message = isset($values['val_msg_'   . $i]) ? trim($values['val_msg_'   . $i]) : NULL;
    db_insert('dynamic_form_question_validations')->fields(array(
      'question_id'   => (int) $question_id,
      'rule_type'     => $rule_type,
      'rule_value'    => $rule_value    ?: NULL,
      'error_message' => $error_message ?: NULL,
      'created_by'    => (int) $user->uid,
      'created_at'    => $now,
      'updated_at'    => $now,
    ))->execute();
  }

  // set form state for next open.
  $q_obj     = (object) $q_fields;
  $q_obj->id = (int) $question_id;
  $form_state['dfb_saved_question']   = $q_obj;
  $form_state['dfb_saved_section_id'] = $section_id;

  $form_state['options']          = array(array('label' => ''), array('label' => ''));
  $form_state['validations']      = array();
  $form_state['input']['width']   = 'full';
  $form_state['rebuild']          = TRUE;
}

/**
 * AJAX callback for the "Add Question" save button.
 */
function _dfb_question_form_save_ajax_callback($form, &$form_state) {
  $commands = array();

  if (form_get_errors()) {
    $commands[] = ajax_command_replace('#dfb-question-form-wrapper', drupal_render($form));
    return array('#type' => 'ajax', '#commands' => $commands);
  }

  $q_obj      = $form_state['dfb_saved_question'];
  $section_id = (int) $form_state['dfb_saved_section_id'];

  $commands[] = ajax_command_append(
    '.dfb-questions-wrapper[data-section-id="' . $section_id . '"]',
    _dynamic_form_render_builder_question($q_obj)
  );
  $commands[] = ajax_command_invoke('#dfb-question-modal', 'fadeOut', array('fast'));
  $commands[] = ajax_command_replace('#dfb-question-form-wrapper', drupal_render($form));
  $commands[] = ajax_command_dfb_toast('success', t('Question added successfully.'));

  return array('#type' => 'ajax', '#commands' => $commands);
}

/**
 * AJAX endpoint: persist a new section or question order after drag-and-drop.
 *
 * Expects a JSON body: { type: 'section'|'question', order: [{id, position, parent_id?}] }
 */
function dynamic_form_ajax_reorder() {
  if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    drupal_json_output(array('status' => 'error', 'message' => 'Invalid request method.'));
    drupal_exit();
  }

  $payload = json_decode(file_get_contents('php://input'), TRUE);

  if (!$payload || !isset($payload['type'], $payload['order'])) {
    drupal_json_output(array('status' => 'error', 'message' => 'Invalid payload.'));
    drupal_exit();
  }

  $type  = $payload['type'];
  $order = $payload['order'];
  $table = ($type === 'section') ? 'dynamic_form_sections' : 'dynamic_form_questions';

  $transaction = db_transaction();
  try {
    foreach ($order as $item) {
      $id       = (int) $item['id'];
      $position = (int) $item['position'];
      $fields   = array('position' => $position, 'updated_at' => REQUEST_TIME);

      if ($type === 'question' && isset($item['parent_id'])) {
        $fields['section_id'] = (int) $item['parent_id'];
      }

      db_update($table)
        ->fields($fields)
        ->condition('id', $id)
        ->execute();
    }
    drupal_json_output(array('status' => 'success'));
  }
  catch (Exception $e) {
    $transaction->rollback();
    watchdog_exception('dynamic_form', $e);
    drupal_json_output(array('status' => 'error', 'message' => 'Database error during reorder.'));
  }

  drupal_exit();
}

/* =====================================================================
   QUESTION EDIT — AJAX ENDPOINT + DRUPAL FORM API
   ===================================================================== */

/**
 * AJAX endpoint: renders the pre-populated edit form for one question.
 *
 * Returns a Drupal AJAX commands array (html + settings) so the JS can
 * inject the form into #dfb-edit-form-placeholder with all Drupal AJAX
 * and #states behaviors properly attached.
 */
function dynamic_form_ajax_question_edit_form($question_id) {
  $question_id = (int) $question_id;
  if (!$question_id) {
    drupal_not_found();
    drupal_exit();
  }

  $html      = drupal_render(drupal_get_form('dynamic_form_builder_question_edit_form', $question_id));
  $commands  = array();
  $commands[] = ajax_command_html('#dfb-edit-form-placeholder', $html);
  print ajax_render($commands);
  drupal_exit();
}

/**
 * Form builder: pre-populated "Edit Question" modal.
 *
 * First render: loads question + options + validations from DB into
 * $form_state. Subsequent rebuilds (add/remove option, save errors)
 * use the already-hydrated state — same pattern as the add form.
 *
 * Reuses the shared add/remove submit handlers (_dfb_question_*_submit)
 * and the shared persist helpers. Only the AJAX wrapper IDs and submit
 * handler differ from the add form.
 */
function dynamic_form_builder_question_edit_form($form, &$form_state, $question_id) {
  $form_state['build_info']['files'][] = array(
    'type'   => 'inc',
    'module' => 'dynamic_form',
    'name'   => 'includes/dynamic_form.builder',
  );

  if (!isset($form_state['dfb_eq_loaded'])) {
    $q = db_select('dynamic_form_questions', 'q')
      ->fields('q')
      ->condition('id', $question_id)
      ->condition('deleted_at', NULL, 'IS NULL')
      ->execute()
      ->fetchObject();

    if (!$q) {
      $form['error'] = array(
        '#markup' => '<div class="dfb-modal-header">'
          . '<div class="dfb-modal-header-text"><h2>' . t('Error') . '</h2></div>'
          . '<button type="button" class="dfb-modal-close" data-action="close-edit-modal">&times;</button>'
          . '</div><p style="padding:24px">' . t('Question not found.') . '</p>',
      );
      return $form;
    }

    $section_name = '';
    if ($q->section_id) {
      $section_name = db_select('dynamic_form_sections', 's')
        ->fields('s', array('name'))
        ->condition('id', $q->section_id)
        ->execute()
        ->fetchField();
    }

    // Existing options.
    $db_options = db_select('dynamic_form_question_options', 'o')
      ->fields('o')
      ->condition('question_id', $question_id)
      ->condition('deleted_at', NULL, 'IS NULL')
      ->orderBy('position', 'ASC')
      ->execute()
      ->fetchAll();
    $form_state['options'] = array();
    foreach ($db_options as $opt) {
      $form_state['options'][] = array('label' => $opt->option_label);
    }
    if (empty($form_state['options'])) {
      $form_state['options'] = array(array('label' => ''), array('label' => ''));
    }

    // Existing validation rules (excluding scale meta + allow_multiple).
    $scale_keys  = array('scale_min', 'scale_max', 'scale_min_label', 'scale_max_label', 'allow_multiple');
    $db_vals = db_select('dynamic_form_question_validations', 'v')
      ->fields('v')
      ->condition('question_id', $question_id)
      ->condition('rule_type', $scale_keys, 'NOT IN')
      ->execute()
      ->fetchAll();
    $form_state['validations'] = array();
    foreach ($db_vals as $val) {
      $form_state['validations'][] = array(
        'rule_type'     => $val->rule_type,
        'rule_value'    => $val->rule_value,
        'error_message' => $val->error_message,
      );
    }

    // Scale meta (rating / linear_scale).
    $scale_meta = array();
    $db_scale = db_select('dynamic_form_question_validations', 'v')
      ->fields('v')
      ->condition('question_id', $question_id)
      ->condition('rule_type', $scale_keys, 'IN')
      ->execute()
      ->fetchAll();
    foreach ($db_scale as $meta) {
      $scale_meta[$meta->rule_type] = $meta->rule_value;
    }

    // Lock type picker if the form already has submitted responses.
    $form_state['dfb_eq_type_locked'] = (bool) db_select('dynamic_form_responses', 'r')
      ->condition('form_id', (int) $q->form_id)
      ->condition('status', 1)
      ->isNull('deleted_at')
      ->countQuery()->execute()->fetchField();

    $form_state['dfb_eq_loaded']       = TRUE;
    $form_state['dfb_eq_question_id']  = (int) $question_id;
    $form_state['dfb_eq_question']     = $q;
    $form_state['dfb_eq_section_name'] = $section_name ?: '';
    $form_state['dfb_eq_scale_meta']   = $scale_meta;
  }

  $q            = $form_state['dfb_eq_question'];
  $section_name = $form_state['dfb_eq_section_name'];
  $scale_meta   = $form_state['dfb_eq_scale_meta'];
  $type_locked  = !empty($form_state['dfb_eq_type_locked']);

  // The #prefix includes the modal header so the injected HTML is self-contained.
  $form['#action'] = url('system/ajax');

  $form['#prefix'] = '<div id="dfb-edit-question-form-wrapper">'
    . '<div class="dfb-modal-header">'
    . '<div class="dfb-modal-header-text">'
    . '<h2>' . t('Edit Question') . '</h2>'
    . '<p class="dfb-modal-header-sub">'
    . t('Section: @name', array('@name' => check_plain($section_name)))
    . '</p>'
    . '</div>'
    . '<button type="button" class="dfb-modal-close" data-action="close-edit-modal">&times;</button>'
    . '</div>';
  $form['#suffix'] = '</div>';

  $form['question_id'] = array('#type' => 'hidden', '#default_value' => (int) $q->id);
  $form['section_id']  = array('#type' => 'hidden', '#default_value' => (int) $q->section_id);

  // -----------------------------------------------------------------
  // Zone 1 — Type picker (pre-selected to current type).
  // -----------------------------------------------------------------
  $form['type_section'] = array(
    '#type'       => 'container',
    '#attributes' => array('class' => array('dfb-type-section')),
  );
  $form['type_section']['type'] = array(
    '#type'          => 'radios',
    '#title'         => t('Question Type'),
    '#title_display' => 'invisible',
    '#required'      => TRUE,
    '#default_value' => $q->type,
    '#options'       => array(
      'text'         => t('Short Text'),
      'textarea'     => t('Long Text'),
      'text_editor'  => t('Text Editor'),
      'radio'        => t('Single Choice'),
      'checkbox'     => t('Multi Choice'),
      'select'       => t('Dropdown'),
      'tags'         => t('Tags'),
      'file'         => t('File Upload'),
      'date'         => t('Date'),
      'time'         => t('Time'),
      'rating'       => t('Rating'),
      'linear_scale' => t('Scale'),
    ),
    '#attributes'    => array('class' => array('dfb-type-picker')),
    '#disabled'      => $type_locked,
  );

  if ($type_locked) {
    $form['type_section']['type_lock_notice'] = array(
      '#markup' => '<div class="dfb-type-locked-notice">'
        . '<span class="dfb-lock-icon">&#128274;</span> '
        . t('This form already has responses. The question type cannot be changed to preserve data integrity.')
        . '</div>',
      '#weight' => 10,
    );
  }

  $form['sep'] = array('#markup' => '<hr class="dfb-modal-separator">');

  // -----------------------------------------------------------------
  // Zone 2 — Scrollable body.
  // -----------------------------------------------------------------
  $form['scrollable'] = array(
    '#type'       => 'container',
    '#attributes' => array('class' => array('dfb-form-scrollable')),
  );

  $form['scrollable']['label'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Question Label'),
    '#required'      => TRUE,
    '#maxlength'     => 512,
    '#default_value' => $q->label,
    '#prefix'        => '<div class="dfb-form-row">',
  );
  $form['scrollable']['section_display'] = array(
    '#type'   => 'item',
    '#title'  => t('Section'),
    '#markup' => '<span class="dfb-section-name-val">' . check_plain($section_name) . '</span>',
    '#suffix' => '</div>',
  );

  $form['scrollable']['is_required'] = array(
    '#type'          => 'checkbox',
    '#title'         => t('Required'),
    '#default_value' => $q->is_required,
    '#prefix'        => '<div class="dfb-toggle-row">',
  );
  $form['scrollable']['is_readonly'] = array(
    '#type'          => 'checkbox',
    '#title'         => t('Read-only'),
    '#default_value' => $q->is_readonly,
    '#suffix'        => '</div>',
  );

  $form['scrollable']['description'] = array(
    '#type'          => 'textarea',
    '#title'         => t('Help Text'),
    '#rows'          => 2,
    '#default_value' => $q->description,
    '#attributes'    => array('placeholder' => t('Optional — shown to respondents.')),
  );

  // Width picker — pre-selected to the question's saved width.
  // On AJAX rebuilds (add/remove option), $form_state['input']['width'] keeps
  // the user's in-progress choice alive across re-renders.
  $current_width = (isset($form_state['input']['width']) && in_array($form_state['input']['width'], array('full', 'half', 'third', 'quarter')))
    ? $form_state['input']['width']
    : ((!empty($q->width) && in_array($q->width, array('full', 'half', 'third', 'quarter'))) ? $q->width : 'full');
  $form['scrollable']['width_picker'] = array(
    '#markup' => _dfb_width_picker_markup($current_width),
  );
  $form['scrollable']['width'] = array(
    '#type'          => 'hidden',
    '#default_value' => $current_width,
    '#attributes'    => array('class' => array('dfb-width-value')),
  );

  // Text editor info — shown only when text_editor type is selected.
  $form['scrollable']['text_editor_info'] = array(
    '#markup' =>
      '<div class="dfb-q-subsection dfb-text-editor-info" data-dfb-show-for="text_editor">' .
        '<p class="dfb-subsection-label">' . t('Rich Text Editor') . '</p>' .
        '<p class="dfb-text-editor-hint">' .
          t('Respondents will see a TinyMCE rich text editor for their answer. No additional configuration needed.') .
        '</p>' .
      '</div>',
  );

  // Tags info — shown only when tags type is selected.
  $form['scrollable']['tags_info'] = array(
    '#markup' =>
      '<div class="dfb-q-subsection dfb-tags-info" data-dfb-show-for="tags">' .
        '<p class="dfb-subsection-label">' . t('Tags') . '</p>' .
        '<p class="dfb-text-editor-hint">' .
          t('Respondents can search existing tags or type new ones. Tags are automatically normalized (lowercase, symbols removed) for analytics.') .
        '</p>' .
      '</div>',
  );

  // Select settings — allow multiple choice. Shown only for select type.
  $form['scrollable']['select_settings'] = array(
    '#type'       => 'container',
    '#attributes' => array('class' => array('dfb-q-subsection'), 'data-dfb-show-for' => 'select'),
  );
  $form['scrollable']['select_settings']['allow_multiple'] = array(
    '#type'          => 'checkbox',
    '#title'         => t('Allow Multiple Choice'),
    '#default_value' => !empty($scale_meta['allow_multiple']) ? 1 : 0,
  );

  // -----------------------------------------------------------------
  // Options sub-section. Uses dfb-eq-* IDs to avoid conflicts with
  // the simultaneously-rendered add form.
  // -----------------------------------------------------------------
  $choice_states = array(
    'visible' => array(
      array(':input[name="type"]' => array('value' => 'radio')),
      array(':input[name="type"]' => array('value' => 'checkbox')),
      array(':input[name="type"]' => array('value' => 'select')),
    ),
  );
  $form['scrollable']['options_wrapper'] = array(
    '#type'       => 'container',
    '#attributes' => array('id' => 'dfb-eq-edit-options-wrapper', 'class' => array('dfb-q-subsection'), 'data-dfb-show-for' => 'radio checkbox select'),
  );
  $form['scrollable']['options_wrapper']['_title'] = array(
    '#markup' => '<p class="dfb-subsection-label">' . t('Options') . '</p>',
  );
  foreach ($form_state['options'] as $i => $opt) {
    $form['scrollable']['options_wrapper']['option_' . $i] = array(
      '#type'          => 'textfield',
      '#title'         => t('Option @n', array('@n' => $i + 1)),
      '#title_display' => 'invisible',
      '#default_value' => isset($opt['label']) ? $opt['label'] : '',
      '#attributes'    => array('placeholder' => t('Option @n', array('@n' => $i + 1))),
      '#prefix'        => '<div class="dfb-option-row">',
    );
    $form['scrollable']['options_wrapper']['remove_option_' . $i] = array(
      '#type'                    => 'submit',
      '#value'                   => t('×'),
      '#name'                    => 'remove_option_' . $i,
      '#id'                      => 'dfb-eq-edit-remove-option-' . $i,
      '#submit'                  => array('_dfb_question_remove_option_submit'),
      '#limit_validation_errors' => array(),
      '#ajax'                    => array(
        'callback' => '_dfb_eq_options_ajax_callback',
        'wrapper'  => 'dfb-eq-edit-options-wrapper',
        'effect'   => 'none',
      ),
      '#attributes'              => array('class' => array('dfb-btn-remove'), 'title' => t('Remove')),
      '#suffix'                  => '</div>',
    );
  }
  $form['scrollable']['options_wrapper']['add_option'] = array(
    '#type'                    => 'submit',
    '#value'                   => t('+ Add Option'),
    '#name'                    => 'add_option',
    '#id'                      => 'dfb-eq-edit-add-option',
    '#submit'                  => array('_dfb_question_add_option_submit'),
    '#limit_validation_errors' => array(),
    '#ajax'                    => array(
      'callback' => '_dfb_eq_options_ajax_callback',
      'wrapper'  => 'dfb-eq-edit-options-wrapper',
      'effect'   => 'none',
    ),
    '#attributes'              => array('class' => array('dfb-btn-add-more')),
  );

  // -----------------------------------------------------------------
  // Scale settings.
  // -----------------------------------------------------------------
  $form['scrollable']['scale_settings'] = array(
    '#type'       => 'container',
    '#attributes' => array('class' => array('dfb-q-subsection'), 'data-dfb-show-for' => 'rating linear_scale'),
  );
  $form['scrollable']['scale_settings']['_title'] = array(
    '#markup' => '<p class="dfb-subsection-label">' . t('Scale Range') . '</p>',
  );
  $form['scrollable']['scale_settings']['scale_min'] = array(
    '#type'          => 'select',
    '#title'         => t('Min'),
    '#options'       => drupal_map_assoc(range(0, 5)),
    '#default_value' => isset($scale_meta['scale_min']) ? (int) $scale_meta['scale_min'] : 1,
    '#prefix'        => '<div class="dfb-form-row dfb-scale-row">',
  );
  $form['scrollable']['scale_settings']['scale_max'] = array(
    '#type'          => 'select',
    '#title'         => t('Max'),
    '#options'       => drupal_map_assoc(range(2, 10)),
    '#default_value' => isset($scale_meta['scale_max']) ? (int) $scale_meta['scale_max'] : 5,
    '#suffix'        => '</div>',
  );
  $form['scrollable']['scale_settings']['scale_min_label'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Min Label'),
    '#default_value' => isset($scale_meta['scale_min_label']) ? $scale_meta['scale_min_label'] : '',
    '#attributes'    => array('placeholder' => t('e.g. Not at all')),
    '#prefix'        => '<div class="dfb-form-row dfb-scale-row">',
  );
  $form['scrollable']['scale_settings']['scale_max_label'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Max Label'),
    '#default_value' => isset($scale_meta['scale_max_label']) ? $scale_meta['scale_max_label'] : '',
    '#attributes'    => array('placeholder' => t('e.g. Extremely likely')),
    '#suffix'        => '</div>',
  );

  // -----------------------------------------------------------------
  // File settings.
  // -----------------------------------------------------------------
  $form['scrollable']['file_settings'] = array(
    '#type'       => 'container',
    '#attributes' => array('class' => array('dfb-q-subsection'), 'data-dfb-show-for' => 'file'),
  );
  $form['scrollable']['file_settings']['_title'] = array(
    '#markup' => '<p class="dfb-subsection-label">' . t('File Settings') . '</p>',
  );
  $form['scrollable']['file_settings']['allow_file_types'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Allowed Extensions'),
    '#description'   => t('Comma-separated, e.g. pdf,docx,jpg'),
    '#default_value' => $q->allow_file_types ? $q->allow_file_types : '',
    '#attributes'    => array('placeholder' => 'pdf,docx,jpg'),
  );
  $size_mb_default = $q->max_file_size ? (int) ($q->max_file_size / 1048576) : '';
  $form['scrollable']['file_settings']['max_file_size_mb'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Max Size (MB)'),
    '#size'          => 6,
    '#default_value' => $size_mb_default,
    '#attributes'    => array('placeholder' => '10'),
    '#prefix'        => '<div class="dfb-form-row dfb-scale-row">',
  );
  $form['scrollable']['file_settings']['max_files'] = array(
    '#type'          => 'select',
    '#title'         => t('Max Files'),
    '#options'       => drupal_map_assoc(range(1, 10)),
    '#default_value' => $q->max_files ? (int) $q->max_files : 1,
    '#suffix'        => '</div>',
  );

  // -----------------------------------------------------------------
  // Validation rules.
  // -----------------------------------------------------------------
  $rule_types = array(
    ''           => t('— Rule —'),
    'min_length' => t('Min Length'),
    'max_length' => t('Max Length'),
    'min_value'  => t('Min Value'),
    'max_value'  => t('Max Value'),
    'regex'      => t('Regex'),
    'email'      => t('Email'),
    'url'        => t('URL'),
    'numeric'    => t('Numeric Only'),
  );
  $validation_states = array(
    'visible' => array(
      array(':input[name="type"]' => array('value' => 'text')),
      array(':input[name="type"]' => array('value' => 'textarea')),
      array(':input[name="type"]' => array('value' => 'date')),
      array(':input[name="type"]' => array('value' => 'time')),
    ),
  );
  $form['scrollable']['validations_wrapper'] = array(
    '#type'       => 'container',
    '#attributes' => array('id' => 'dfb-eq-edit-validations-wrapper', 'class' => array('dfb-q-subsection'), 'data-dfb-show-for' => 'text textarea date time'),
  );
  $form['scrollable']['validations_wrapper']['_title'] = array(
    '#markup' => '<p class="dfb-subsection-label">' . t('Validation Rules') . '</p>',
  );
  foreach ($form_state['validations'] as $i => $v) {
    $form['scrollable']['validations_wrapper']['val_type_' . $i] = array(
      '#type'          => 'select',
      '#title'         => t('Rule'),
      '#title_display' => 'invisible',
      '#options'       => $rule_types,
      '#default_value' => isset($v['rule_type']) ? $v['rule_type'] : '',
      '#prefix'        => '<div class="dfb-validation-row">',
      '#attributes'    => array('class' => array('dfb-val-type')),
    );
    $form['scrollable']['validations_wrapper']['val_value_' . $i] = array(
      '#type'          => 'textfield',
      '#title'         => t('Value'),
      '#title_display' => 'invisible',
      '#default_value' => isset($v['rule_value']) ? $v['rule_value'] : '',
      '#size'          => 10,
      '#attributes'    => array('placeholder' => t('Value'), 'class' => array('dfb-val-value')),
    );
    $form['scrollable']['validations_wrapper']['val_msg_' . $i] = array(
      '#type'          => 'textfield',
      '#title'         => t('Error Message'),
      '#title_display' => 'invisible',
      '#default_value' => isset($v['error_message']) ? $v['error_message'] : '',
      '#attributes'    => array('placeholder' => t('Error message'), 'class' => array('dfb-val-msg')),
    );
    $form['scrollable']['validations_wrapper']['remove_validation_' . $i] = array(
      '#type'                    => 'submit',
      '#value'                   => t('×'),
      '#name'                    => 'remove_validation_' . $i,
      '#id'                      => 'dfb-eq-edit-remove-validation-' . $i,
      '#submit'                  => array('_dfb_question_remove_validation_submit'),
      '#limit_validation_errors' => array(),
      '#ajax'                    => array(
        'callback' => '_dfb_eq_validations_ajax_callback',
        'wrapper'  => 'dfb-eq-edit-validations-wrapper',
        'effect'   => 'none',
      ),
      '#attributes'              => array('class' => array('dfb-btn-remove'), 'title' => t('Remove')),
      '#suffix'                  => '</div>',
    );
  }
  $form['scrollable']['validations_wrapper']['add_validation'] = array(
    '#type'                    => 'submit',
    '#value'                   => t('+ Add Validation'),
    '#name'                    => 'add_validation',
    '#id'                      => 'dfb-eq-edit-add-validation',
    '#submit'                  => array('_dfb_question_add_validation_submit'),
    '#limit_validation_errors' => array(),
    '#ajax'                    => array(
      'callback' => '_dfb_eq_validations_ajax_callback',
      'wrapper'  => 'dfb-eq-edit-validations-wrapper',
      'effect'   => 'none',
    ),
    '#attributes'              => array('class' => array('dfb-btn-add-more')),
  );

  // -----------------------------------------------------------------
  // Zone 3 — Action footer.
  // -----------------------------------------------------------------
  $form['actions'] = array(
    '#type'       => 'container',
    '#attributes' => array('class' => array('dfb-modal-actions')),
  );
  $form['actions']['cancel'] = array(
    '#markup' => '<button type="button" class="dfb-btn-cancel" data-action="close-edit-modal">'
      . t('Cancel') . '</button>',
  );
  $form['actions']['save'] = array(
    '#type'       => 'submit',
    '#value'      => t('Save Changes'),
    '#id'         => 'dfb-eq-edit-save',
    '#attributes' => array('class' => array('dfb-btn-save-question')),
    '#ajax'       => array(
      'callback' => '_dfb_question_edit_save_ajax_callback',
    ),
  );

  return $form;
}

/* AJAX callbacks for the edit form's dynamic wrappers.
 * These return the same elements as the add-form callbacks but target the
 * edit-form's uniquely-IDed containers so the DOM replacement is correct. */

function _dfb_eq_options_ajax_callback($form, &$form_state) {
  return $form['scrollable']['options_wrapper'];
}

function _dfb_eq_validations_ajax_callback($form, &$form_state) {
  return $form['scrollable']['validations_wrapper'];
}

/**
 * Validate handler for the edit form.
 */
function dynamic_form_builder_question_edit_form_validate($form, &$form_state) {
  if (trim($form_state['values']['label']) === '') {
    form_set_error('label', t('Question label is required.'));
  }
}

/**
 * Submit handler: UPDATE question + soft-delete/re-insert options
 * + hard-delete/re-insert validations.
 */
function dynamic_form_builder_question_edit_form_submit($form, &$form_state) {
  global $user;
  $now         = REQUEST_TIME;
  $values      = $form_state['values'];
  $question_id = (int) $form_state['dfb_eq_question_id'];

  // If the form has responses, enforce the original type regardless of input.
  $type = !empty($form_state['dfb_eq_type_locked'])
    ? $form_state['dfb_eq_question']->type
    : $values['type'];

  $allow_file_types = NULL;
  $max_file_size    = NULL;
  $max_files        = NULL;
  if ($type === 'file') {
    $allow_file_types = isset($values['allow_file_types']) ? trim($values['allow_file_types']) : NULL;
    $size_mb          = isset($values['max_file_size_mb'])  ? (int) $values['max_file_size_mb']  : 0;
    $max_file_size    = $size_mb ? $size_mb * 1048576 : NULL;
    $max_files        = isset($values['max_files'])         ? (int) $values['max_files']         : NULL;
  }

  $width_allowed = array('full', 'half', 'third', 'quarter');
  $width = (isset($values['width']) && in_array($values['width'], $width_allowed))
    ? $values['width']
    : 'full';

  $new_q_fields = array(
    'label'            => trim($values['label']),
    'description'      => trim(isset($values['description']) ? $values['description'] : ''),
    'type'             => $type,
    'is_required'      => !empty($values['is_required']) ? 1 : 0,
    'is_readonly'      => !empty($values['is_readonly'])  ? 1 : 0,
    'allow_file_types' => $allow_file_types ?: NULL,
    'max_file_size'    => $max_file_size,
    'max_files'        => $max_files,
    'width'            => $width,
    'updated_at'       => $now,
  );

  db_update('dynamic_form_questions')
    ->fields($new_q_fields)
    ->condition('id', $question_id)
    ->execute();

  _dynamic_form_audit_log('question', $question_id, 'updated', (array) $form_state['dfb_eq_question'], $new_q_fields);

  // Soft-delete existing options then re-insert survivors.
  db_update('dynamic_form_question_options')
    ->fields(array('deleted_at' => $now, 'deleted_by' => (int) $user->uid))
    ->condition('question_id', $question_id)
    ->condition('deleted_at', NULL, 'IS NULL')
    ->execute();

  $choice_types = array('radio', 'checkbox', 'select', 'rating', 'linear_scale');
  if (in_array($type, $choice_types)) {
    foreach ($form_state['options'] as $i => $opt) {
      $label = isset($values['option_' . $i]) ? trim($values['option_' . $i]) : '';
      if ($label !== '') {
        db_insert('dynamic_form_question_options')->fields(array(
          'question_id'  => $question_id,
          'option_label' => $label,
          'position'     => $i,
          'created_by'   => (int) $user->uid,
          'created_at'   => $now,
          'updated_at'   => $now,
        ))->execute();
      }
    }
  }

  // Hard-delete all validation rows then re-insert.
  db_delete('dynamic_form_question_validations')
    ->condition('question_id', $question_id)
    ->execute();

  if (in_array($type, array('rating', 'linear_scale'))) {
    $scale_fields = array(
      'scale_min'       => isset($values['scale_min'])       ? (string) $values['scale_min']    : NULL,
      'scale_max'       => isset($values['scale_max'])       ? (string) $values['scale_max']    : NULL,
      'scale_min_label' => isset($values['scale_min_label']) ? trim($values['scale_min_label']) : NULL,
      'scale_max_label' => isset($values['scale_max_label']) ? trim($values['scale_max_label']) : NULL,
    );
    foreach ($scale_fields as $rule_type => $rule_value) {
      if ($rule_value !== NULL && $rule_value !== '') {
        db_insert('dynamic_form_question_validations')->fields(array(
          'question_id' => $question_id,
          'rule_type'   => $rule_type,
          'rule_value'  => $rule_value,
          'created_by'  => (int) $user->uid,
          'created_at'  => $now,
          'updated_at'  => $now,
        ))->execute();
      }
    }
  }

  foreach ($form_state['validations'] as $i => $v) {
    $rule_type = isset($values['val_type_' . $i]) ? $values['val_type_' . $i] : '';
    if ($rule_type === '') { continue; }
    db_insert('dynamic_form_question_validations')->fields(array(
      'question_id'   => $question_id,
      'rule_type'     => $rule_type,
      'rule_value'    => isset($values['val_value_' . $i]) ? (trim($values['val_value_' . $i]) ?: NULL) : NULL,
      'error_message' => isset($values['val_msg_'   . $i]) ? (trim($values['val_msg_'   . $i]) ?: NULL) : NULL,
      'created_by'    => (int) $user->uid,
      'created_at'    => $now,
      'updated_at'    => $now,
    ))->execute();
  }

  // Allow-multiple flag for select questions.
  if ($type === 'select' && !empty($values['allow_multiple'])) {
    db_insert('dynamic_form_question_validations')->fields(array(
      'question_id' => $question_id,
      'rule_type'   => 'allow_multiple',
      'rule_value'  => '1',
      'created_by'  => (int) $user->uid,
      'created_at'  => $now,
      'updated_at'  => $now,
    ))->execute();
  }

  // Fetch the updated row so the AJAX callback can re-render the card.
  $form_state['dfb_eq_saved_question'] = db_select('dynamic_form_questions', 'q')
    ->fields('q')
    ->condition('id', $question_id)
    ->execute()
    ->fetchObject();

  $form_state['rebuild'] = TRUE;
}

/**
 * AJAX callback for the "Save Changes" button.
 *
 * On success: replaces the question card in the builder, closes the
 * edit modal, then resets the form wrapper for the next edit.
 * On error: re-renders the form in place so error messages appear.
 */
function _dfb_question_edit_save_ajax_callback($form, &$form_state) {
  $commands = array();

  if (form_get_errors()) {
    $commands[] = ajax_command_replace(
      '#dfb-edit-question-form-wrapper',
      drupal_render($form)
    );
    return array('#type' => 'ajax', '#commands' => $commands);
  }

  $q = $form_state['dfb_eq_saved_question'];
  $commands[] = ajax_command_replace(
    '.dfb-question-card[data-question-id="' . (int) $q->id . '"]',
    _dynamic_form_render_builder_question($q)
  );
  $commands[] = ajax_command_invoke('#dfb-edit-question-modal', 'fadeOut', array('fast'));
  $commands[] = ajax_command_replace(
    '#dfb-edit-question-form-wrapper',
    drupal_render($form)
  );
  $commands[] = ajax_command_dfb_toast('success', t('Question updated successfully.'));

  return array('#type' => 'ajax', '#commands' => $commands);
}

/**
 * AJAX endpoint: rename a section inline.
 *
 * Accepts POST body: { "name": "New Section Name" }
 * Returns JSON: { "status": "success", "name": "New Section Name" }
 *             | { "status": "error",   "message": "..." }
 */
function dynamic_form_ajax_section_rename($section_id) {
  $section_id = (int) $section_id;
  $body       = file_get_contents('php://input');
  $data       = json_decode($body, TRUE);
  $name       = isset($data['name']) ? trim($data['name']) : '';

  if ($name === '') {
    drupal_json_output(array('status' => 'error', 'message' => t('Section name cannot be empty.')));
    drupal_exit();
  }
  if (drupal_strlen($name) > 255) {
    drupal_json_output(array('status' => 'error', 'message' => t('Section name is too long.')));
    drupal_exit();
  }

  $section = db_select('dynamic_form_sections', 's')
    ->fields('s', array('id', 'form_id', 'name'))
    ->condition('s.id', $section_id)
    ->isNull('s.deleted_at')
    ->execute()
    ->fetchObject();

  if (!$section) {
    drupal_json_output(array('status' => 'error', 'message' => t('Section not found.')));
    drupal_exit();
  }

  db_update('dynamic_form_sections')
    ->fields(array('name' => $name, 'updated_at' => REQUEST_TIME))
    ->condition('id', $section_id)
    ->execute();

  _dynamic_form_audit_log(
    'section',
    $section_id,
    'updated',
    array('name' => $section->name),
    array('name' => $name)
  );

  drupal_json_output(array('status' => 'success', 'name' => $name));
  drupal_exit();
}
