<?php

/**
 * Public-facing form rendering, AJAX save/submit endpoints, and thank-you page.
 */



/**
 * Entry point: returns the rendered fillable form for a given form record.
 *
 * Called by dynamic_form_public_form_view_page() when this module is enabled.
 * Assets are added here; the form itself is built via drupal_get_form() so
 * every question input is a proper FAPI element in a shared $form array.
 */
function dynamic_form_response_render_form($record) {
  global $user;

  $guest_token = $user->uid ? NULL : _dynamic_form_response_get_guest_token();

  // Already-submitted check — skipped when the form allows multiple submissions.
  if (empty($record->allow_multiple_submissions)) {
    $already = $user->uid
      ? DynamicFormResponseEntityController::hasSubmittedByUser($record->id, $user->uid)
      : DynamicFormResponseEntityController::hasSubmittedByGuest($record->id, $guest_token);

    if ($already) {
      return theme('dynamic_form_response_already_submitted', array());
    }
  }

  // Add JS and conditional third-party assets.
  $module_path = drupal_get_path('module', 'dynamic_form_response');
  drupal_add_js($module_path . '/js/response-form.js');
  _dynamic_form_response_maybe_load_select2($record->id);
  _dynamic_form_response_maybe_load_tinymce($record->id);
  drupal_add_css(drupal_get_path('module', 'dynamic_form') . '/css/preview.css');

  return drupal_render(drupal_get_form('dynamic_form_response_form', $record, $guest_token));
}

/**
 * FAPI form builder for the public response form.
 *
 * Builds the complete multi-step response form as a $form array.
 * All question inputs are proper FAPI elements nested under their step,
 * section, and column containers. JS (response-form.js) handles AJAX
 * save/submit by reading CSS classes on the rendered elements.
 *
 * @param array  $form
 * @param array &$form_state
 * @param object $record      dynamic_form_forms DB row.
 * @param string $guest_token Guest cookie token, or NULL for logged-in users.
 */
