<?php

/**
 * @file
 * Question CRUD — edit and delete using the same UI as the builder modal.
 */

/* =====================================================================
   EDIT — FORM BUILDER
   ===================================================================== */

function dynamic_form_edit_question_form($form, &$form_state, $question_id) {
  global $user;

  // Tell Drupal where to find this function and the shared AJAX handlers
  // during AJAX rebuilds (options/validations add-more).
  $form_state['build_info']['files'][] = array(
    'type'   => 'inc',
    'module' => 'dynamic_form',
    'name'   => 'includes/dynamic_form.questions',
  );
  $form_state['build_info']['files'][] = array(
    'type'   => 'inc',
    'module' => 'dynamic_form',
    'name'   => 'includes/dynamic_form.builder',
  );

  // --- Load record on first build only ---
  if (!isset($form_state['dfb_edit_question'])) {
    $record = db_select('dynamic_form_questions', 'q')
      ->fields('q')
      ->condition('id', $question_id)
      ->condition('deleted_at', NULL, 'IS NULL')
      ->execute()->fetchObject();

    if (!$record) {
      drupal_set_message(t('Question not found.'), 'error');
      drupal_goto('dashboard/questions');
    }
    if (!user_access('edit any form') && $record->created_by != $user->uid) {
      drupal_access_denied(); drupal_exit();
    }

    // Load existing options.
    $db_options = db_select('dynamic_form_question_options', 'o')
      ->fields('o', array('option_label'))
      ->condition('question_id', $question_id)
      ->condition('deleted_at', NULL, 'IS NULL')
      ->orderBy('position', 'ASC')
      ->execute()->fetchCol();
    $options = array();
    foreach ($db_options as $lbl) {
      $options[] = array('label' => $lbl);
    }
    if (empty($options)) {
      $options = array(array('label' => ''), array('label' => ''));
    }

    // Separate scale meta-rules from user validations.
    $scale_keys  = array('scale_min', 'scale_max', 'scale_min_label', 'scale_max_label');
    $scale_vals  = array('scale_min' => 1, 'scale_max' => 5, 'scale_min_label' => '', 'scale_max_label' => '');
    $db_vals     = array();
    $db_val_rows = db_select('dynamic_form_question_validations', 'v')
      ->fields('v', array('rule_type', 'rule_value', 'error_message'))
      ->condition('question_id', $question_id)
      ->execute()->fetchAll();
    $validations = array();
    foreach ($db_val_rows as $row) {
      if (in_array($row->rule_type, $scale_keys)) {
        $scale_vals[$row->rule_type] = $row->rule_value;
      }
      else {
        $validations[] = array(
          'rule_type'     => $row->rule_type,
          'rule_value'    => $row->rule_value,
          'error_message' => $row->error_message,
        );
      }
    }

    $form_state['dfb_edit_question'] = $record;
    $form_state['options']           = $options;
    $form_state['validations']       = $validations;
    $form_state['scale_vals']        = $scale_vals;
    // File settings from record.
    $form_state['file_settings'] = array(
      'allow_file_types'  => $record->allow_file_types,
      'max_file_size_mb'  => $record->max_file_size ? round($record->max_file_size / 1048576) : '',
      'max_files'         => $record->max_files ?: 1,
    );
  }

  $record     = $form_state['dfb_edit_question'];
  $scale_vals = $form_state['scale_vals'];
  $fs         = $form_state['file_settings'];

  $form['#prefix'] = '<div id="dfb-question-form-wrapper" class="dfb-edit-question-page">';
  $form['#suffix'] = '</div>';

  // ── Zone 1: Type picker ──────────────────────────────────────────
  $form['type_section'] = array(
    '#type'       => 'container',
    '#attributes' => array('class' => array('dfb-type-section')),
  );
  $form['type_section']['type'] = array(
    '#type'          => 'radios',
    '#title'         => t('Question Type'),
    '#title_display' => 'invisible',
    '#required'      => TRUE,
    '#default_value' => $record->type,
    '#options'       => array(
      'text'         => t('Short Text'),
      'textarea'     => t('Long Text'),
      'text_editor'  => t('Text Editor'),
      'radio'        => t('Single Choice'),
      'checkbox'     => t('Multi Choice'),
      'select'       => t('Dropdown'),
      'file'         => t('File Upload'),
      'date'         => t('Date'),
      'time'         => t('Time'),
      'rating'       => t('Rating'),
      'linear_scale' => t('Scale'),
    ),
    '#attributes' => array('class' => array('dfb-type-picker')),
  );

  $form['sep'] = array('#markup' => '<hr class="dfb-modal-separator">');

  // ── Zone 2: Scrollable body ──────────────────────────────────────
  $form['scrollable'] = array(
    '#type'       => 'container',
    '#attributes' => array('class' => array('dfb-form-scrollable', 'dfb-edit-scrollable')),
  );

  // Label row.
  $form['scrollable']['label'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Question Label'),
    '#required'      => TRUE,
    '#maxlength'     => 512,
    '#default_value' => $record->label,
    '#attributes'    => array('placeholder' => t('e.g. What is your preferred department?')),
  );

  // Required + Read-only.
  $form['scrollable']['is_required'] = array(
    '#type'          => 'checkbox',
    '#title'         => t('Required'),
    '#default_value' => $record->is_required,
    '#prefix'        => '<div class="dfb-toggle-row">',
  );
  $form['scrollable']['is_readonly'] = array(
    '#type'          => 'checkbox',
    '#title'         => t('Read-only'),
    '#default_value' => $record->is_readonly,
    '#suffix'        => '</div>',
  );

  // Help text.
  $form['scrollable']['description'] = array(
    '#type'          => 'textarea',
    '#title'         => t('Help Text'),
    '#rows'          => 2,
    '#default_value' => $record->description,
    '#attributes'    => array('placeholder' => t('Optional — shown to respondents.')),
  );

  // Text editor info — shown only when text_editor type is selected.
  $form['scrollable']['text_editor_info'] = array(
    '#markup' =>
      '<div class="dfb-q-subsection dfb-text-editor-info" data-dfb-show-for="text_editor">' .
        '<p class="dfb-subsection-label">' . t('Rich Text Editor') . '</p>' .
        '<p class="dfb-text-editor-hint">' .
          t('Respondents will see a CKEditor rich text editor for their answer. No additional configuration needed.') .
        '</p>' .
      '</div>',
  );

  // Placeholder.
  $form['scrollable']['placeholder'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Placeholder'),
    '#maxlength'     => 255,
    '#default_value' => $record->placeholder,
    '#attributes'    => array('placeholder' => t('Input placeholder text')),
  );

  // ── Options sub-section (radio / checkbox / select) ──────────────
  $form['scrollable']['options_wrapper'] = array(
    '#type'       => 'container',
    '#attributes' => array('id' => 'dfb-q-options-wrapper', 'class' => array('dfb-q-subsection')),
    '#states'     => array(
      'visible' => array(
        array(':input[name="type"]' => array('value' => 'radio')),
        array(':input[name="type"]' => array('value' => 'checkbox')),
        array(':input[name="type"]' => array('value' => 'select')),
      ),
    ),
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
        'wrapper'  => 'dfb-q-options-wrapper',
        'effect'   => 'none',
      ),
      '#attributes' => array('class' => array('dfb-btn-remove'), 'title' => t('Remove')),
      '#suffix'     => '</div>',
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
      'wrapper'  => 'dfb-q-options-wrapper',
      'effect'   => 'none',
    ),
    '#attributes' => array('class' => array('dfb-btn-add-more')),
  );

  // ── Scale settings (rating / linear_scale) ───────────────────────
  $form['scrollable']['scale_settings'] = array(
    '#type'       => 'container',
    '#attributes' => array('class' => array('dfb-q-subsection')),
    '#states'     => array(
      'visible' => array(
        array(':input[name="type"]' => array('value' => 'rating')),
        array(':input[name="type"]' => array('value' => 'linear_scale')),
      ),
    ),
  );
  $form['scrollable']['scale_settings']['_title'] = array(
    '#markup' => '<p class="dfb-subsection-label">' . t('Scale Range') . '</p>',
  );
  $form['scrollable']['scale_settings']['scale_min'] = array(
    '#type'          => 'select',
    '#title'         => t('Min'),
    '#options'       => drupal_map_assoc(range(0, 5)),
    '#default_value' => $scale_vals['scale_min'],
    '#prefix'        => '<div class="dfb-form-row dfb-scale-row">',
  );
  $form['scrollable']['scale_settings']['scale_max'] = array(
    '#type'          => 'select',
    '#title'         => t('Max'),
    '#options'       => drupal_map_assoc(range(2, 10)),
    '#default_value' => $scale_vals['scale_max'],
    '#suffix'        => '</div>',
  );
  $form['scrollable']['scale_settings']['scale_min_label'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Min Label'),
    '#default_value' => $scale_vals['scale_min_label'],
    '#attributes'    => array('placeholder' => t('e.g. Not at all')),
    '#prefix'        => '<div class="dfb-form-row dfb-scale-row">',
  );
  $form['scrollable']['scale_settings']['scale_max_label'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Max Label'),
    '#default_value' => $scale_vals['scale_max_label'],
    '#attributes'    => array('placeholder' => t('e.g. Extremely likely')),
    '#suffix'        => '</div>',
  );

  // ── File settings ────────────────────────────────────────────────
  $form['scrollable']['file_settings'] = array(
    '#type'       => 'container',
    '#attributes' => array('class' => array('dfb-q-subsection')),
    '#states'     => array(
      'visible' => array(':input[name="type"]' => array('value' => 'file')),
    ),
  );
  $form['scrollable']['file_settings']['_title'] = array(
    '#markup' => '<p class="dfb-subsection-label">' . t('File Settings') . '</p>',
  );
  $form['scrollable']['file_settings']['allow_file_types'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Allowed Extensions'),
    '#description'   => t('Comma-separated, e.g. pdf,docx,jpg'),
    '#default_value' => $fs['allow_file_types'],
    '#attributes'    => array('placeholder' => 'pdf,docx,jpg'),
  );
  $form['scrollable']['file_settings']['max_file_size_mb'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Max Size (MB)'),
    '#size'          => 6,
    '#default_value' => $fs['max_file_size_mb'],
    '#attributes'    => array('placeholder' => '10'),
    '#prefix'        => '<div class="dfb-form-row dfb-scale-row">',
  );
  $form['scrollable']['file_settings']['max_files'] = array(
    '#type'          => 'select',
    '#title'         => t('Max Files'),
    '#options'       => drupal_map_assoc(range(1, 10)),
    '#default_value' => $fs['max_files'],
    '#suffix'        => '</div>',
  );

  // ── Validation rules ─────────────────────────────────────────────
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
  $form['scrollable']['validations_wrapper'] = array(
    '#type'       => 'container',
    '#attributes' => array('id' => 'dfb-q-validations-wrapper', 'class' => array('dfb-q-subsection')),
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
      '#submit'                  => array('_dfb_question_remove_validation_submit'),
      '#limit_validation_errors' => array(),
      '#ajax'                    => array(
        'callback' => '_dfb_question_validations_ajax_callback',
        'wrapper'  => 'dfb-q-validations-wrapper',
        'effect'   => 'none',
      ),
      '#attributes' => array('class' => array('dfb-btn-remove'), 'title' => t('Remove')),
      '#suffix'     => '</div>',
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
    '#attributes' => array('class' => array('dfb-btn-add-more')),
  );

  // ── Zone 3: Action footer ────────────────────────────────────────
  $form['actions'] = array(
    '#type'       => 'container',
    '#attributes' => array('class' => array('dfb-modal-actions')),
  );
  $form['actions']['cancel'] = array(
    '#markup' => l(t('Cancel'), 'dashboard/questions', array(
      'attributes' => array('class' => array('dfb-btn-cancel')),
    )),
  );
  $form['actions']['submit'] = array(
    '#type'       => 'submit',
    '#value'      => t('Save Changes →'),
    '#attributes' => array('class' => array('dfb-btn-save-question')),
  );

  return $form;
}

