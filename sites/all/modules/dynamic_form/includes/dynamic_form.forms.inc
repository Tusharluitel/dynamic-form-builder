<?php

/**
 * @file
 * Form CRUD page callbacks and forms for the Dynamic Form module.
 *
 * Contains create, list, edit, and delete (soft) form builders,
 * validators, and submit handlers.
 */

/* =====================================================================
   FORM CRUD — CREATE
   ===================================================================== */

/**
 * Form builder: Create a new form.
 */
function dynamic_form_create_form($form, &$form_state)
{
  module_load_include('inc', 'dynamic_form', 'includes/dynamic_form.members');

  $module_path = drupal_get_path('module', 'dynamic_form');
  $form['#attached']['css'][] = $module_path . '/css/members.css';
  $form['#attached']['js'][]  = $module_path . '/js/members.js';
  $form['#attached']['js'][]  = array(
    'data' => array(
      'dynamicFormMembers' => array(
        'searchUrl' => url('dynamic-form/ajax/members/search'),
      ),
    ),
    'type' => 'setting',
  );
  // Swap rendered text inputs to datetime-local after Drupal renders them.
  $form['#attached']['js'][] = array(
    'data' => '(function($){Drupal.behaviors.dfbDatetimePicker={attach:function(ctx){$("input.dfb-datetime-input",ctx).each(function(){var o=$(this),n=document.createElement("input");n.type="datetime-local";n.name=o.attr("name");n.id=o.attr("id");n.value=o.val();n.className=this.className;o.replaceWith(n);});}};})(jQuery);',
    'type' => 'inline',
  );

  // Wrapper for dashboard styling.
  $form['#prefix'] = '<div class="dfb-form-wrapper">';
  $form['#suffix'] = '</div>';

  // -- Basic Info section --
  $form['basic_info'] = array(
    '#type' => 'fieldset',
    '#title' => t('Basic Information'),
    '#attributes' => array('class' => array('dfb-form-section')),
  );

  $form['basic_info']['title'] = array(
    '#type' => 'textfield',
    '#title' => t('Form Title'),
    '#required' => TRUE,
    '#maxlength' => 255,
    '#attributes' => array('placeholder' => t('e.g. Customer Feedback Survey')),
    '#description' => t('A clear, descriptive name for your form.'),
  );

  $form['basic_info']['description'] = array(
    '#type' => 'textarea',
    '#title' => t('Description'),
    '#required' => FALSE,
    '#rows' => 3,
    '#attributes' => array('placeholder' => t('Briefly describe the purpose of this form...')),
    '#description' => t('Optional. Shown to respondents before they begin.'),
  );

  // -- Settings section --
  $form['settings'] = array(
    '#type' => 'fieldset',
    '#title' => t('Settings'),
    '#attributes' => array('class' => array('dfb-form-section')),
  );

  $form['settings']['status'] = array(
    '#type' => 'select',
    '#title' => t('Status'),
    '#options' => array(
      0 => t('Draft'),
      1 => t('Published'),
      2 => t('Closed'),
    ),
    '#default_value' => 0,
    '#description' => t('Draft forms are not visible to respondents.'),
  );

  $form['settings']['visibility'] = array(
    '#type' => 'select',
    '#title' => t('Visibility'),
    '#options' => array(
      'p' => t('Public'),
      'r' => t('Restricted'),
      'm' => t('Members Only'),
    ),
    '#default_value' => 'p',
    '#description' => t('Control who can access and fill out this form.'),
  );

  $form['settings']['question_per_page'] = array(
    '#type' => 'textfield',
    '#title' => t('Questions Per Page'),
    '#default_value' => 1,
    '#size' => 5,
    '#attributes' => array('placeholder' => t('e.g. 10')),
    '#description' => t('Number of questions shown per page when filling out the form.'),
  );

  // -- Schedule section --
  $form['schedule'] = array(
    '#type' => 'fieldset',
    '#title' => t('Schedule'),
    '#attributes' => array('class' => array('dfb-form-section')),
    '#description' => t('Optionally set when the form opens and closes for responses.'),
  );

  $form['schedule']['opens_at'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Opens At'),
    '#required'      => FALSE,
    '#default_value' => '',
    '#attributes'    => array('class' => array('dfb-datetime-input')),
    '#description'   => t('Leave blank to open immediately.'),
  );

  $form['schedule']['closes_at'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Closes At'),
    '#required'      => FALSE,
    '#default_value' => '',
    '#attributes'    => array('class' => array('dfb-datetime-input')),
    '#description'   => t('Leave blank for no expiry.'),
  );

  // -- Members section --
  $form['members'] = array(
    '#type'        => 'fieldset',
    '#title'       => t('Form Members'),
    '#attributes'  => array('class' => array('dfb-form-section')),
    '#description' => t('Add editors and form applicants. They will be notified by email when added.'),
  );

  $form['members']['members_search'] = array(
    '#type'       => 'textfield',
    '#title'      => t('Search Users'),
    '#prefix'     => '<div class="dfb-member-picker-wrap">',
    '#suffix'     => '<div id="dfb-members-results" class="dfb-members-results" style="display:none;"></div></div>',
    '#attributes' => array(
      'id'           => 'dfb-members-search',
      'placeholder'  => t('Type a name or email to search…'),
      'autocomplete' => 'off',
    ),
    '#description' => t('Search by name or email. Click a result to add the user.'),
  );

  $form['members']['members_selected'] = array(
    '#type'          => 'hidden',
    '#default_value' => '[]',
    '#attributes'    => array('id' => 'dfb-members-selected'),
  );

  $form['members']['members_chips_wrap'] = array(
    '#markup' => '<div id="dfb-members-chips" class="dfb-members-chips"></div>',
  );

  // -- Actions --
  $form['actions'] = array(
    '#type' => 'container',
    '#attributes' => array('class' => array('dfb-form-actions')),
  );

  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Create Form'),
    '#attributes' => array('class' => array('dfb-btn-submit')),
  );

  $form['actions']['cancel'] = array(
    '#markup' => l(t('Cancel'), 'dashboard/forms', array(
      'attributes' => array('class' => array('dfb-btn-cancel')),
    )),
  );

  return $form;
}