function dynamic_form_response_form($form, &$form_state, $record, $guest_token = NULL) {
  global $user;

  $form_state['build_info']['files'][] = array(
    'type'   => 'inc',
    'module' => 'dynamic_form_response',
    'name'   => 'includes/dynamic_form_response.submit',
  );

  // Load draft + answers once; cache in form state for any AJAX rebuilds.
  if (!isset($form_state['dfr_loaded'])) {
    $response_id      = 0;
    $existing_answers = array();
    $draft = $user->uid
      ? DynamicFormResponseEntityController::findDraftByUser($record->id, $user->uid)
      : DynamicFormResponseEntityController::findDraftByGuest($record->id, $guest_token);
    if ($draft) {
      $response_id      = (int) $draft->id;
      $existing_answers = DynamicFormResponseEntityController::getAnswersMap($response_id);
    }

    $form_state['dfr_record']           = $record;
    $form_state['dfr_response_id']      = $response_id;
    $form_state['dfr_existing_answers'] = $existing_answers;
    $form_state['dfr_loaded']           = TRUE;
  }

  $record           = $form_state['dfr_record'];
  $response_id      = $form_state['dfr_response_id'];
  $existing_answers = $form_state['dfr_existing_answers'];

  // Load sections ordered by position.
  $sections = db_select('dynamic_form_sections', 's')
    ->fields('s')
    ->condition('form_id', $record->id)
    ->isNull('deleted_at')
    ->orderBy('position', 'ASC')
    ->execute()
    ->fetchAll();

  // Load questions.
  $questions = dynamic_form_get_questions($record->id);
  if (empty($questions)) {
    $form['empty'] = array(
      '#markup' => '<p class="dfr-empty">' . t('This form has no questions yet.') . '</p>',
    );
    return $form;
  }

  $question_ids = array();
  $q_by_section = array();
  foreach ($questions as $q) {
    $question_ids[] = (int) $q->id;
    $sid = $q->section_id ? (int) $q->section_id : 0;
    $q_by_section[$sid][] = $q;
  }

  // Load selectable options, grouped by question.
  $options_by_q = array();
  if (!empty($question_ids)) {
    $opts = db_select('dynamic_form_question_options', 'o')
      ->fields('o')
      ->condition('question_id', $question_ids, 'IN')
      ->isNull('deleted_at')
      ->orderBy('position', 'ASC')
      ->execute()
      ->fetchAll();
    foreach ($opts as $opt) {
      $options_by_q[(int) $opt->question_id][] = $opt;
    }
  }

  // Load scale / allow_multiple meta rules and custom validation rules.
  $scale_by_q = array();
  $rules_by_q = array();
  if (!empty($question_ids)) {
    $metas = db_select('dynamic_form_question_validations', 'v')
      ->fields('v')
      ->condition('question_id', $question_ids, 'IN')
      ->execute()
      ->fetchAll();
    $scale_keys = array('scale_min', 'scale_max', 'scale_min_label', 'scale_max_label', 'allow_multiple');
    foreach ($metas as $m) {
      $qid = (int) $m->question_id;
      if (in_array($m->rule_type, $scale_keys)) {
        $scale_by_q[$qid][$m->rule_type] = $m->rule_value;
      }
      else {
        $rules_by_q[$qid][] = array(
          'rule_type'     => $m->rule_type,
          'rule_value'    => $m->rule_value,
          'error_message' => $m->error_message,
        );
      }
    }
  }

  // Flatten questions in section order, carrying section context.
  $ordered_questions = array();
  foreach ($sections as $sec) {
    $sid = (int) $sec->id;
    $qs  = isset($q_by_section[$sid]) ? $q_by_section[$sid] : array();
    foreach ($qs as $q) {
      $ordered_questions[] = array('q' => $q, 'section' => $sec);
    }
  }
  // Unsectioned questions (section_id IS NULL).
  if (!empty($q_by_section[0])) {
    foreach ($q_by_section[0] as $q) {
      $ordered_questions[] = array('q' => $q, 'section' => NULL);
    }
  }

  if (empty($ordered_questions)) {
    $form['empty'] = array(
      '#markup' => '<p class="dfr-empty">' . t('This form has no questions yet.') . '</p>',
    );
    return $form;
  }

  // Divide into steps. question_per_page = 0 means "show all at once".
  $questions_per_page = (int) $record->question_per_page;
  $questions_per_page = ($questions_per_page > 0) ? $questions_per_page : count($ordered_questions);
  $steps = array_chunk($ordered_questions, $questions_per_page);
  $total = count($steps);

  $token           = drupal_get_token('dfr_submit');
  $save_url        = url('dynamic-form-response/ajax/save');
  $submit_url      = url('dynamic-form-response/ajax/submit');
  $upload_url      = url('dynamic-form-response/ajax/upload');
  $guest_email_url = url('dynamic-form-response/ajax/guest-email');
  $thanks_url      = url('forms/' . check_plain($record->slug) . '/thank-you');

  $col_map = array('full' => 12, 'half' => 6, 'third' => 4, 'quarter' => 3);

  // -----------------------------------------------------------------
  // Outer card wrapper: .dfp-form-card > .dfr-form-container
  // The JS (response-form.js) attaches to .dfr-form-container and reads
  // the data-* attributes to configure itself.
  // -----------------------------------------------------------------
  $form['card'] = array(
    '#type'       => 'container',
    '#attributes' => array('class' => array('dfp-form-card')),
  );
  $form['card']['inner'] = array(
    '#type'       => 'container',
    '#attributes' => array(
      'class'                => array('dfr-form-container'),
      'data-form-id'         => (string) (int) $record->id,
      'data-token'           => check_plain($token),
      'data-save-url'        => check_url($save_url),
      'data-submit-url'      => check_url($submit_url),
      'data-upload-url'      => check_url($upload_url),
      'data-thankyou-url'    => check_url($thanks_url),
      'data-total-steps'     => (string) $total,
      'data-is-anonymous'    => $user->uid ? '0' : '1',
      'data-guest-email-url' => check_url($guest_email_url),
    ),
  );

  // Response ID — JS updates this via $('#dfr-response-id').val(n) as
  // drafts are created; using #markup keeps the id stable across rebuilds.
  $form['card']['inner']['response_id'] = array(
    '#markup' => '<input type="hidden" id="dfr-response-id" value="' . (int) $response_id . '">',
  );

  // Progress bar (only shown when there is more than one step).
  if ($total > 1) {
    $form['card']['inner']['progress'] = array(
      '#markup' => '<div class="dfr-progress-wrap"><div class="dfr-progress-fill"></div></div>'
        . '<div class="dfr-progress-text"></div>',
    );
  }

  // -----------------------------------------------------------------
  // Steps
  // -----------------------------------------------------------------
  foreach ($steps as $step_idx => $step_items) {
    $step_num = $step_idx + 1;
    $is_first = ($step_num === 1);
    $is_last  = ($step_num === $total);
    $step_key = 'step_' . $step_num;

    $step_classes = array('dfr-step');
    if ($is_first) {
      $step_classes[] = 'dfr-step-active';
    }

    $form['card']['inner'][$step_key] = array(
      '#type'       => 'container',
      '#attributes' => array(
        'class'     => $step_classes,
        'data-step' => (string) $step_num,
      ),
    );

    $prev_sid = -1;
    $sec_key  = NULL;
    $row_idx  = 0;
    $row_key  = NULL;
    $row_cols = 0;

    foreach ($step_items as $item) {
      $q   = $item['q'];
      $sec = $item['section'];
      $sid = $sec ? (int) $sec->id : 0;
      $qid = (int) $q->id;

      $width  = (!empty($q->width) && isset($col_map[$q->width])) ? $q->width : 'full';
      $q_cols = $col_map[$width];

      // ---- Section boundary ----------------------------------------
      if ($sid !== $prev_sid) {
        // Unique key: section ID + step number avoids clashes when the
        // same section spans two steps after chunking.
        $sec_key = 'sec_' . ($sid ?: 'none') . '_s' . $step_num;
        $form['card']['inner'][$step_key][$sec_key] = array(
          '#type'       => 'container',
          '#attributes' => array('class' => array('dfp-section')),
        );
        if ($sec) {
          $form['card']['inner'][$step_key][$sec_key]['header'] = array(
            '#markup' => theme('dynamic_form_preview_section_header', array('section' => $sec)),
          );
        }
        $prev_sid = $sid;
        $row_idx  = 0;
        $row_key  = NULL;
        $row_cols = 0;
      }

      // ---- Column row management -----------------------------------
      if ($row_key === NULL) {
        $row_key = 'row_' . $row_idx;
        $form['card']['inner'][$step_key][$sec_key][$row_key] = array(
          '#type'       => 'container',
          '#attributes' => array('class' => array('dfp-question-row')),
        );
        $row_cols = 0;
      }
      elseif ($row_cols + $q_cols > 12) {
        $row_idx++;
        $row_key = 'row_' . $row_idx;
        $form['card']['inner'][$step_key][$sec_key][$row_key] = array(
          '#type'       => 'container',
          '#attributes' => array('class' => array('dfp-question-row')),
        );
        $row_cols = 0;
      }
      $row_cols += $q_cols;

      $opts  = isset($options_by_q[$qid]) ? $options_by_q[$qid] : array();
      $scale = isset($scale_by_q[$qid])   ? $scale_by_q[$qid]   : array();
      $rules = isset($rules_by_q[$qid])   ? $rules_by_q[$qid]   : array();
      $val   = isset($existing_answers[$qid]) ? $existing_answers[$qid] : NULL;

      // ---- Column container ----------------------------------------
      $form['card']['inner'][$step_key][$sec_key][$row_key]['col_' . $qid] = array(
        '#type'       => 'container',
        '#attributes' => array('class' => array('dfp-question-col', 'dfp-col-' . check_plain($width))),
      );

      // ---- Question wrapper (.dfp-question) ------------------------
      // JS reads data-question-id, data-type, data-required, and the
      // optional data-validations JSON to drive client-side behaviour.
      $q_attrs = array(
        'class'            => array('dfp-question'),
        'data-question-id' => (string) $qid,
        'data-type'        => check_plain($q->type),
        'data-required'    => $q->is_required ? '1' : '0',
      );
      if (!empty($rules)) {
        $q_attrs['data-validations'] = check_plain(drupal_json_encode($rules));
      }

      $form['card']['inner'][$step_key][$sec_key][$row_key]['col_' . $qid]['question'] = array(
        '#type'       => 'container',
        '#attributes' => $q_attrs,
      );

      // Question label and optional description.
      $label_html = '<div class="dfp-question-label">'
        . check_plain($q->label)
        . ($q->is_required
            ? ' <span class="dfp-req" title="' . t('Required') . '">*</span>'
            : '')
        . '</div>';
      if (!empty($q->description)) {
        $label_html .= '<p class="dfp-help-text">' . check_plain($q->description) . '</p>';
      }
      $form['card']['inner'][$step_key][$sec_key][$row_key]['col_' . $qid]['question']['label'] = array(
        '#markup' => $label_html,
      );

      // Input wrapper — merge the named sub-elements returned by the
      // builder directly into the container using PHP's + operator so
      // each question type's keys (input, options, widget, …) sit as
      // immediate children without an extra wrapping key.
      $form['card']['inner'][$step_key][$sec_key][$row_key]['col_' . $qid]['question']['input_wrap'] = array(
        '#type'       => 'container',
        '#attributes' => array('class' => array('dfp-question-input')),
      ) + _dynamic_form_response_build_fapi_element($q, $opts, $scale, $val);

      // Error placeholder — JS populates via .text(msg).show().
      $form['card']['inner'][$step_key][$sec_key][$row_key]['col_' . $qid]['question']['error'] = array(
        '#markup' => '<div class="dfr-field-error"></div>',
      );
    }

    // ---- Step navigation -----------------------------------------
    $form['card']['inner'][$step_key]['nav'] = array(
      '#markup' => theme('dynamic_form_response_step_nav', array(
        'is_first'  => $is_first,
        'is_last'   => $is_last,
        'prev_step' => $step_num - 1,
        'next_step' => $step_num + 1,
      )),
    );
  }

  return $form;
}

