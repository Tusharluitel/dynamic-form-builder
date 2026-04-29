<?php

/**
 * @file
 * Section CRUD page callbacks and forms for the Dynamic Form module.
 *
 * Contains create, edit, and delete (soft) form builders,
 * validators, and submit handlers.
 */

/* =====================================================================
   SECTION CRUD — CREATE
   ===================================================================== */

/**
 * Form builder: Create a new section.
 */
function dynamic_form_create_section_form($form, &$form_state)
{
  global $user;
  
  // Wrapper for dashboard styling.
  $form['#prefix'] = '<div class="dfb-form-wrapper">';
  $form['#suffix'] = '</div>';

  // Get available forms for the user.
  $is_admin = user_access('edit any form');
  $query = db_select('dynamic_form_forms', 'f')
    ->fields('f', array('id', 'title'))
    ->condition('f.deleted_at', NULL, 'IS NULL')
    ->orderBy('f.title', 'ASC');
    
  if (!$is_admin) {
    $query->condition('f.created_by', $user->uid);
  }
  
  $form_options = $query->execute()->fetchAllKeyed();
  
  if (empty($form_options)) {
    drupal_set_message(t('You must create a form before you can create a section.'), 'warning');
    drupal_goto('dashboard/forms/create');
  }

  // -- Basic Info section --
  $form['basic_info'] = array(
    '#type' => 'fieldset',
    '#title' => t('Basic Information'),
    '#attributes' => array('class' => array('dfb-form-section')),
  );

  $form['basic_info']['name'] = array(
    '#type' => 'textfield',
    '#title' => t('Section Name'),
    '#required' => TRUE,
    '#maxlength' => 255,
    '#attributes' => array('placeholder' => t('e.g. Personal Details')),
    '#description' => t('A clear, descriptive name for your section.'),
  );

  $form['basic_info']['description'] = array(
    '#type' => 'textarea',
    '#title' => t('Description'),
    '#required' => FALSE,
    '#rows' => 3,
    '#attributes' => array('placeholder' => t('Briefly describe the purpose of this section...')),
    '#description' => t('Optional. Shown to respondents.'),
  );

  // -- Configuration section --
  $form['config'] = array(
    '#type' => 'fieldset',
    '#title' => t('Configuration'),
    '#attributes' => array('class' => array('dfb-form-section')),
  );

  $form['config']['form_id'] = array(
    '#type' => 'select',
    '#title' => t('Form'),
    '#options' => $form_options,
    '#required' => TRUE,
    '#description' => t('Select the form this section belongs to.'),
  );

  $form['config']['position'] = array(
    '#type' => 'textfield',
    '#title' => t('Position'),
    '#default_value' => 0,
    '#size' => 5,
    '#description' => t('Order of the section within the form (lower numbers appear first).'),
  );

  // -- Actions --
  $form['actions'] = array(
    '#type' => 'container',
    '#attributes' => array('class' => array('dfb-form-actions')),
  );

  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Create Section'),
    '#attributes' => array('class' => array('dfb-btn-submit')),
  );

  $form['actions']['cancel'] = array(
    '#markup' => l(t('Cancel'), 'dashboard/sections', array(
      'attributes' => array('class' => array('dfb-btn-cancel')),
    )),
  );

  return $form;
}

/**
 * Validate: Create section.
 */
function dynamic_form_create_section_form_validate($form, &$form_state)
{
  $name = trim($form_state['values']['name']);

  if (drupal_strlen($name) < 2) {
    form_set_error('name', t('Name must be at least 2 characters long.'));
  }
}

/**
 * Submit: Create section.
 */
function dynamic_form_create_section_form_submit($form, &$form_state)
{
  global $user;
  $values = $form_state['values'];
  $now = REQUEST_TIME;

  $fields = array(
    'form_id' => (int) $values['form_id'],
    'name' => trim($values['name']),
    'description' => trim($values['description']),
    'position' => (int) $values['position'],
    'created_by' => $user->uid,
    'created_at' => $now,
    'updated_at' => $now,
  );

  $id = db_insert('dynamic_form_sections')
    ->fields($fields)
    ->execute();

  _dynamic_form_audit_log('section', $id, 'created', NULL, $fields);

  drupal_set_message(t('Section "@name" has been created.', array('@name' => $values['name'])));
  $form_state['redirect'] = 'dashboard/sections';
}

/* =====================================================================
   SECTION CRUD — EDIT
   ===================================================================== */

/**
 * Form builder: Edit an existing section.
 */