/* =====================================================================
   EDIT — VALIDATE
   ===================================================================== */

function dynamic_form_edit_question_form_validate($form, &$form_state) {
  if (drupal_strlen(trim($form_state['values']['label'])) < 2) {
    form_set_error('label', t('Question label must be at least 2 characters.'));
  }
}

/* =====================================================================
   EDIT — SUBMIT
   ===================================================================== */

function dynamic_form_edit_question_form_submit($form, &$form_state) {
  global $user;
  $now     = REQUEST_TIME;
  $values  = $form_state['values'];
  $record  = $form_state['dfb_edit_question'];
  $type    = $values['type'];
  $qid     = (int) $record->id;

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

  // Update the question record.
  db_update('dynamic_form_questions')
    ->fields(array(
      'label'            => trim($values['label']),
      'description'      => isset($values['description'])  ? trim($values['description'])  : '',
      'placeholder'      => isset($values['placeholder'])  ? trim($values['placeholder'])  : '',
      'type'             => $type,
      'is_required'      => (int) $values['is_required'],
      'is_readonly'      => (int) $values['is_readonly'],
      'allow_file_types' => $allow_file_types,
      'max_file_size'    => $max_file_size,
      'max_files'        => $max_files,
      'updated_at'       => $now,
    ))
    ->condition('id', $qid)
    ->execute();

  // --- Options: soft-delete old, re-insert ---
  db_update('dynamic_form_question_options')
    ->fields(array('deleted_at' => $now, 'deleted_by' => $user->uid, 'updated_at' => $now))
    ->condition('question_id', $qid)
    ->condition('deleted_at', NULL, 'IS NULL')
    ->execute();

  $choice_types = array('radio', 'checkbox', 'select', 'rating', 'linear_scale');
  if (in_array($type, $choice_types)) {
    foreach ($form_state['options'] as $i => $opt) {
      $label = isset($values['option_' . $i]) ? trim($values['option_' . $i]) : '';
      if ($label !== '') {
        db_insert('dynamic_form_question_options')->fields(array(
          'question_id'  => $qid,
          'option_label' => $label,
          'position'     => $i,
          'created_by'   => $user->uid,
          'created_at'   => $now,
          'updated_at'   => $now,
        ))->execute();
      }
    }
  }

  // --- Validations: delete old user rules, re-insert ---
  $scale_keys = array('scale_min', 'scale_max', 'scale_min_label', 'scale_max_label');
  db_delete('dynamic_form_question_validations')
    ->condition('question_id', $qid)
    ->condition('rule_type', $scale_keys, 'NOT IN')
    ->execute();

  // Re-insert user validation rules.
  foreach ($form_state['validations'] as $i => $v) {
    $rule_type = isset($values['val_type_' . $i]) ? $values['val_type_' . $i] : '';
    if ($rule_type === '') { continue; }
    $rule_value    = isset($values['val_value_' . $i]) ? trim($values['val_value_' . $i]) : NULL;
    $error_message = isset($values['val_msg_'   . $i]) ? trim($values['val_msg_'   . $i]) : NULL;
    db_insert('dynamic_form_question_validations')->fields(array(
      'question_id'   => $qid,
      'rule_type'     => $rule_type,
      'rule_value'    => $rule_value    ?: NULL,
      'error_message' => $error_message ?: NULL,
      'created_by'    => $user->uid,
      'created_at'    => $now,
      'updated_at'    => $now,
    ))->execute();
  }

  // --- Scale settings: delete and re-insert ---
  db_delete('dynamic_form_question_validations')
    ->condition('question_id', $qid)
    ->condition('rule_type', $scale_keys, 'IN')
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
          'question_id' => $qid,
          'rule_type'   => $rule_type,
          'rule_value'  => $rule_value,
          'created_by'  => $user->uid,
          'created_at'  => $now,
          'updated_at'  => $now,
        ))->execute();
      }
    }
  }

  drupal_set_message(t('Question "@label" has been updated.', array('@label' => trim($values['label']))));
  $form_state['redirect'] = 'dashboard/questions';
}