/* =====================================================================
   QUESTION INPUT BUILDER
   ===================================================================== */

/**
 * Builds a $form sub-array of named FAPI elements for one question input.
 *
 * Returns an associative array of child elements (e.g. 'input', 'options',
 * 'widget') that the caller merges directly into the question's input_wrap
 * container using PHP's + operator. Each question type uses descriptive
 * keys so the purpose of each child is self-evident in the form tree.
 */
function _dynamic_form_response_build_fapi_element($q, array $opts, array $scale, $existing_value = NULL) {
  $form = array();
  $qid  = (int) $q->id;
  $type = $q->type;
  $val  = ($existing_value !== NULL) ? $existing_value : '';

  switch ($type) {

    case 'text':
      $attrs = array('class' => array('dfp-input'));
      if (!empty($q->placeholder)) {
        $attrs['placeholder'] = check_plain($q->placeholder);
      }
      $form['input'] = array(
        '#type'          => 'textfield',
        '#title'         => '',
        '#title_display' => 'invisible',
        '#required'      => FALSE,
        '#value'         => check_plain($val),
        '#attributes'    => $attrs,
      );
      break;

    case 'textarea':
      $attrs = array('class' => array('dfp-textarea'));
      if (!empty($q->placeholder)) {
        $attrs['placeholder'] = check_plain($q->placeholder);
      }
      $form['input'] = array(
        '#type'          => 'textarea',
        '#title'         => '',
        '#title_display' => 'invisible',
        '#required'      => FALSE,
        '#rows'          => 4,
        '#value'         => check_plain($val),
        '#attributes'    => $attrs,
      );
      break;

    case 'text_editor':
      $form['input'] = array(
        '#type'          => 'textarea',
        '#title'         => '',
        '#title_display' => 'invisible',
        '#required'      => FALSE,
        '#rows'          => 6,
        '#id'            => 'dfr-tinymce-' . $qid,
        '#value'         => check_plain($val),
        '#attributes'    => array('class' => array('dfp-textarea', 'dfr-tinymce-area')),
      );
      break;

    case 'date':
      $form['input'] = array(
        '#markup' => '<input type="date" class="dfp-input" value="' . check_plain($val) . '">',
      );
      break;

    case 'time':
      $form['input'] = array(
        '#markup' => '<input type="time" class="dfp-input" value="' . check_plain($val) . '">',
      );
      break;

    case 'radio':
      if (empty($opts)) {
        $form['no_options'] = array(
          '#markup' => '<p class="dfp-no-options">' . t('No options defined.') . '</p>',
        );
        break;
      }
      // Pre-expand individual radios — form_expand_radios() is a #process
      // callback that does not run outside drupal_get_form().
      $form['options'] = array(
        '#type'       => 'container',
        '#attributes' => array('class' => array('dfp-options-list')),
      );
      foreach ($opts as $idx => $opt) {
        $key = $opt->option_label;
        $form['options']['opt_' . $idx] = array(
          '#type'         => 'radio',
          '#title'        => check_plain($key),
          '#return_value' => $key,
          '#value'        => ($val !== '') ? $val : NULL,
          '#name'         => 'dfr-radio-' . $qid,
          '#id'           => drupal_html_id('dfr-radio-' . $qid . '-' . $idx),
          '#attributes'   => array(),
        );
      }
      break;

    case 'checkbox':
      if (empty($opts)) {
        $form['no_options'] = array(
          '#markup' => '<p class="dfp-no-options">' . t('No options defined.') . '</p>',
        );
        break;
      }
      $checked_map = array();
      if ($existing_value !== NULL) {
        $dec = @json_decode($existing_value, TRUE);
        if (is_array($dec)) {
          $checked_map = array_flip($dec);
        }
      }
      // Pre-expand individual checkboxes — same reason as radio above.
      $form['options'] = array(
        '#type'       => 'container',
        '#attributes' => array('class' => array('dfp-options-list')),
      );
      foreach ($opts as $idx => $opt) {
        $key        = $opt->option_label;
        $is_checked = isset($checked_map[$key]);
        $form['options']['opt_' . $idx] = array(
          '#type'         => 'checkbox',
          '#title'        => check_plain($key),
          '#return_value' => $key,
          '#value'        => $is_checked ? $key : 0,
          '#name'         => 'dfr-cb-' . $qid . '[' . $idx . ']',
          '#id'           => drupal_html_id('dfr-cb-' . $qid . '-' . $idx),
          '#attributes'   => array(),
        );
      }
      break;

    case 'select':
      if (empty($opts)) {
        $form['no_options'] = array(
          '#markup' => '<p class="dfp-no-options">' . t('No options defined.') . '</p>',
        );
        break;
      }
      $allow_multi    = !empty($scale['allow_multiple']);
      $select_classes = array('dfp-select', 'dfp-select2');
      if ($allow_multi) {
        $select_classes[] = 'dfp-select-multi';
      }
      $sel_val = '';
      if ($allow_multi && $existing_value !== NULL) {
        $dec     = @json_decode($existing_value, TRUE);
        $sel_val = is_array($dec) ? $dec : array();
      }
      elseif (!$allow_multi && $existing_value !== NULL) {
        $sel_val = $existing_value;
      }
      $options = array();
      foreach ($opts as $opt) {
        $options[$opt->option_label] = check_plain($opt->option_label);
      }
      $select_options = $allow_multi
        ? $options
        : array('' => t('— Select an option —')) + $options;
      // drupal_render() does not invoke #process callbacks, so form_process_select()
      // never runs and #multiple has no effect on the HTML output. Add the
      // multiple attribute directly so Select2 renders as a multi-select widget.
      $select_attrs = array('class' => $select_classes);
      if ($allow_multi) {
        $select_attrs['multiple'] = 'multiple';
      }
      $form['input'] = array(
        '#type'          => 'select',
        '#title'         => '',
        '#title_display' => 'invisible',
        '#required'      => FALSE,
        '#options'       => $select_options,
        '#multiple'      => $allow_multi,
        '#value'         => $sel_val,
        '#attributes'    => $select_attrs,
      );
      if ($allow_multi) {
        $form['multi_hint'] = array(
          '#markup' => '<p class="dfp-multi-select-hint">'
            . '<svg xmlns="http://www.w3.org/2000/svg" width="12" height="12" viewBox="0 0 24 24" fill="none"'
            . ' stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round">'
            . '<polyline points="9 11 12 14 22 4"/>'
            . '<path d="M21 12v7a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h11"/>'
            . '</svg>'
            . ' ' . t('Multiple options can be selected') . '</p>',
        );
      }
      break;

    case 'rating':
      $min       = isset($scale['scale_min']) ? (int) $scale['scale_min'] : 1;
      $max       = isset($scale['scale_max']) ? (int) $scale['scale_max'] : 5;
      $min_label = isset($scale['scale_min_label']) ? check_plain($scale['scale_min_label']) : '';
      $max_label = isset($scale['scale_max_label']) ? check_plain($scale['scale_max_label']) : '';
      $cur       = ($existing_value !== NULL) ? (int) $existing_value : 0;
      $stars     = array();
      for ($i = $min; $i <= $max; $i++) {
        $stars[] = array('value' => $i, 'active' => ($i <= $cur));
      }
      $form['widget'] = array(
        '#markup' => theme('dynamic_form_response_rating_widget', array(
          'stars'     => $stars,
          'current'   => $cur,
          'min_label' => $min_label,
          'max_label' => $max_label,
        )),
      );
      break;

    case 'linear_scale':
      $min       = isset($scale['scale_min']) ? (int) $scale['scale_min'] : 1;
      $max       = isset($scale['scale_max']) ? (int) $scale['scale_max'] : 5;
      $min_label = isset($scale['scale_min_label']) ? check_plain($scale['scale_min_label']) : '';
      $max_label = isset($scale['scale_max_label']) ? check_plain($scale['scale_max_label']) : '';
      $cur       = ($existing_value !== NULL) ? (int) $existing_value : NULL;
      $buttons   = array();
      for ($i = $min; $i <= $max; $i++) {
        $buttons[] = array('value' => $i, 'active' => ($cur !== NULL && $cur === $i));
      }
      $form['widget'] = array(
        '#markup' => theme('dynamic_form_response_linear_scale_widget', array(
          'buttons'   => $buttons,
          'current'   => $cur,
          'min_label' => $min_label,
          'max_label' => $max_label,
        )),
      );
      break;

    case 'tags':
      $ac_url = check_url(url('dynamic-form/ajax/tags/autocomplete'));
      $tv     = '';
      if ($existing_value !== NULL) {
        $dec = @json_decode($existing_value, TRUE);
        if (is_array($dec)) {
          $tv = implode(',', array_map('check_plain', $dec));
        }
      }
      $form['input'] = array(
        '#type'          => 'hidden',
        '#title'         => '',
        '#title_display' => 'invisible',
        '#required'      => FALSE,
        '#value'         => $tv,
        '#attributes'    => array(
          'class'                 => array('dfp-tags-select2'),
          'data-autocomplete-url' => $ac_url,
          'style'                 => 'width:100%',
        ),
      );
      $form['hint'] = array(
        '#markup' => '<p class="dfp-help-text">'
          . t('Type a tag and press Enter, or search existing tags.') . '</p>',
      );
      break;

    case 'file':
      $file_val_attr  = '';
      $file_name_disp = '';
      if ($existing_value !== NULL && $existing_value !== '') {
        $fdec = @json_decode($existing_value, TRUE);
        if (is_array($fdec)) {
          $flist = isset($fdec['fid']) ? array($fdec) : array_values($fdec);
          $names = array();
          foreach ($flist as $fitem) {
            if (!empty($fitem['filename'])) {
              $names[] = $fitem['filename'];
            }
          }
          if ($names) {
            $file_val_attr  = check_plain($existing_value);
            $file_name_disp = check_plain(implode(', ', $names));
          }
        }
      }
      $accept_list = array();
      if (!empty($q->allow_file_types)) {
        foreach (array_filter(array_map('trim', explode(',', $q->allow_file_types))) as $ft) {
          $accept_list[] = '.' . $ft;
        }
      }
      $badges = '';
      if (!empty($q->allow_file_types)) {
        foreach (array_filter(array_map('trim', explode(',', $q->allow_file_types))) as $ft) {
          $badges .= '<span class="dfp-file-type-badge">' . check_plain(drupal_strtoupper($ft)) . '</span>';
        }
      }
      $size_pill  = !empty($q->max_file_size)
        ? '<span class="dfp-file-size-pill">' . t('Max @mb MB', array('@mb' => round($q->max_file_size / 1048576, 1))) . '</span>'
        : '';
      $files_pill = !empty($q->max_files)
        ? '<span class="dfp-file-size-pill">' . t('Up to @n file(s)', array('@n' => (int) $q->max_files)) . '</span>'
        : '';
      $meta = '';
      if ($badges || $size_pill || $files_pill) {
        $meta = '<div class="dfp-file-meta">'
          . ($badges ? '<div class="dfp-file-types">' . $badges . '</div>' : '')
          . $size_pill . $files_pill . '</div>';
      }
      $file_attrs = array(
        'class'            => array('dfp-file-hidden'),
        'data-question-id' => (string) $qid,
      );
      if ($accept_list) {
        $file_attrs['accept'] = implode(',', $accept_list);
      }
      if (!empty($q->max_files) && (int) $q->max_files > 1) {
        $file_attrs['multiple'] = 'multiple';
      }
      $svg = '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none"'
        . ' stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">'
        . '<path d="M20.39 18.39A5 5 0 0 0 18 9h-1.26A8 8 0 1 0 3 16.3"/>'
        . '<polyline points="16 16 12 12 8 16"/><line x1="12" y1="12" x2="12" y2="21"/></svg>';
      $dropzone_classes = array('dfp-file-dropzone');
      if ($file_name_disp) {
        $dropzone_classes[] = 'dfp-file-done';
      }
      $form['dropzone'] = array(
        '#type'       => 'container',
        '#attributes' => array('class' => $dropzone_classes),
        'file_value'  => array(
          '#type'       => 'hidden',
          '#value'      => $file_val_attr,
          '#attributes' => array('class' => array('dfp-file-value')),
        ),
        'file_input'  => array(
          '#type'          => 'file',
          '#title'         => '',
          '#title_display' => 'invisible',
          '#required'      => FALSE,
          '#attributes'    => $file_attrs,
        ),
        'dropzone_inner' => array(
          '#markup' => theme('dynamic_form_response_file_dropzone', array(
            'svg'       => $svg,
            'has_file'  => (bool) $file_name_disp,
            'file_name' => $file_name_disp,
            'meta_html' => $meta,
          )),
        ),
      );
      break;

    default:
      $attrs = array('class' => array('dfp-input'));
      if (!empty($q->placeholder)) {
        $attrs['placeholder'] = check_plain($q->placeholder);
      }
      $form['input'] = array(
        '#type'          => 'textfield',
        '#title'         => '',
        '#title_display' => 'invisible',
        '#required'      => FALSE,
        '#value'         => check_plain($val),
        '#attributes'    => $attrs,
      );
      break;
  }

  return $form;
}