function dynamic_form_edit_section_form($form, &$form_state, $section_id)
{
  global $user;

  $record = db_select('dynamic_form_sections', 's')
    ->fields('s')
    ->condition('id', $section_id)
    ->condition('deleted_at', NULL, 'IS NULL')
    ->execute()
    ->fetchObject();

  if (!$record) {
    drupal_set_message(t('Section not found.'), 'error');
    drupal_goto('dashboard/sections');
  }

  // Check access (rudimentary check here, since hook_menu allowed 'edit any form')
  if (!user_access('edit any form') && $record->created_by != $user->uid) {
    drupal_access_denied();
    drupal_exit();
  }

  // Store the record for use in validate/submit.
  $form_state['dynamic_form_record'] = $record;

  // Wrapper for dashboard styling.
  $form['#prefix'] = '<div class="dfb-form-wrapper">';
  $form['#suffix'] = '</div>';

  // Get available forms for the user.
  $is_admin = user_access('edit any form');
  $query = db_select('dynamic_form_forms', 'f')
    ->fields('f', array('id', 'title'))
    ->condition('f.deleted_at', NULL, 'IS NULL')
    ->orderBy('f.title', 'ASC');
    
  if (!$is_admin) {
    $query->condition('f.created_by', $user->uid);
  }
  
  $form_options = $query->execute()->fetchAllKeyed();

  // -- Basic Info section --
  $form['basic_info'] = array(
    '#type' => 'fieldset',
    '#title' => t('Basic Information'),
    '#attributes' => array('class' => array('dfb-form-section')),
  );

  $form['basic_info']['name'] = array(
    '#type' => 'textfield',
    '#title' => t('Section Name'),
    '#required' => TRUE,
    '#maxlength' => 255,
    '#default_value' => $record->name,
    '#description' => t('A clear, descriptive name for your section.'),
  );

  $form['basic_info']['description'] = array(
    '#type' => 'textarea',
    '#title' => t('Description'),
    '#default_value' => $record->description,
    '#rows' => 3,
    '#description' => t('Optional. Shown to respondents.'),
  );

  // -- Configuration section --
  $form['config'] = array(
    '#type' => 'fieldset',
    '#title' => t('Configuration'),
    '#attributes' => array('class' => array('dfb-form-section')),
  );

  $form['config']['form_id'] = array(
    '#type' => 'select',
    '#title' => t('Form'),
    '#options' => $form_options,
    '#required' => TRUE,
    '#default_value' => $record->form_id,
    '#description' => t('Select the form this section belongs to.'),
  );

  $form['config']['position'] = array(
    '#type' => 'textfield',
    '#title' => t('Position'),
    '#default_value' => $record->position,
    '#size' => 5,
    '#description' => t('Order of the section within the form (lower numbers appear first).'),
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
    '#markup' => l(t('Cancel'), 'dashboard/sections', array(
      'attributes' => array('class' => array('dfb-btn-cancel')),
    )),
  );

  return $form;
}

/**
 * Validate: Edit section.
 */
function dynamic_form_edit_section_form_validate($form, &$form_state)
{
  $name = trim($form_state['values']['name']);

  if (drupal_strlen($name) < 2) {
    form_set_error('name', t('Name must be at least 2 characters long.'));
  }
}

/**
 * Submit: Edit section.
 */
function dynamic_form_edit_section_form_submit($form, &$form_state)
{
  $values = $form_state['values'];
  $record = $form_state['dynamic_form_record'];
  $now = REQUEST_TIME;

  $fields = array(
    'form_id' => (int) $values['form_id'],
    'name' => trim($values['name']),
    'description' => trim($values['description']),
    'position' => (int) $values['position'],
    'updated_at' => $now,
  );

  db_update('dynamic_form_sections')
    ->fields($fields)
    ->condition('id', $record->id)
    ->execute();

  _dynamic_form_audit_log('section', $record->id, 'updated', (array) $record, $fields);

  drupal_set_message(t('Section "@name" has been updated.', array('@name' => $values['name'])));
  $form_state['redirect'] = 'dashboard/sections';
}

/* =====================================================================
   SECTION CRUD — DELETE (SOFT)
   ===================================================================== */

/**
 * Form builder: Confirm soft deletion of a section.
 */
function dynamic_form_delete_section_confirm($form, &$form_state, $section_id)
{
  global $user;

  $record = db_select('dynamic_form_sections', 's')
    ->fields('s', array('id', 'name', 'created_by'))
    ->condition('id', $section_id)
    ->condition('deleted_at', NULL, 'IS NULL')
    ->execute()
    ->fetchObject();

  if (!$record) {
    drupal_set_message(t('Section not found.'), 'error');
    drupal_goto('dashboard/sections');
  }

  // Check access.
  if (!user_access('delete any form') && $record->created_by != $user->uid) {
    drupal_access_denied();
    drupal_exit();
  }

  $form_state['dynamic_form_record'] = $record;

  return confirm_form(
    $form,
    t('Are you sure you want to delete the section "@name"?', array('@name' => $record->name)),
    'dashboard/sections',
    t('This action can be undone by an administrator. Any questions inside this section will need to be reassigned.'),
    t('Delete'),
    t('Cancel')
  );
}

/**
 * Submit: Soft delete section.
 */
function dynamic_form_delete_section_confirm_submit($form, &$form_state)
{
  global $user;
  $record = $form_state['dynamic_form_record'];
  $now = REQUEST_TIME;

  db_update('dynamic_form_sections')
    ->fields(array(
      'deleted_at' => $now,
      'deleted_by' => $user->uid,
      'updated_at' => $now,
    ))
    ->condition('id', $record->id)
    ->execute();

  _dynamic_form_audit_log('section', $record->id, 'deleted', (array) $record, NULL);

  drupal_set_message(t('Section "@name" has been deleted.', array('@name' => $record->name)));
  $form_state['redirect'] = 'dashboard/sections';
}