/* =====================================================================
   DELETE — CONFIRM + SUBMIT
   ===================================================================== */

function dynamic_form_delete_question_confirm($form, &$form_state, $question_id) {
  global $user;

  $record = db_select('dynamic_form_questions', 'q')
    ->fields('q', array('id', 'label', 'created_by'))
    ->condition('id', $question_id)
    ->condition('deleted_at', NULL, 'IS NULL')
    ->execute()->fetchObject();

  if (!$record) {
    drupal_set_message(t('Question not found.'), 'error');
    drupal_goto('dashboard/questions');
  }
  if (!user_access('delete any form') && $record->created_by != $user->uid) {
    drupal_access_denied(); drupal_exit();
  }

  $form_state['dynamic_form_question'] = $record;

  return confirm_form(
    $form,
    t('Are you sure you want to delete "@label"?', array('@label' => $record->label)),
    'dashboard/questions',
    t('This will soft-delete the question. It can be restored by an administrator.'),
    t('Delete'),
    t('Cancel')
  );
}

function dynamic_form_delete_question_confirm_submit($form, &$form_state) {
  global $user;
  $record = $form_state['dynamic_form_question'];
  $now    = REQUEST_TIME;

  db_update('dynamic_form_questions')
    ->fields(array('deleted_at' => $now, 'deleted_by' => $user->uid, 'updated_at' => $now))
    ->condition('id', $record->id)
    ->execute();

  drupal_set_message(t('Question "@label" has been deleted.', array('@label' => $record->label)));
  $form_state['redirect'] = 'dashboard/questions';
}