/* =====================================================================
   ASSET LOADERS
   ===================================================================== */

/**
 * Loads Select2 and inline initialisation JS when the form has select/tags questions.
 */
function _dynamic_form_response_maybe_load_select2($form_id) {
  $has = FALSE;
  foreach (dynamic_form_get_questions($form_id) as $q) {
    if ($q->type === 'select' || $q->type === 'tags') {
      $has = TRUE;
      break;
    }
  }

  if (!$has) {
    return;
  }

  $base = 'sites/all/libraries/select2';
  if (!file_exists(DRUPAL_ROOT . '/' . $base . '/select2.min.js')) {
    return;
  }

  $module_path = drupal_get_path('module', 'dynamic_form_response');
  drupal_add_css($base . '/select2.css');
  drupal_add_js($base . '/select2.min.js', array('weight' => 10));
  drupal_add_js($module_path . '/js/response-select2.js', array('scope' => 'footer', 'weight' => 20));
}

/**
 * Loads TinyMCE and inline initialisation JS when the form has text_editor questions.
 */
function _dynamic_form_response_maybe_load_tinymce($form_id) {
  $has = FALSE;
  foreach (dynamic_form_get_questions($form_id) as $q) {
    if ($q->type === 'text_editor') {
      $has = TRUE;
      break;
    }
  }

  if (!$has) {
    return;
  }

  $tinymce = 'sites/all/libraries/tinymce/js/tinymce/tinymce.min.js';
  if (!file_exists(DRUPAL_ROOT . '/' . $tinymce)) {
    return;
  }

  $module_path = drupal_get_path('module', 'dynamic_form_response');
  drupal_add_js($tinymce, array('type' => 'file', 'weight' => 10));
  drupal_add_js($module_path . '/js/response-tinymce.js', array('scope' => 'footer', 'weight' => 20));
}