/**
 * Validate: Create form.
 */
function dynamic_form_create_form_validate($form, &$form_state)
{
  $title = trim($form_state['values']['title']);

  if (drupal_strlen($title) < 3) {
    form_set_error('title', t('Title must be at least 3 characters long.'));
  }

  // Generate slug and check uniqueness.
  $slug = _dynamic_form_generate_slug($title);
  $exists = db_select('dynamic_form_forms', 'f')
    ->fields('f', array('id'))
    ->condition('slug', $slug)
    ->condition('deleted_at', NULL, 'IS NULL')
    ->execute()
    ->fetchField();

  if ($exists) {
    form_set_error('title', t('A form with a similar title already exists. Please choose a different title.'));
  }
}

/**
 * Submit: Create form.
 */
function dynamic_form_create_form_submit($form, &$form_state)
{
  global $user;
  module_load_include('inc', 'dynamic_form', 'includes/dynamic_form.members');

  $values = $form_state['values'];
  $now = REQUEST_TIME;

  $slug = _dynamic_form_generate_slug(trim($values['title']));
  $status = (int) $values['status'];

  // Parse date fields.
  $closes_at = _dynamic_form_parse_date($values['closes_at']);
  $opens_at = _dynamic_form_parse_date($values['opens_at']);

  $fields = array(
    'title' => trim($values['title']),
    'description' => trim($values['description']),
    'status' => $status,
    'slug' => $slug,
    'question_per_page' => (int) $values['question_per_page'],
    'visibility' => $values['visibility'],
    'closes_at' => $closes_at,
    'opens_at' => $opens_at,
    'created_by' => $user->uid,
    'created_at' => $now,
    'updated_at' => $now,
  );

  // Set published_at and published_by when publishing immediately.
  if ($status === 1) {
    $fields['published_at'] = $now;
    $fields['published_by'] = $user->uid;
  }

  $id = db_insert('dynamic_form_forms')
    ->fields($fields)
    ->execute();

  _dynamic_form_audit_log('form', $id, 'created', NULL, $fields);

  // Save members and queue email notifications for newly added ones.
  $members_json = isset($values['members_selected']) ? $values['members_selected'] : '[]';
  $newly_added  = _dynamic_form_save_members($id, $members_json, TRUE);
  _dynamic_form_queue_member_notifications($id, trim($values['title']), $newly_added);

  drupal_set_message(t('Form "@title" has been created.', array('@title' => $values['title'])));
  $form_state['redirect'] = 'dashboard/forms';
}

/* =====================================================================
   FORM CRUD — LIST
   ===================================================================== */

/**
 * Page callback: List all forms for the current user.
 */
