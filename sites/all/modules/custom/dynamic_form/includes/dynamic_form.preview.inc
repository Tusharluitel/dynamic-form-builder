<?php

/**
 * Page callback: dashboard/forms/%/preview
 */
function dynamic_form_preview_page($form_id) {
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

  drupal_set_title(t('Preview: @title', array('@title' => $form_record->title)));

  $module_path = drupal_get_path('module', 'dynamic_form');
  drupal_add_css($module_path . '/css/preview.css');
  drupal_add_js($module_path . '/js/preview.js', array('scope' => 'footer', 'weight' => 15));

  _dynamic_form_preview_maybe_load_tinymce($form_id);
  _dynamic_form_preview_maybe_load_select2($form_id);

  module_load_include('inc', 'dynamic_form', 'includes/dynamic_form.dashboard');
  $sidebar_html = _dynamic_form_dashboard_sidebar('forms');

  $content = theme('dynamic_form_preview_page', array(
    'form_record' => $form_record,
    'back_url'    => l(t('← Back to Builder'), 'dashboard/forms/' . (int) $form_id . '/builder', array(
      'attributes' => array('class' => array('dfp-back-link')),
    )),
    'body_html'   => drupal_render(drupal_get_form('dynamic_form_preview_form', $form_record)),
  ));

  return theme('dynamic_form_dashboard', array(
    'sidebar' => $sidebar_html,
    'content' => $content,
  ));
}

/**
 * FAPI form builder for the admin preview page.
 *
 * Builds the complete paginated question list as a $form array so every
 * question input is a proper FAPI element nested under its section and
 * column containers. The pager is server-side (Drupal's
 * pager_default_initialize / theme_pager), so each page link triggers a
 * fresh page load rather than an AJAX rebuild.
 *
 * @param array  $form
 * @param array &$form_state
 * @param object $form_record  dynamic_form_forms DB row.
 */