/* =====================================================================
   AJAX ENDPOINTS
   ===================================================================== */

/**
 * AJAX callback: POST dynamic-form-response/ajax/guest-email
 *
 * Validates an email address submitted by an anonymous visitor via the
 * capture popup. Returns { status: 'ok' } or { status: 'error', message }.
 * The caller is responsible for passing the validated email in subsequent
 * save requests so it can be persisted in dynamic_form_response_sessions.
 */
function dynamic_form_response_ajax_guest_email() {
  // 5 guest-email lookups per hour per IP — prevents email harvesting.
  $ip = ip_address();
  if (!flood_is_allowed('dfr_guest_email', 5, 3600, $ip)) {
    drupal_add_http_header('Status', '429 Too Many Requests');
    drupal_add_http_header('Retry-After', '3600');
    drupal_json_output(array('status' => 'error', 'message' => t('Too many requests. Please try again later.')));
    drupal_exit();
  }
  flood_register_event('dfr_guest_email', 3600, $ip);
  drupal_json_output(_dynamic_form_response_do_guest_email());
}

function _dynamic_form_response_do_guest_email() {
  global $user;
  if ($user->uid) {
    return array('status' => 'ok');
  }

  if (!_dynamic_form_response_request_token_valid()) {
    return array('status' => 'error', 'message' => t('Invalid security token.'));
  }

  $email = isset($_POST['email']) ? trim($_POST['email']) : '';
  if (!valid_email_address($email)) {
    return array('status' => 'error', 'message' => t('Please enter a valid email address.'));
  }

  return array('status' => 'ok');
}

/**
 * AJAX callback: POST dynamic-form-response/ajax/save
 *
 * Creates or updates a draft response with the given answers.
 * Returns JSON: { status: 'ok', response_id: N } or { status: 'error', message: '...' }
 */
function dynamic_form_response_ajax_save() {
  // 120 auto-saves per hour per IP (one every 30 s is normal usage).
  $ip = ip_address();
  if (!flood_is_allowed('dfr_response_save', 120, 3600, $ip)) {
    drupal_add_http_header('Status', '429 Too Many Requests');
    drupal_add_http_header('Retry-After', '60');
    drupal_json_output(array('status' => 'error', 'message' => t('Too many requests. Please try again later.')));
    drupal_exit();
  }
  flood_register_event('dfr_response_save', 3600, $ip);
  drupal_json_output(_dynamic_form_response_do_save());
}