function dynamic_form_list_page()
{
  global $user;

  $query = db_select('dynamic_form_forms', 'f')
    ->fields('f', array('id', 'title', 'slug', 'status', 'visibility', 'created_at', 'created_by'))
    ->condition('f.deleted_at', NULL, 'IS NULL')
    ->orderBy('f.created_at', 'DESC');

  // Non-admin users see only their own forms.
  if (!user_access('edit any form')) {
    $query->condition('f.created_by', $user->uid);
  }

  $results = $query->execute()->fetchAll();

  if (empty($results)) {
    return '<p>' . t('You have not created any forms yet. <a href="!url">Create your first form</a>.', array('!url' => url('form/create'))) . '</p>';
  }

  $status_labels = array(
    0 => t('Draft'),
    1 => t('Published'),
    2 => t('Closed'),
  );

  $visibility_labels = array(
    'p' => t('Public'),
    'r' => t('Restricted'),
    'm' => t('Members Only'),
  );

  $header = array(
    t('Title'),
    t('Status'),
    t('Visibility'),
    t('Created'),
    t('Actions'),
  );

  $rows = array();
  foreach ($results as $record) {
    $actions = array();
    $actions[] = l(t('Edit'), 'form/' . $record->id . '/edit');
    $actions[] = l(t('Delete'), 'form/' . $record->id . '/delete');

    $rows[] = array(
      check_plain($record->title),
      isset($status_labels[$record->status]) ? $status_labels[$record->status] : t('Unknown'),
      isset($visibility_labels[$record->visibility]) ? $visibility_labels[$record->visibility] : $record->visibility,
      format_date($record->created_at, 'short'),
      implode(' | ', $actions),
    );
  }

  return theme('table', array(
    'header' => $header,
    'rows' => $rows,
    'attributes' => array('class' => array('dfb-forms-table')),
    'empty' => t('No forms found.'),
  ));
}

/* =====================================================================
   FORM CRUD — EDIT
   ===================================================================== */

/**
 * Form builder: Edit an existing form.
 */
