<?php

/**
 * @file
 * Builder-side logic: form alter for translation mode, submit handler,
 * and AJAX callback for the modified question edit modal.
 */

/* =====================================================================
   FORM ALTER — rewires the question edit modal in translation mode
   ===================================================================== */

/**
 * Modifies dynamic_form_builder_question_edit_form for translation mode.
 *
 * - Pre-fills label / description with the existing German translation.
 * - Shows the original English value as a hint below each field.
 * - Hides structural fields (type, options, scale, etc.) — they are not
 *   translatable and must not change in translation mode.
 * - Swaps the submit handler and AJAX callback to write to the translation
 *   table instead of the base questions table.
 */
function _dfbi18n_alter_question_edit_form(&$form, &$form_state, $lang) {
  if (empty($form_state['dfb_eq_question'])) {
    return;
  }

  $q         = $form_state['dfb_eq_question'];
  $q_id      = (int) $form_state['dfb_eq_question_id'];
  $trans     = _dfbi18n_load_question_translation($q_id, $lang);
  $lang_name = _dfbi18n_lang_name($lang);

  // ── Modal header: change title to "Translate Question (German)" ────
  $form['#prefix'] = preg_replace(
    '/<h2>(' . preg_quote(t('Edit Question'), '/') . ')<\/h2>/',
    '<h2>' . t('Translate Question') . ' <span class="dfbi18n-modal-lang-badge">' . check_plain($lang_name) . '</span></h2>',
    $form['#prefix']
  );

  // ── Translation mode banner ─────────────────────────────────────────
  $form['scrollable']['i18n_banner'] = array(
    '#markup' => '<div class="dfbi18n-modal-banner">'
      . '<span class="dfbi18n-modal-banner-icon">&#127760;</span> '
      . t('You are editing the <strong>@lang</strong> translation. The original English content is shown below each field for reference.', array('@lang' => $lang_name))
      . '</div>',
    '#weight' => -50,
  );

  // ── Label — pre-fill with German; show English as description hint ──
  $de_label = ($trans && $trans->label !== '') ? $trans->label : '';
  $form['scrollable']['label']['#default_value'] = $de_label;
  $form['scrollable']['label']['#attributes']['placeholder'] = t('@lang label…', array('@lang' => $lang_name));
  $form['scrollable']['label']['#description'] =
    '<span class="dfbi18n-original-hint"><strong>' . t('English:') . '</strong> ' . check_plain($q->label) . '</span>';

  // ── Description — pre-fill with German; show English hint if present
  $de_desc = ($trans && $trans->description !== '') ? $trans->description : '';
  $form['scrollable']['description']['#default_value'] = $de_desc;
  $form['scrollable']['description']['#attributes']['placeholder'] = t('@lang help text…', array('@lang' => $lang_name));
  if (!empty($q->description)) {
    $form['scrollable']['description']['#description'] =
      '<span class="dfbi18n-original-hint"><strong>' . t('English:') . '</strong> ' . check_plain($q->description) . '</span>';
  }

  // ── Hidden lang field — survives AJAX form submissions via POST ─────
  $form['i18n_lang'] = array(
    '#type'  => 'hidden',
    '#value' => $lang,
  );

  // ── Tell Drupal where to find the new submit/callback during rebuilds
  $form_state['build_info']['files'][] = array(
    'type'   => 'inc',
    'module' => 'dynamic_form_i18n',
    'name'   => 'includes/dynamic_form_i18n.builder',
  );

  // ── Swap submit handler and AJAX callback ───────────────────────────
  $form['actions']['save']['#submit']            = array('dynamic_form_i18n_question_translate_submit');
  $form['actions']['save']['#ajax']['callback']  = '_dfbi18n_question_translate_ajax_callback';
  $form['actions']['save']['#value']             = t('Save @lang Translation', array('@lang' => $lang_name));

  // ── Hide every structural field — not translatable ──────────────────
  $structural = array(
    'type_section', 'sep', 'i18n_banner',  // banner added above; hide sep only
  );
  // Hide the type picker section and its visual separator.
  $form['type_section']['#access'] = FALSE;
  $form['sep']['#access']          = FALSE;

  // Hide toggles and all type-specific sub-sections.
  foreach (array(
    'is_required', 'is_readonly',
    'width_picker', 'width',
    'select_settings', 'options_wrapper',
    'scale_settings', 'file_settings',
    'validations_wrapper',
    'text_editor_info', 'tags_info',
  ) as $key) {
    if (isset($form['scrollable'][$key])) {
      $form['scrollable'][$key]['#access'] = FALSE;
    }
  }
}