function dynamic_form_preview_form($form, &$form_state, $form_record) {
  $form_state['build_info']['files'][] = array(
    'type'   => 'inc',
    'module' => 'dynamic_form',
    'name'   => 'includes/dynamic_form.preview',
  );

  $form_id = (int) $form_record->id;

  // Load sections ordered by position.
  $sections = db_select('dynamic_form_sections', 's')
    ->fields('s')
    ->condition('form_id', $form_id)
    ->isNull('deleted_at')
    ->orderBy('position', 'ASC')
    ->execute()
    ->fetchAll();

  // Load questions ordered by position.
  $questions = db_select('dynamic_form_questions', 'q')
    ->fields('q')
    ->condition('form_id', $form_id)
    ->isNull('deleted_at')
    ->orderBy('position', 'ASC')
    ->execute()
    ->fetchAll();

  $question_ids         = array();
  $questions_by_section = array();
  foreach ($questions as $q) {
    $question_ids[] = (int) $q->id;
    $sid = $q->section_id ? (int) $q->section_id : 0;
    $questions_by_section[$sid][] = $q;
  }

  // Load options grouped by question.
  $options_by_question = array();
  if (!empty($question_ids)) {
    $opts = db_select('dynamic_form_question_options', 'o')
      ->fields('o')
      ->condition('question_id', $question_ids, 'IN')
      ->isNull('deleted_at')
      ->orderBy('position', 'ASC')
      ->execute()
      ->fetchAll();
    foreach ($opts as $opt) {
      $options_by_question[(int) $opt->question_id][] = $opt;
    }
  }

  // Load scale meta and allow_multiple flags grouped by question.
  $scale_by_question = array();
  if (!empty($question_ids)) {
    $metas = db_select('dynamic_form_question_validations', 'v')
      ->fields('v')
      ->condition('question_id', $question_ids, 'IN')
      ->condition('rule_type', array('scale_min', 'scale_max', 'scale_min_label', 'scale_max_label', 'allow_multiple'), 'IN')
      ->execute()
      ->fetchAll();
    foreach ($metas as $meta) {
      $scale_by_question[(int) $meta->question_id][$meta->rule_type] = $meta->rule_value;
    }
  }

  // Flatten questions in section order.
  $flat = array();
  foreach ($sections as $section) {
    $sid = (int) $section->id;
    $qs  = isset($questions_by_section[$sid]) ? $questions_by_section[$sid] : array();
    foreach ($qs as $q) {
      $flat[] = array('q' => $q, 'section' => $section);
    }
  }

  if (empty($flat)) {
    $form['empty'] = array(
      '#markup' => '<p class="dfp-empty">' . t('No sections or questions have been added yet.') . '</p>',
    );
    return $form;
  }

  // Pagination — pager_default_initialize() registers the pager globally
  // and returns the current 0-based page number from $_GET['page'].
  $qpp          = max(1, (int) $form_record->question_per_page);
  $total_q      = count($flat);
  $use_pager    = $total_q > $qpp;
  $current_page = $use_pager ? pager_default_initialize($total_q, $qpp) : 0;
  $page_items   = array_slice($flat, $current_page * $qpp, $qpp);

  $col_map = array('full' => 12, 'half' => 6, 'third' => 4, 'quarter' => 3);

  $form['content'] = array(
    '#type'       => 'container',
    '#attributes' => array('class' => array('dfp-paginated-form')),
  );

  $prev_sid = -1;
  $sec_key  = NULL;
  $row_idx  = 0;
  $row_key  = NULL;
  $row_cols = 0;

  foreach ($page_items as $item) {
    $q   = $item['q'];
    $sec = $item['section'];
    $sid = (int) $sec->id;
    $qid = (int) $q->id;

    $width  = (!empty($q->width) && isset($col_map[$q->width])) ? $q->width : 'full';
    $q_cols = $col_map[$width];

    // ---- Section boundary -------------------------------------------
    if ($sid !== $prev_sid) {
      $sec_key = 'sec_' . $sid;
      $form['content'][$sec_key] = array(
        '#type'       => 'container',
        '#attributes' => array('class' => array('dfp-section')),
        'header'      => array(
          '#markup' => theme('dynamic_form_preview_section_header', array('section' => $sec)),
        ),
      );
      $prev_sid = $sid;
      $row_idx  = 0;
      $row_key  = NULL;
      $row_cols = 0;
    }

    // ---- Column row management --------------------------------------
    if ($row_key === NULL) {
      $row_key = 'row_' . $row_idx;
      $form['content'][$sec_key][$row_key] = array(
        '#type'       => 'container',
        '#attributes' => array('class' => array('dfp-question-row')),
      );
      $row_cols = 0;
    }
    elseif ($row_cols + $q_cols > 12) {
      $row_idx++;
      $row_key = 'row_' . $row_idx;
      $form['content'][$sec_key][$row_key] = array(
        '#type'       => 'container',
        '#attributes' => array('class' => array('dfp-question-row')),
      );
      $row_cols = 0;
    }
    $row_cols += $q_cols;

    $opts  = isset($options_by_question[$qid]) ? $options_by_question[$qid] : array();
    $scale = isset($scale_by_question[$qid])   ? $scale_by_question[$qid]   : array();

    // ---- Column container -------------------------------------------
    $form['content'][$sec_key][$row_key]['col_' . $qid] = array(
      '#type'       => 'container',
      '#attributes' => array('class' => array('dfp-question-col', 'dfp-col-' . check_plain($width))),
    );

    // ---- Question wrapper (.dfp-question) ---------------------------
    $form['content'][$sec_key][$row_key]['col_' . $qid]['question'] = array(
      '#type'       => 'container',
      '#attributes' => array(
        'class'     => array('dfp-question'),
        'data-type' => check_plain($q->type),
      ),
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
    $form['content'][$sec_key][$row_key]['col_' . $qid]['question']['label'] = array(
      '#markup' => $label_html,
    );

    // Input wrapper — merge the named sub-elements returned by the
    // builder directly into the container using PHP's + operator.
    $form['content'][$sec_key][$row_key]['col_' . $qid]['question']['input_wrap'] = array(
      '#type'       => 'container',
      '#attributes' => array('class' => array('dfp-question-input')),
    ) + _dynamic_form_preview_build_fapi_element($q, $opts, $scale);
  }

  if ($use_pager) {
    $form['pager'] = array(
      '#markup' => theme('pager', array('element' => 0)),
    );
  }

  return $form;
}

/* =====================================================================
   ASSET LOADERS
   ===================================================================== */

/**
 * Loads TinyMCE assets for the preview page if text_editor questions exist.
 */
function _dynamic_form_preview_maybe_load_tinymce($form_id) {
  $has_editor = db_select('dynamic_form_questions', 'q')
    ->fields('q', array('id'))
    ->condition('form_id', $form_id)
    ->condition('type', 'text_editor')
    ->isNull('deleted_at')
    ->range(0, 1)
    ->execute()
    ->fetchField();

  if (!$has_editor) {
    return;
  }

  $tinymce_js = 'sites/all/libraries/tinymce/js/tinymce/tinymce.min.js';
  if (!file_exists(DRUPAL_ROOT . '/' . $tinymce_js)) {
    return;
  }

  drupal_add_js($tinymce_js, array('type' => 'file', 'weight' => 10));
}

/**
 * Loads Select2 assets when the form has 'select' or 'tags' questions.
 */
function _dynamic_form_preview_maybe_load_select2($form_id) {
  $has_select2 = db_select('dynamic_form_questions', 'q')
    ->fields('q', array('id'))
    ->condition('form_id', $form_id)
    ->condition('type', array('select', 'tags'), 'IN')
    ->isNull('deleted_at')
    ->range(0, 1)
    ->execute()
    ->fetchField();

  if (!$has_select2) {
    return;
  }

  $select2_base = 'sites/all/libraries/select2';
  if (!file_exists(DRUPAL_ROOT . '/' . $select2_base . '/select2.min.js')) {
    return;
  }

  drupal_add_css($select2_base . '/select2.css');
  drupal_add_js($select2_base . '/select2.min.js', array('weight' => 10));

  // Pass the autocomplete URL via Drupal.settings rather than embedding
  // a PHP-generated URL directly inside JavaScript.
  drupal_add_js(array('dfPreview' => array(
    'autocompleteUrl' => url('dynamic-form/ajax/tags/autocomplete'),
  )), 'setting');
}

/* =====================================================================
   QUESTION INPUT BUILDER
   ===================================================================== */

/**
 * Builds a $form sub-array of named FAPI elements for one preview question.
 *
 * Returns an associative array of child elements (e.g. 'input', 'options',
 * 'widget') that the caller merges into the question's input_wrap container
 * using PHP's + operator. Inputs are read-only placeholders; no values are
 * pre-filled because the preview shows blank inputs.
 */
function _dynamic_form_preview_build_fapi_element($q, array $opts, array $scale) {
  $form = array();
  $type = $q->type;
  $qid  = (int) $q->id;

  // Build a keyed options array for FAPI choice elements.
  $options = array();
  foreach ($opts as $opt) {
    $options[$opt->option_label] = check_plain($opt->option_label);
  }

  switch ($type) {

    case 'text':
      $form['input'] = array(
        '#type'          => 'textfield',
        '#title'         => '',
        '#title_display' => 'invisible',
        '#required'      => FALSE,
        '#attributes'    => array('class' => array('dfp-input'), 'placeholder' => t('Short answer text')),
      );
      break;

    case 'textarea':
      $form['input'] = array(
        '#type'          => 'textarea',
        '#title'         => '',
        '#title_display' => 'invisible',
        '#required'      => FALSE,
        '#rows'          => 4,
        '#attributes'    => array('class' => array('dfp-textarea'), 'placeholder' => t('Long answer text')),
      );
      break;

    case 'text_editor':
      $form['input'] = array(
        '#type'          => 'textarea',
        '#title'         => '',
        '#title_display' => 'invisible',
        '#required'      => FALSE,
        '#rows'          => 6,
        '#id'            => 'dfp-tinymce-' . $qid,
        '#attributes'    => array('class' => array('dfp-text-editor-area')),
      );
      break;

    case 'radio':
      if (empty($options)) {
        $form['no_options'] = array(
          '#markup' => '<p class="dfp-no-options">' . t('No options defined.') . '</p>',
        );
        break;
      }
      // form_process_radios() is a #process callback that never runs outside
      // drupal_get_form(), so #type => 'radios' renders empty. Pre-expand instead.
      $form['options'] = array(
        '#type'       => 'container',
        '#attributes' => array('class' => array('dfp-options-list')),
      );
      $idx = 0;
      foreach ($options as $value => $label) {
        $form['options']['opt_' . $idx] = array(
          '#type'         => 'radio',
          '#title'        => $label,
          '#return_value' => $value,
          '#value'        => FALSE,
          '#name'         => 'dfp-preview-radio-' . $qid,
          '#id'           => drupal_html_id('dfp-preview-radio-' . $qid . '-' . $idx),
          '#attributes'   => array(),
        );
        $idx++;
      }
      break;

    case 'checkbox':
      if (empty($options)) {
        $form['no_options'] = array(
          '#markup' => '<p class="dfp-no-options">' . t('No options defined.') . '</p>',
        );
        break;
      }
      // form_process_checkboxes() has the same #process constraint as radios.
      $form['options'] = array(
        '#type'       => 'container',
        '#attributes' => array('class' => array('dfp-options-list')),
      );
      $idx = 0;
      foreach ($options as $value => $label) {
        $form['options']['opt_' . $idx] = array(
          '#type'         => 'checkbox',
          '#title'        => $label,
          '#return_value' => $value,
          '#value'        => 0,
          '#name'         => 'dfp-preview-cb-' . $qid . '[' . $idx . ']',
          '#id'           => drupal_html_id('dfp-preview-cb-' . $qid . '-' . $idx),
          '#attributes'   => array(),
        );
        $idx++;
      }
      break;

    case 'select':
      if (empty($options)) {
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
      $select_options = $allow_multi
        ? $options
        : array('' => t('— Select an option —')) + $options;
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

    case 'file':
      $type_badges = '';
      if (!empty($q->allow_file_types)) {
        foreach (array_filter(array_map('trim', explode(',', $q->allow_file_types))) as $ft) {
          $type_badges .= '<span class="dfp-file-type-badge">' . check_plain(drupal_strtoupper($ft)) . '</span>';
        }
      }
      $size_pill = !empty($q->max_file_size)
        ? '<span class="dfp-file-size-pill">' . t('Max @mb MB', array('@mb' => round($q->max_file_size / 1048576, 1))) . '</span>'
        : '';
      $files_pill = !empty($q->max_files)
        ? '<span class="dfp-file-size-pill">' . t('Up to @n file(s)', array('@n' => (int) $q->max_files)) . '</span>'
        : '';
      $meta_html = '';
      if ($type_badges || $size_pill || $files_pill) {
        $meta_html = '<div class="dfp-file-meta">'
          . ($type_badges ? '<div class="dfp-file-types">' . $type_badges . '</div>' : '')
          . $size_pill . $files_pill . '</div>';
      }
      $svg = '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none"'
        . ' stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">'
        . '<path d="M20.39 18.39A5 5 0 0 0 18 9h-1.26A8 8 0 1 0 3 16.3"/>'
        . '<polyline points="16 16 12 12 8 16"/>'
        . '<line x1="12" y1="12" x2="12" y2="21"/>'
        . '</svg>';
      // The outer .dfp-file-dropzone wrapper is provided by this container;
      // the template renders only the inner content.
      $form['dropzone'] = array(
        '#type'       => 'container',
        '#attributes' => array('class' => array('dfp-file-dropzone')),
        'inner'       => array(
          '#markup' => theme('dynamic_form_preview_file_dropzone', array(
            'svg'       => $svg,
            'meta_html' => $meta_html,
          )),
        ),
        'file_input'  => array(
          '#type'          => 'file',
          '#title'         => '',
          '#title_display' => 'invisible',
          '#required'      => FALSE,
          '#attributes'    => array('class' => array('dfp-file-hidden')),
        ),
      );
      break;

    case 'date':
      $form['input'] = array(
        '#markup' => '<input type="date" class="dfp-input">',
      );
      break;

    case 'time':
      $form['input'] = array(
        '#markup' => '<input type="time" class="dfp-input">',
      );
      break;

    case 'rating':
      $min       = isset($scale['scale_min']) ? (int) $scale['scale_min'] : 1;
      $max       = isset($scale['scale_max']) ? (int) $scale['scale_max'] : 5;
      $min_label = isset($scale['scale_min_label']) ? check_plain($scale['scale_min_label']) : '';
      $max_label = isset($scale['scale_max_label']) ? check_plain($scale['scale_max_label']) : '';
      $form['widget'] = array(
        '#markup' => theme('dynamic_form_preview_rating_widget', array(
          'stars'     => range($min, $max),
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
      $form['widget'] = array(
        '#markup' => theme('dynamic_form_preview_linear_scale_widget', array(
          'buttons'   => range($min, $max),
          'min_label' => $min_label,
          'max_label' => $max_label,
        )),
      );
      break;

    case 'tags':
      $form['input'] = array(
        '#type'          => 'hidden',
        '#title'         => '',
        '#title_display' => 'invisible',
        '#required'      => FALSE,
        '#attributes'    => array('class' => array('dfp-tags-select2'), 'style' => 'width:100%'),
      );
      $form['hint'] = array(
        '#markup' => '<p class="dfp-help-text">'
          . t('Type a tag and press Enter, or search existing tags.') . '</p>',
      );
      break;

    default:
      $form['input'] = array(
        '#type'          => 'textfield',
        '#title'         => '',
        '#title_display' => 'invisible',
        '#required'      => FALSE,
        '#attributes'    => array('class' => array('dfp-input'), 'placeholder' => t('Answer')),
      );
      break;
  }

  return $form;
}