function _dynamic_form_response_do_save() {
  if (!_dynamic_form_response_request_token_valid()) {
    return array('status' => 'error', 'message' => t('Invalid security token.'));
  }

  $form_id     = isset($_POST['form_id'])     ? (int) $_POST['form_id']     : 0;
  $response_id = isset($_POST['response_id']) ? (int) $_POST['response_id'] : 0;
  $answers     = isset($_POST['answers'])     ? $_POST['answers']            : array();

  // Guest email submitted by anonymous visitor via the capture popup.
  $guest_email = NULL;
  if (!isset($GLOBALS['user']) || !$GLOBALS['user']->uid) {
    $raw_email = isset($_POST['guest_email']) ? trim($_POST['guest_email']) : '';
    if ($raw_email && valid_email_address($raw_email)) {
      $guest_email = $raw_email;
    }
  }

  if (!$form_id) {
    return array('status' => 'error', 'message' => t('Missing form ID.'));
  }

  $form = DynamicFormResponseEntityController::loadForm($form_id);
  if (!$form || $form->status != 1) {
    return array('status' => 'error', 'message' => t('Form not found or not accepting responses.'));
  }

  $now = REQUEST_TIME;
  if ($form->opens_at && $form->opens_at > $now) {
    return array('status' => 'error', 'message' => t('This form is not open yet.'));
  }
  if ($form->closes_at && $form->closes_at < $now) {
    return array('status' => 'error', 'message' => t('This form is closed.'));
  }

  global $user;

  // Anonymous users may only save drafts for public forms.
  if (!$user->uid && $form->visibility !== 'p') {
    return array('status' => 'error', 'message' => t('You must be logged in to submit this form.'));
  }

  if ($response_id) {
    $draft = DynamicFormResponseEntityController::load($response_id);
    if (!$draft || (int) $draft->form_id !== $form_id || $draft->status != 0) {
      return array('status' => 'error', 'message' => t('Draft not found.'));
    }

    if ($user->uid) {
      if ((int) $draft->user_id !== (int) $user->uid) {
        return array('status' => 'error', 'message' => t('Access denied.'));
      }
    } else {
      $guest_token = _dynamic_form_response_get_guest_token();
      if (!DynamicFormResponseEntityController::isGuestSessionOwner($response_id, $guest_token)) {
        return array('status' => 'error', 'message' => t('Access denied.'));
      }
    }

    DynamicFormResponseEntityController::touch($response_id, $now);
  } else {
    // Create a new draft response.
    $response_id = DynamicFormResponseEntityController::createDraft($form_id, $user->uid, $guest_email, $now);
    DynamicFormResponseEntityController::createSession(
      $response_id,
      $user->uid ? NULL : _dynamic_form_response_get_guest_token(),
      $now
    );
  }

  // Persist the guest email whenever it arrives on an existing draft.
  if (!$user->uid && $guest_email && $response_id) {
    DynamicFormResponseEntityController::setGuestEmail($response_id, $guest_email);
  }

  // Fetch question types for all answers being saved so tags can be routed
  // to the dedicated tag-save function.
  $q_types = array();
  $q_ids   = array_filter(array_map('intval', array_keys($answers)));
  if (!empty($q_ids)) {
    $q_id_set = array_flip($q_ids);
    foreach (dynamic_form_get_questions($form_id) as $q) {
      if (isset($q_id_set[(int) $q->id])) {
        $q_types[(int) $q->id] = $q->type;
      }
    }
  }

  // Upsert each submitted answer.
  foreach ($answers as $question_id => $value) {
    $question_id = (int) $question_id;
    if (!$question_id) {
      continue;
    }

    if (isset($q_types[$question_id]) && $q_types[$question_id] === 'tags') {
      // Parse submitted value into a flat array of raw tag strings.
      if (is_array($value)) {
        $raw_tags = array_values(array_filter($value, 'strlen'));
      }
      else {
        $decoded  = @json_decode($value, TRUE);
        $raw_tags = is_array($decoded)
          ? array_values(array_filter($decoded, 'strlen'))
          : array_values(array_filter(array_map('trim', explode(',', (string) $value)), 'strlen'));
      }
      module_load_include('inc', 'dynamic_form', 'includes/dynamic_form.tags');
      _dynamic_form_save_tag_answers((int) $response_id, $question_id, $raw_tags);
    }
    elseif (isset($q_types[$question_id]) && $q_types[$question_id] === 'file') {
      $answer_id = _dynamic_form_response_upsert_answer((int) $response_id, $question_id, (string) $value, $now);
      $fdec = @json_decode($value, TRUE);
      if ($answer_id && is_array($fdec)) {
        // Normalize: old format is {fid,filename,url}, new format is [{fid,...},...]
        $file_list = isset($fdec['fid']) ? array($fdec) : array_values($fdec);
        // Replace all existing file records for this answer.
        db_delete('dynamic_form_answer_files')
          ->condition('answer_id', $answer_id)
          ->execute();
        foreach ($file_list as $fitem) {
          if (empty($fitem['fid'])) { continue; }
          $managed = db_select('file_managed', 'fm')
            ->fields('fm', array('uri', 'filename', 'filemime', 'filesize'))
            ->condition('fid', (int) $fitem['fid'])
            ->execute()
            ->fetchObject();
          if ($managed) {
            db_insert('dynamic_form_answer_files')
              ->fields(array(
                'answer_id'  => $answer_id,
                'file_path'  => $managed->uri,
                'file_name'  => $managed->filename,
                'file_type'  => $managed->filemime,
                'file_size'  => (int) $managed->filesize,
                'created_at' => $now,
                'updated_at' => $now,
              ))
              ->execute();
          }
        }
      }
    }
    else {
      if (is_array($value)) {
        $value = drupal_json_encode(array_values(array_filter($value, 'strlen')));
      }
      _dynamic_form_response_upsert_answer((int) $response_id, $question_id, (string) $value, $now);
    }
  }

  return array('status' => 'ok', 'response_id' => (int) $response_id);
}

/**
 * AJAX callback: POST dynamic-form-response/ajax/submit
 *
 * Validates all required fields and marks the response as submitted.
 * Returns JSON: { status: 'ok' } or { status: 'error', errors: { qid: msg, … } }
 */
function dynamic_form_response_ajax_submit() {
  // 10 final submissions per hour per IP — genuine users rarely resubmit.
  $ip = ip_address();
  if (!flood_is_allowed('dfr_response_submit', 10, 3600, $ip)) {
    drupal_add_http_header('Status', '429 Too Many Requests');
    drupal_add_http_header('Retry-After', '3600');
    drupal_json_output(array('status' => 'error', 'message' => t('Too many submissions. Please try again later.')));
    drupal_exit();
  }
  flood_register_event('dfr_response_submit', 3600, $ip);
  drupal_json_output(_dynamic_form_response_do_submit());
}