function dynamic_form_edit_form($form, &$form_state, $form_id)
{
  module_load_include('inc', 'dynamic_form', 'includes/dynamic_form.members');

  $module_path = drupal_get_path('module', 'dynamic_form');
  $form['#attached']['css'][] = $module_path . '/css/members.css';
  $form['#attached']['js'][]  = $module_path . '/js/members.js';
  $form['#attached']['js'][]  = array(
    'data' => array(
      'dynamicFormMembers' => array(
        'searchUrl' => url('dynamic-form/ajax/members/search'),
      ),
    ),
    'type' => 'setting',
  );
  $form['#attached']['js'][] = array(
    'data' => '(function($){Drupal.behaviors.dfbDatetimePicker={attach:function(ctx){$("input.dfb-datetime-input",ctx).each(function(){var o=$(this),n=document.createElement("input");n.type="datetime-local";n.name=o.attr("name");n.id=o.attr("id");n.value=o.val();n.className=this.className;o.replaceWith(n);});}};})(jQuery);',
    'type' => 'inline',
  );

  $record = db_select('dynamic_form_forms', 'f')
    ->fields('f')
    ->condition('id', $form_id)
    ->condition('deleted_at', NULL, 'IS NULL')
    ->execute()
    ->fetchObject();

  if (!$record) {
    drupal_set_message(t('Form not found.'), 'error');
    drupal_goto('dashboard/forms');
  }

  // Store the record for use in validate/submit.
  $form_state['dynamic_form_record'] = $record;

  // Wrapper for dashboard styling.
  $form['#prefix'] = '<div class="dfb-form-wrapper">';
  $form['#suffix'] = '</div>';

  // -- Basic Info section --
  $form['basic_info'] = array(
    '#type' => 'fieldset',
    '#title' => t('Basic Information'),
    '#attributes' => array('class' => array('dfb-form-section')),
  );

  $form['basic_info']['title'] = array(
    '#type' => 'textfield',
    '#title' => t('Form Title'),
    '#required' => TRUE,
    '#maxlength' => 255,
    '#default_value' => $record->title,
    '#description' => t('A clear, descriptive name for your form.'),
  );

  $form['basic_info']['description'] = array(
    '#type' => 'textarea',
    '#title' => t('Description'),
    '#default_value' => $record->description,
    '#rows' => 3,
    '#description' => t('Optional. Shown to respondents before they begin.'),
  );

  // -- Settings section --
  $form['settings'] = array(
    '#type' => 'fieldset',
    '#title' => t('Settings'),
    '#attributes' => array('class' => array('dfb-form-section')),
  );

  $form['settings']['status'] = array(
    '#type' => 'select',
    '#title' => t('Status'),
    '#options' => array(
      0 => t('Draft'),
      1 => t('Published'),
      2 => t('Closed'),
    ),
    '#default_value' => $record->status,
    '#description' => t('Draft forms are not visible to respondents.'),
  );

  $form['settings']['visibility'] = array(
    '#type' => 'select',
    '#title' => t('Visibility'),
    '#options' => array(
      'p' => t('Public'),
      'r' => t('Restricted'),
      'm' => t('Members Only'),
    ),
    '#default_value' => $record->visibility,
    '#description' => t('Control who can access and fill out this form.'),
  );

  $form['settings']['question_per_page'] = array(
    '#type' => 'textfield',
    '#title' => t('Questions Per Page'),
    '#default_value' => $record->question_per_page,
    '#size' => 5,
    '#description' => t('Number of questions shown per page when filling out the form.'),
  );

  // -- Schedule section --
  $form['schedule'] = array(
    '#type' => 'fieldset',
    '#title' => t('Schedule'),
    '#attributes' => array('class' => array('dfb-form-section')),
    '#description' => t('Optionally set when the form opens and closes for responses.'),
  );

  $form['schedule']['opens_at'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Opens At'),
    '#required'      => FALSE,
    '#default_value' => !empty($record->opens_at) ? format_date($record->opens_at, 'custom', 'Y-m-d\TH:i') : '',
    '#attributes'    => array('class' => array('dfb-datetime-input')),
    '#description'   => t('Leave blank to open immediately.'),
  );

  $form['schedule']['closes_at'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Closes At'),
    '#required'      => FALSE,
    '#default_value' => !empty($record->closes_at) ? format_date($record->closes_at, 'custom', 'Y-m-d\TH:i') : '',
    '#attributes'    => array('class' => array('dfb-datetime-input')),
    '#description'   => t('Leave blank for no expiry.'),
  );

  // -- Members section --
  // Load existing members to pre-populate on first render.
  $existing_members      = _dynamic_form_load_existing_members($form_id);
  $existing_members_json = json_encode($existing_members);

  $form['members'] = array(
    '#type'        => 'fieldset',
    '#title'       => t('Form Members'),
    '#attributes'  => array('class' => array('dfb-form-section')),
    '#description' => t('Add editors and form applicants. They will be notified by email when added.'),
  );

  $form['members']['members_search'] = array(
    '#type'       => 'textfield',
    '#title'      => t('Search Users'),
    '#prefix'     => '<div class="dfb-member-picker-wrap">',
    '#suffix'     => '<div id="dfb-members-results" class="dfb-members-results" style="display:none;"></div></div>',
    '#attributes' => array(
      'id'           => 'dfb-members-search',
      'placeholder'  => t('Type a name or email to search…'),
      'autocomplete' => 'off',
    ),
    '#description' => t('Search by name or email. Click a result to add the user.'),
  );

  $form['members']['members_selected'] = array(
    '#type'          => 'hidden',
    '#default_value' => $existing_members_json,
    '#attributes'    => array('id' => 'dfb-members-selected'),
  );

  $form['members']['members_chips_wrap'] = array(
    '#markup' => '<div id="dfb-members-chips" class="dfb-members-chips"></div>',
  );

  // -- Actions --
  $form['actions'] = array(
    '#type' => 'container',
    '#attributes' => array('class' => array('dfb-form-actions')),
  );

  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save Changes'),
    '#attributes' => array('class' => array('dfb-btn-submit')),
  );

  $form['actions']['cancel'] = array(
    '#markup' => l(t('Cancel'), 'dashboard/forms', array(
      'attributes' => array('class' => array('dfb-btn-cancel')),
    )),
  );

  return $form;
}


/**
 * Validate: Edit form.
 */
function dynamic_form_edit_form_validate($form, &$form_state)
{
  $title = trim($form_state['values']['title']);
  $record = $form_state['dynamic_form_record'];

  if (drupal_strlen($title) < 3) {
    form_set_error('title', t('Title must be at least 3 characters long.'));
  }

  // Check slug uniqueness if title changed.
  if ($title !== $record->title) {
    $slug = _dynamic_form_generate_slug($title);
    $exists = db_select('dynamic_form_forms', 'f')
      ->fields('f', array('id'))
      ->condition('slug', $slug)
      ->condition('id', $record->id, '<>')
      ->condition('deleted_at', NULL, 'IS NULL')
      ->execute()
      ->fetchField();

    if ($exists) {
      form_set_error('title', t('A form with a similar title already exists.'));
    }
  }
}