/* =====================================================================
   SUBMIT — writes to translation table instead of base record
   ===================================================================== */

/**
 * Saves label and description to the question translation table.
 */
function dynamic_form_i18n_question_translate_submit($form, &$form_state) {
  global $user;

  $now         = REQUEST_TIME;
  $values      = $form_state['values'];
  $question_id = (int) $form_state['dfb_eq_question_id'];
  $lang        = isset($values['i18n_lang']) ? $values['i18n_lang'] : _dfbi18n_get_language();

  if (!$lang) {
    return;
  }

  $label       = trim($values['label']);
  $description = trim(isset($values['description']) ? $values['description'] : '');

  $existing_id = db_select('dynamic_form_question_translations', 't')
    ->fields('t', array('id'))
    ->condition('question_id', $question_id)
    ->condition('language', $lang)
    ->execute()
    ->fetchField();

  if ($existing_id) {
    db_update('dynamic_form_question_translations')
      ->fields(array(
        'label'       => $label,
        'description' => $description,
        'updated_at'  => $now,
      ))
      ->condition('id', $existing_id)
      ->execute();
  }
  else {
    db_insert('dynamic_form_question_translations')->fields(array(
      'question_id' => $question_id,
      'language'    => $lang,
      'label'       => $label,
      'description' => $description,
      'placeholder' => '',
      'created_by'  => (int) $user->uid,
      'created_at'  => $now,
      'updated_at'  => $now,
    ))->execute();
  }

  // Stash results for the AJAX callback.
  $form_state['dfb_eq_saved_question'] = db_select('dynamic_form_questions', 'q')
    ->fields('q')
    ->condition('id', $question_id)
    ->execute()
    ->fetchObject();

  $form_state['dfbi18n_saved_label'] = $label;
  $form_state['dfbi18n_lang']        = $lang;
  $form_state['rebuild']             = TRUE;
}

/* =====================================================================
   AJAX CALLBACK — updates the question card after translation save
   ===================================================================== */

/**
 * After a successful translation save:
 *   1. Updates the card's displayed label in the builder DOM.
 *   2. Sends a custom dfbi18nCardUpdate command so JS can flip the
 *      translation-status badge (⚠ Needs translation → ✓ DE).
 *   3. Closes the modal and resets the form wrapper.
 */
function _dfbi18n_question_translate_ajax_callback($form, &$form_state) {
  $commands = array();

  if (form_get_errors()) {
    $commands[] = ajax_command_replace('#dfb-edit-question-form-wrapper', drupal_render($form));
    return array('#type' => 'ajax', '#commands' => $commands);
  }

  $q        = $form_state['dfb_eq_saved_question'];
  $de_label = $form_state['dfbi18n_saved_label'];
  $lang     = $form_state['dfbi18n_lang'];

  // Update the card's label text — show German label (fall back to English).
  $display_label = ($de_label !== '') ? $de_label : $q->label;
  $req_star      = $q->is_required ? ' <span class="dfb-req">*</span>' : '';
  $commands[] = ajax_command_html(
    '.dfb-question-card[data-question-id="' . (int) $q->id . '"] .dfb-question-label',
    check_plain($display_label) . $req_star
  );

  // Custom command: tells JS to update the translation-status badge.
  $commands[] = array(
    'command'    => 'dfbi18nCardUpdate',
    'questionId' => (int) $q->id,
    'translated' => ($de_label !== ''),
    'lang'       => strtoupper($lang),
  );

  $commands[] = ajax_command_invoke('#dfb-edit-question-modal', 'fadeOut', array('fast'));
  $commands[] = ajax_command_replace('#dfb-edit-question-form-wrapper', drupal_render($form));
  $commands[] = ajax_command_dfb_toast('success', t('@lang translation saved.', array('@lang' => _dfbi18n_lang_name($lang))));

  return array('#type' => 'ajax', '#commands' => $commands);
}

/* =====================================================================
   INTERNAL HELPERS
   ===================================================================== */

/**
 * Returns the human-readable name for a language code.
 */
function _dfbi18n_lang_name($lang) {
  $supported = unserialize(DFBI18N_SUPPORTED_LANGS);
  return isset($supported[$lang]) ? $supported[$lang] : strtoupper($lang);
}