function _dynamic_form_response_do_submit() {
  if (!_dynamic_form_response_request_token_valid()) {
    return array('status' => 'error', 'message' => t('Invalid security token.'));
  }

  $form_id     = isset($_POST['form_id'])     ? (int) $_POST['form_id']     : 0;
  $response_id = isset($_POST['response_id']) ? (int) $_POST['response_id'] : 0;

  if (!$form_id || !$response_id) {
    return array('status' => 'error', 'message' => t('Missing parameters.'));
  }

  global $user;

  // Anonymous users may only submit public forms.
  if (!$user->uid) {
    $form = DynamicFormResponseEntityController::loadForm($form_id);
    if (!$form || $form->status != 1 || $form->visibility !== 'p') {
      return array('status' => 'error', 'message' => t('You must be logged in to submit this form.'));
    }
  }

  $response = DynamicFormResponseEntityController::load($response_id);
  if (!$response || (int) $response->form_id !== $form_id || $response->status != 0) {
    return array('status' => 'error', 'message' => t('Response not found or already submitted.'));
  }

  // Ownership check.
  if ($user->uid) {
    if ((int) $response->user_id !== (int) $user->uid) {
      return array('status' => 'error', 'message' => t('Access denied.'));
    }
  } else {
    $guest_token = _dynamic_form_response_get_guest_token();
    if (!DynamicFormResponseEntityController::isGuestSessionOwner($response_id, $guest_token)) {
      return array('status' => 'error', 'message' => t('Access denied.'));
    }
  }

  // Validate required fields.
  $required = array();
  foreach (dynamic_form_get_questions($form_id) as $q) {
    if ($q->is_required) {
      $required[(int) $q->id] = $q;
    }
  }

  $answers = DynamicFormResponseEntityController::getAnswersMap($response_id);

  $errors = array();
  foreach ($required as $qid => $q) {
    $v = isset($answers[$qid]) ? trim($answers[$qid]) : '';
    if ($v === '' || $v === '[]' || $v === 'null') {
      $errors[(int) $qid] = t('This field is required.');
    }
  }
  if (!empty($errors)) {
    return array('status' => 'error', 'errors' => $errors);
  }

  // Validate custom rules (min_length, max_length, min_value, max_value, regex).
  $user_rules = db_select('dynamic_form_question_validations', 'v')
    ->fields('v')
    ->condition('question_id', array_keys($answers), 'IN')
    ->condition('rule_type', array('min_length', 'max_length', 'min_value', 'max_value', 'regex'), 'IN')
    ->execute()
    ->fetchAll();

  foreach ($user_rules as $rule) {
    $qid = (int) $rule->question_id;
    if (isset($errors[$qid])) { continue; }
    $val = isset($answers[$qid]) ? $answers[$qid] : '';
    if ($val === '') { continue; }
    $err = _dynamic_form_response_apply_rule($rule->rule_type, $rule->rule_value, $rule->error_message, $val);
    if ($err !== NULL) {
      $errors[$qid] = $err;
    }
  }
  if (!empty($errors)) {
    return array('status' => 'error', 'errors' => $errors);
  }

  $now = REQUEST_TIME;
  DynamicFormResponseEntityController::submit($response_id, $now);

  _dynamic_form_audit_log(
    'response', (int) $response_id, 'submitted',
    NULL, array('form_id' => $form_id, 'submitted_at' => $now)
  );

  if (module_exists('dynamic_form_solr')) {
    dynamic_form_solr_index_response($response_id);
  }

  return array('status' => 'ok');
}

/* =====================================================================
   HELPERS
   ===================================================================== */

/**
 * Returns TRUE when the CSRF token in the current POST request is valid.
 */
function _dynamic_form_response_request_token_valid() {
  $token = isset($_POST['token']) ? $_POST['token'] : '';
  return drupal_valid_token($token, 'dfr_submit', TRUE);
}

/**
 * Applies one validation rule to a value. Returns an error string or NULL.
 */
function _dynamic_form_response_apply_rule($rule_type, $rule_value, $error_message, $value) {
  $default_messages = array(
    'min_length' => t('Must be at least @n characters.', array('@n' => $rule_value)),
    'max_length' => t('Must be at most @n characters.',  array('@n' => $rule_value)),
    'min_value'  => t('Must be at least @n.',            array('@n' => $rule_value)),
    'max_value'  => t('Must be at most @n.',             array('@n' => $rule_value)),
    'regex'      => t('Invalid format.'),
  );
  $msg = (!empty($error_message)) ? $error_message : (isset($default_messages[$rule_type]) ? $default_messages[$rule_type] : t('Invalid value.'));

  switch ($rule_type) {
    case 'min_length':
      return (drupal_strlen($value) < (int) $rule_value) ? $msg : NULL;
    case 'max_length':
      return (drupal_strlen($value) > (int) $rule_value) ? $msg : NULL;
    case 'min_value':
      return (is_numeric($value) && (float) $value < (float) $rule_value) ? $msg : NULL;
    case 'max_value':
      return (is_numeric($value) && (float) $value > (float) $rule_value) ? $msg : NULL;
    case 'regex':
      $pattern = '/' . addcslashes($rule_value, '/') . '/u';
      // Cap backtracking to prevent ReDoS via malicious patterns stored in the DB.
      $prev_limit = ini_set('pcre.backtrack_limit', 10000);
      $match = @preg_match($pattern, $value);
      ini_set('pcre.backtrack_limit', $prev_limit);
      return ($match === 0) ? $msg : NULL;
  }
  return NULL;
}

/**
 * Returns a stable 64-char hex guest token for the current anonymous visitor.
 *
 * On first call (no cookie present), generates a cryptographically random
 * token, sets an httponly cookie valid for one year, and returns the value.
 * On subsequent calls the existing cookie value is returned after format
 * validation. Always returns NULL-safe for authenticated users — call only
 * when $user->uid === 0.
 */
function _dynamic_form_response_get_guest_token() {
  if (!empty($_COOKIE['dfr_guest'])) {
    $token = $_COOKIE['dfr_guest'];
    if (preg_match('/^[a-f0-9]{64}$/', $token)) {
      return $token;
    }
  }
  $token = bin2hex(drupal_random_bytes(32));
  setcookie('dfr_guest', $token, time() + 60 * 60 * 24 * 365, '/', '', FALSE, TRUE);
  $_COOKIE['dfr_guest'] = $token;
  return $token;
}

/**
 * Inserts or updates one answer row for a given response + question pair.
 */
function _dynamic_form_response_upsert_answer($response_id, $question_id, $value, $now = NULL) {
  if ($now === NULL) {
    $now = REQUEST_TIME;
  }

  return DynamicFormResponseEntityController::upsertAnswer($response_id, $question_id, $value, $now);
}

/**
 * Inserts or updates a dynamic_form_answer_files record for a file answer.
 */
function _dynamic_form_response_upsert_answer_file($answer_id, $file_path, $file_name, $file_type, $file_size, $now = NULL) {
  if ($now === NULL) {
    $now = REQUEST_TIME;
  }

  $existing = db_select('dynamic_form_answer_files', 'f')
    ->fields('f', array('id'))
    ->condition('answer_id', $answer_id)
    ->isNull('deleted_at')
    ->execute()
    ->fetchField();

  if ($existing) {
    db_update('dynamic_form_answer_files')
      ->fields(array(
        'file_path'  => $file_path,
        'file_name'  => $file_name,
        'file_type'  => $file_type,
        'file_size'  => $file_size,
        'updated_at' => $now,
      ))
      ->condition('id', $existing)
      ->execute();
  } else {
    db_insert('dynamic_form_answer_files')
      ->fields(array(
        'answer_id'  => $answer_id,
        'file_path'  => $file_path,
        'file_name'  => $file_name,
        'file_type'  => $file_type,
        'file_size'  => $file_size,
        'created_at' => $now,
        'updated_at' => $now,
      ))
      ->execute();
  }
}

/* =====================================================================
   THANK-YOU PAGE
   ===================================================================== */

/**
 * Page callback: forms/%/thank-you
 */