/**
 * Submit: Edit form.
 */
function dynamic_form_edit_form_submit($form, &$form_state)
{
  global $user;
  module_load_include('inc', 'dynamic_form', 'includes/dynamic_form.members');

  $values = $form_state['values'];
  $record = $form_state['dynamic_form_record'];
  $now = REQUEST_TIME;

  $fields = array(
    'title' => trim($values['title']),
    'description' => trim($values['description']),
    'status' => (int) $values['status'],
    'question_per_page' => (int) $values['question_per_page'],
    'visibility' => $values['visibility'],
    'closes_at' => _dynamic_form_parse_date($values['closes_at']),
    'opens_at' => _dynamic_form_parse_date($values['opens_at']),
    'updated_at' => $now,
  );

  // Update slug if title changed.
  if (trim($values['title']) !== $record->title) {
    $fields['slug'] = _dynamic_form_generate_slug(trim($values['title']));
  }

  // Set published_at and published_by when publishing.
  if ((int) $values['status'] === 1 && (int) $record->status !== 1) {
    $fields['published_at'] = $now;
    $fields['published_by'] = $user->uid;
  }

  db_update('dynamic_form_forms')
    ->fields($fields)
    ->condition('id', $record->id)
    ->execute();

  _dynamic_form_audit_log('form', $record->id, 'updated', (array) $record, $fields);

  // Sync members and queue emails for newly added ones.
  $members_json = isset($values['members_selected']) ? $values['members_selected'] : '[]';
  $newly_added  = _dynamic_form_save_members($record->id, $members_json, FALSE);
  _dynamic_form_queue_member_notifications($record->id, trim($values['title']), $newly_added);

  drupal_set_message(t('Form "@title" has been updated.', array('@title' => $values['title'])));
  $form_state['redirect'] = 'dashboard/forms';
}

/* =====================================================================
   FORM CRUD — DELETE (SOFT)
   ===================================================================== */

/**
 * Form builder: Confirm soft deletion of a form.
 */
function dynamic_form_delete_confirm($form, &$form_state, $form_id)
{
  $record = db_select('dynamic_form_forms', 'f')
    ->fields('f', array('id', 'title'))
    ->condition('id', $form_id)
    ->condition('deleted_at', NULL, 'IS NULL')
    ->execute()
    ->fetchObject();

  if (!$record) {
    drupal_set_message(t('Form not found.'), 'error');
    drupal_goto('form/list');
  }

  $form_state['dynamic_form_record'] = $record;

  return confirm_form(
    $form,
    t('Are you sure you want to delete the form "@title"?', array('@title' => $record->title)),
    'form/list',
    t('This action can be undone by an administrator.'),
    t('Delete'),
    t('Cancel')
  );
}

/**
 * Submit: Soft delete form.
 */
function dynamic_form_delete_confirm_submit($form, &$form_state)
{
  global $user;
  $record = $form_state['dynamic_form_record'];
  $now = REQUEST_TIME;

  db_update('dynamic_form_forms')
    ->fields(array(
      'deleted_at' => $now,
      'deleted_by' => $user->uid,
      'updated_at' => $now,
    ))
    ->condition('id', $record->id)
    ->execute();

  _dynamic_form_audit_log('form', $record->id, 'deleted', (array) $record, NULL);

  drupal_set_message(t('Form "@title" has been deleted.', array('@title' => $record->title)));
  $form_state['redirect'] = 'dashboard/forms';
}

/* =====================================================================
   HELPERS
   ===================================================================== */

/**
 * Parses a date string (YYYY-MM-DD HH:MM) into a Unix timestamp.
 *
 * @param string $date_string
 *   A date string in 'Y-m-d H:i' format, or empty.
 *
 * @return int|null
 *   A Unix timestamp, or NULL if the input is empty or invalid.
 */
 function _dynamic_form_parse_date($date_string)
{
  $date_string = trim($date_string);
  if (empty($date_string)) {
    return NULL;
  }

  $timestamp = strtotime($date_string);
  return ($timestamp !== FALSE) ? $timestamp : NULL;
}