function dynamic_form_response_thankyou_page($slug) {
  $record = db_select('dynamic_form_forms', 'f')
    ->fields('f', array('title', 'slug'))
    ->condition('slug', check_plain($slug))
    ->isNull('deleted_at')
    ->execute()
    ->fetchObject();

  if (!$record) {
    return drupal_not_found();
  }

  drupal_set_title(check_plain($record->title));
  $module_path = drupal_get_path('module', 'dynamic_form_response');
  drupal_add_css($module_path . '/css/response-form.css');

  return theme('dynamic_form_response_thankyou', array(
    'title'    => check_plain($record->title),
    'back_url' => url('forms'),
  ));
}

/* =====================================================================
   FILE UPLOAD ENDPOINT
   ===================================================================== */

/**
 * AJAX callback: POST dynamic-form-response/ajax/upload
 *
 * Accepts a single file, validates type/size against the question's
 * constraints, saves it to Drupal's managed file system, and returns
 * JSON: { status:'ok', fid:N, filename:'...', url:'...' }
 */
function dynamic_form_response_ajax_upload() {
  // 30 uploads per hour per IP — file I/O is the most expensive operation.
  $ip = ip_address();
  if (!flood_is_allowed('dfr_response_upload', 30, 3600, $ip)) {
    drupal_add_http_header('Status', '429 Too Many Requests');
    drupal_add_http_header('Retry-After', '3600');
    drupal_json_output(array('status' => 'error', 'message' => t('Too many uploads. Please try again later.')));
    drupal_exit();
  }
  flood_register_event('dfr_response_upload', 3600, $ip);
  drupal_json_output(_dynamic_form_response_do_upload());
}

function _dynamic_form_response_do_upload() {
  if (!_dynamic_form_response_request_token_valid()) {
    return array('status' => 'error', 'message' => t('Invalid security token.'));
  }

  $form_id     = isset($_POST['form_id'])     ? (int) $_POST['form_id']     : 0;
  $question_id = isset($_POST['question_id']) ? (int) $_POST['question_id'] : 0;

  if (!$form_id || !$question_id) {
    return array('status' => 'error', 'message' => t('Missing parameters.'));
  }

  if (empty($_FILES['file']['tmp_name']) || $_FILES['file']['error'] !== UPLOAD_ERR_OK) {
    $codes = array(
      UPLOAD_ERR_INI_SIZE  => t('File exceeds the server upload limit.'),
      UPLOAD_ERR_FORM_SIZE => t('File exceeds the form size limit.'),
      UPLOAD_ERR_PARTIAL   => t('File was only partially uploaded.'),
      UPLOAD_ERR_NO_FILE   => t('No file was received.'),
    );
    $err_code = isset($_FILES['file']['error']) ? (int) $_FILES['file']['error'] : UPLOAD_ERR_NO_FILE;
    return array(
      'status'  => 'error',
      'message' => isset($codes[$err_code]) ? $codes[$err_code] : t('Upload error.'),
    );
  }

  // Confirm form is active.
  $form = DynamicFormResponseEntityController::loadForm($form_id);
  if (!$form || $form->status != 1) {
    return array('status' => 'error', 'message' => t('Form not found.'));
  }

  // Load question constraints.
  $question = entity_load_single('dynamic_form_question', $question_id);
  if (!$question || (int) $question->form_id !== $form_id || $question->type !== 'file') {
    return array('status' => 'error', 'message' => t('File question not found.'));
  }

  $file_tmp  = $_FILES['file']['tmp_name'];
  $file_name = $_FILES['file']['name'];
  $file_size = $_FILES['file']['size'];

  // Always block executable extensions regardless of the configured allowlist.
  $_dangerous_exts = array(
    'php', 'php3', 'php4', 'php5', 'php7', 'phtml', 'phar',
    'asp', 'aspx', 'cgi', 'pl', 'py', 'rb', 'sh', 'bash',
    'exe', 'htaccess', 'shtml',
  );
  $_upload_ext = drupal_strtolower(pathinfo(drupal_basename($file_name), PATHINFO_EXTENSION));
  if (in_array($_upload_ext, $_dangerous_exts)) {
    return array(
      'status'  => 'error',
      'message' => t('File type .@ext is not allowed.', array('@ext' => $_upload_ext)),
    );
  }

  // Validate file extension against the configured allowlist.
  if (!empty($question->allow_file_types)) {
    $allowed = array_filter(array_map('drupal_strtolower', array_map('trim', explode(',', $question->allow_file_types))));
    $ext = drupal_strtolower(pathinfo($file_name, PATHINFO_EXTENSION));
    if (!in_array($ext, $allowed)) {
      return array(
        'status'  => 'error',
        'message' => t('File type .@ext is not allowed. Allowed: @list.', array(
          '@ext'  => $ext,
          '@list' => implode(', ', $allowed),
        )),
      );
    }
  }

  // Validate file size.
  if (!empty($question->max_file_size) && $file_size > (int) $question->max_file_size) {
    return array(
      'status'  => 'error',
      'message' => t('File exceeds the @mb MB limit.', array(
        '@mb' => round((int) $question->max_file_size / 1048576, 1),
      )),
    );
  }

  // Prepare upload directory.
  $dir = 'public://form-responses/' . $form_id . '/' . $question_id;
  if (!file_prepare_directory($dir, FILE_CREATE_DIRECTORY | FILE_MODIFY_PERMISSIONS)) {
    return array('status' => 'error', 'message' => t('Could not create upload directory.'));
  }

  // Sanitise filename and resolve name conflicts.
  // Pass the allowlist so file_munge_filename catches dual-extension tricks.
  $_munge_exts = !empty($question->allow_file_types)
    ? implode(' ', array_filter(array_map('trim', explode(',', $question->allow_file_types))))
    : '';
  $safe_name = file_munge_filename(drupal_basename($file_name), $_munge_exts);
  $dest      = file_destination($dir . '/' . $safe_name, FILE_EXISTS_RENAME);

  if (!drupal_move_uploaded_file($file_tmp, $dest)) {
    return array('status' => 'error', 'message' => t('Could not save the uploaded file.'));
  }

  // Register as a Drupal managed file.
  global $user;
  $file            = new stdClass();
  $file->uid       = (int) $user->uid;
  $file->filename  = drupal_basename($dest);
  $file->uri       = $dest;
  $file->filemime  = file_get_mimetype($dest);
  $file->filesize  = $file_size;
  $file->status    = FILE_STATUS_PERMANENT;
  $file->timestamp = REQUEST_TIME;
  file_save($file);

  return array(
    'status'   => 'ok',
    'fid'      => (int) $file->fid,
    'filename' => $file->filename,
    'url'      => file_create_url($file->uri),
  );
}
