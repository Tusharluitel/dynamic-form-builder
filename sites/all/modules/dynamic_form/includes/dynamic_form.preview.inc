<?php

/**
 * @file
 * Form preview page — renders a read-only respondent view of the form.
 */

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

  // Load CKEditor when the form has text_editor questions.
  _dynamic_form_preview_maybe_load_ckeditor($form_id);

  // Load Select2 when the form has select questions.
  _dynamic_form_preview_maybe_load_select2($form_id);

  module_load_include('inc', 'dynamic_form', 'includes/dynamic_form.dashboard');
  $sidebar_html = _dynamic_form_dashboard_sidebar('forms');

  // Load sections ordered by position.
  $sections = db_select('dynamic_form_sections', 's')
    ->fields('s')
    ->condition('form_id', $form_id)
    ->condition('deleted_at', NULL, 'IS NULL')
    ->orderBy('position', 'ASC')
    ->execute()
    ->fetchAll();

  // Load all questions for this form.
  $questions = db_select('dynamic_form_questions', 'q')
    ->fields('q')
    ->condition('form_id', $form_id)
    ->condition('deleted_at', NULL, 'IS NULL')
    ->orderBy('position', 'ASC')
    ->execute()
    ->fetchAll();

  $question_ids          = array();
  $questions_by_section  = array();
  foreach ($questions as $q) {
    $question_ids[] = (int) $q->id;
    $sid = $q->section_id ? (int) $q->section_id : 0;
    $questions_by_section[$sid][] = $q;
  }

  // Load all options, grouped by question_id.
  $options_by_question = array();
  if (!empty($question_ids)) {
    $options = db_select('dynamic_form_question_options', 'o')
      ->fields('o')
      ->condition('question_id', $question_ids, 'IN')
      ->condition('deleted_at', NULL, 'IS NULL')
      ->orderBy('position', 'ASC')
      ->execute()
      ->fetchAll();
    foreach ($options as $opt) {
      $options_by_question[(int) $opt->question_id][] = $opt;
    }
  }

  // Load scale/rating meta-rules and select flags, grouped by question_id.
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

  // -----------------------------------------------------------------------
  // Render
  // -----------------------------------------------------------------------
  $output  = '<div class="dfp-preview-container">';

  // Banner.
  $output .= '<div class="dfp-preview-banner">';
  $output .= '<span class="dfp-preview-badge">' . t('Preview Mode') . '</span>';
  $output .= '<span class="dfp-preview-note">' . t('This is how respondents will see your form. Inputs are disabled in preview.') . '</span>';
  $output .= l(t('← Back to Builder'), 'dashboard/forms/' . (int) $form_id . '/builder', array(
    'attributes' => array('class' => array('dfp-back-link')),
  ));
  $output .= '</div>';

  // Form card.
  $output .= '<div class="dfp-form-card">';
  $output .= '<div class="dfp-form-header">';
  $output .= '<h1 class="dfp-form-title">' . check_plain($form_record->title) . '</h1>';
  if (!empty($form_record->description)) {
    $output .= '<p class="dfp-form-desc">' . check_markup($form_record->description) . '</p>';
  }
  $output .= '</div>';

  if (empty($sections)) {
    $output .= '<p class="dfp-empty">' . t('No sections or questions have been added yet.') . '</p>';
  }
  else {
    foreach ($sections as $section) {
      $sid = (int) $section->id;
      $qs  = isset($questions_by_section[$sid]) ? $questions_by_section[$sid] : array();

      $output .= '<div class="dfp-section">';
      $output .= '<div class="dfp-section-header">';
      $output .= '<h2 class="dfp-section-title">' . check_plain($section->name) . '</h2>';
      if (!empty($section->description)) {
        $output .= '<p class="dfp-section-desc">' . check_plain($section->description) . '</p>';
      }
      $output .= '</div>';

      if (empty($qs)) {
        $output .= '<p class="dfp-no-questions">' . t('No questions in this section yet.') . '</p>';
      }
      else {
        foreach ($qs as $q) {
          $qid   = (int) $q->id;
          $opts  = isset($options_by_question[$qid]) ? $options_by_question[$qid] : array();
          $scale = isset($scale_by_question[$qid])   ? $scale_by_question[$qid]   : array();
          $output .= _dynamic_form_render_preview_question($q, $opts, $scale);
        }
      }

      $output .= '</div>';
    }
  }

  $output .= '</div>'; // .dfp-form-card
  $output .= '</div>'; // .dfp-preview-container

  return theme('dynamic_form_dashboard', array(
    'sidebar' => $sidebar_html,
    'content' => $output,
  ));
}

/**
 * Loads CKEditor assets for the preview page if text_editor questions exist.
 *
 * Adds CKEditor's JS (resolved via the ckeditor module) and an inline
 * Drupal behaviour that initialises each .dfp-text-editor-area textarea
 * in read-only mode once the library is ready.
 *
 * @param int $form_id  The dynamic_form_forms record ID.
 */
function _dynamic_form_preview_maybe_load_ckeditor($form_id) {
  $has_editor = db_select('dynamic_form_questions', 'q')
    ->fields('q', array('id'))
    ->condition('form_id', $form_id)
    ->condition('type', 'text_editor')
    ->condition('deleted_at', NULL, 'IS NULL')
    ->range(0, 1)
    ->execute()
    ->fetchField();

  if (!$has_editor) {
    return;
  }

  if (!module_exists('ckeditor')) {
    return;
  }

  $cke_dir = ckeditor_path('url');
  if (empty($cke_dir)) {
    return;
  }

  // Load the CKEditor library JS.
  drupal_add_js($cke_dir . '/ckeditor.js', array('type' => 'external', 'weight' => 10));

  // Drupal behaviour: initialise each preview textarea as read-only CKEditor.
  $init_js = <<<JS
(function ($) {
  Drupal.behaviors.dfpCkeditorPreview = {
    attach: function (context, settings) {
      \$('.dfp-text-editor-area', context).once('dfp-cke-init', function () {
        var id = this.id;
        if (typeof CKEDITOR !== 'undefined' && id) {
          CKEDITOR.replace(id, {
            readOnly : true,
            height   : 180,
            toolbar  : [
              { name: 'basicstyles', items: ['Bold','Italic','Underline','Strike'] },
              { name: 'paragraph',   items: ['NumberedList','BulletedList','Blockquote'] },
              { name: 'links',       items: ['Link'] }
            ]
          });
        }
      });
    }
  };
})(jQuery);
JS;

  drupal_add_js($init_js, array('type' => 'inline', 'scope' => 'footer', 'weight' => 20));
}

/**
 * Loads Select2 assets for the preview page if select questions exist.
 *
 * Adds Select2 CSS/JS from sites/all/libraries/select2/ and an inline
 * Drupal behaviour that initialises each .dfp-select2 element.
 * The init is wrapped in try/catch so it degrades gracefully when
 * jQuery < 1.7 is installed (Select2 v3 needs .prop() from jQuery 1.6+).
 *
 * @param int $form_id  The dynamic_form_forms record ID.
 */
function _dynamic_form_preview_maybe_load_select2($form_id) {
  $has_select = db_select('dynamic_form_questions', 'q')
    ->fields('q', array('id'))
    ->condition('form_id', $form_id)
    ->condition('type', 'select')
    ->condition('deleted_at', NULL, 'IS NULL')
    ->range(0, 1)
    ->execute()
    ->fetchField();

  if (!$has_select) {
    return;
  }

  $select2_base = 'sites/all/libraries/select2';
  if (!file_exists(DRUPAL_ROOT . '/' . $select2_base . '/select2.min.js')) {
    return;
  }

  drupal_add_css($select2_base . '/select2.css');
  drupal_add_js($select2_base . '/select2.min.js', array('weight' => 10));

  $init_js = <<<JS
(function ($) {
  Drupal.behaviors.dfpSelect2Preview = {
    attach: function (context, settings) {
      $('.dfp-select2', context).once('dfp-s2-init', function () {
        try {
          $(this).select2({
            placeholder:    $(this).hasClass('dfp-select-multi') ? '' : '-- Select an option --',
            allowClear:     !$(this).hasClass('dfp-select-multi'),
            width:          '100%',
            disabled:       true
          });
        } catch (e) { /* graceful fallback for jQuery < 1.7 */ }
      });
    }
  };
})(jQuery);
JS;

  drupal_add_js($init_js, array('type' => 'inline', 'scope' => 'footer', 'weight' => 20));
}

/**
 * Renders one question as a disabled preview form element.
 *
 * @param object $q     Question DB row.
 * @param array  $opts  Option DB rows for this question.
 * @param array  $scale Associative scale meta (scale_min, scale_max, …).
 *
 * @return string HTML.
 */
function _dynamic_form_render_preview_question($q, array $opts, array $scale) {
  $type      = $q->type;
  $label     = check_plain($q->label);
  $req_html  = $q->is_required
    ? ' <span class="dfp-req" title="' . t('Required') . '">*</span>'
    : '';
  $desc_html = !empty($q->description)
    ? '<p class="dfp-help-text">' . check_plain($q->description) . '</p>'
    : '';

  $input_html = '';

  switch ($type) {

    case 'text':
      $input_html = '<input type="text" class="dfp-input" disabled'
        . ' placeholder="' . t('Short answer text') . '">';
      break;

    case 'textarea':
      $input_html = '<textarea class="dfp-textarea" disabled rows="4"'
        . ' placeholder="' . t('Long answer text') . '"></textarea>';
      break;

    case 'text_editor':
      $editor_id  = 'dfp-cke-' . (int) $q->id;
      $input_html = '<textarea id="' . $editor_id . '" class="dfp-text-editor-area" rows="6"></textarea>';
      break;

    case 'radio':
      if (empty($opts)) {
        $input_html = '<p class="dfp-no-options">' . t('No options defined.') . '</p>';
      }
      else {
        $input_html = '<div class="dfp-options-list">';
        foreach ($opts as $opt) {
          $input_html .= '<label class="dfp-option-label">'
            . '<input type="radio" disabled> '
            . check_plain($opt->option_label)
            . '</label>';
        }
        $input_html .= '</div>';
      }
      break;

    case 'checkbox':
      if (empty($opts)) {
        $input_html = '<p class="dfp-no-options">' . t('No options defined.') . '</p>';
      }
      else {
        $input_html = '<div class="dfp-options-list">';
        foreach ($opts as $opt) {
          $input_html .= '<label class="dfp-option-label">'
            . '<input type="checkbox" disabled> '
            . check_plain($opt->option_label)
            . '</label>';
        }
        $input_html .= '</div>';
      }
      break;

    case 'select':
      if (empty($opts)) {
        $input_html = '<p class="dfp-no-options">' . t('No options defined.') . '</p>';
      }
      else {
        $allow_multi  = !empty($scale['allow_multiple']);
        $multi_attr   = $allow_multi ? ' multiple' : '';
        $select_class = $allow_multi ? 'dfp-select dfp-select-multi dfp-select2' : 'dfp-select dfp-select2';
        $input_html   = '<select class="' . $select_class . '" disabled' . $multi_attr . '>';
        if (!$allow_multi) {
          $input_html .= '<option value="">' . t('— Select an option —') . '</option>';
        }
        foreach ($opts as $opt) {
          $input_html .= '<option>' . check_plain($opt->option_label) . '</option>';
        }
        $input_html .= '</select>';
      }
      break;

    case 'file':
      $input_html = '<input type="file" class="dfp-file" disabled>';
      if (!empty($q->allow_file_types)) {
        $input_html .= '<p class="dfp-help-text dfp-file-hint">'
          . t('Accepted types: @types', array('@types' => check_plain($q->allow_file_types)))
          . '</p>';
      }
      if (!empty($q->max_file_size)) {
        $mb = round($q->max_file_size / 1048576, 1);
        $input_html .= '<p class="dfp-help-text dfp-file-hint">'
          . t('Max size: @mb MB', array('@mb' => $mb))
          . '</p>';
      }
      break;

    case 'date':
      $input_html = '<input type="date" class="dfp-input" disabled>';
      break;

    case 'time':
      $input_html = '<input type="time" class="dfp-input" disabled>';
      break;

    case 'rating':
      $min       = isset($scale['scale_min']) ? (int) $scale['scale_min'] : 1;
      $max       = isset($scale['scale_max']) ? (int) $scale['scale_max'] : 5;
      $min_label = isset($scale['scale_min_label']) ? check_plain($scale['scale_min_label']) : '';
      $max_label = isset($scale['scale_max_label']) ? check_plain($scale['scale_max_label']) : '';

      $input_html = '<div class="dfp-rating-wrap">';
      if ($min_label) {
        $input_html .= '<span class="dfp-scale-label dfp-scale-label-min">' . $min_label . '</span>';
      }
      $input_html .= '<div class="dfp-stars">';
      for ($i = $min; $i <= $max; $i++) {
        $input_html .= '<span class="dfp-star" title="' . $i . '">&#9733;</span>';
      }
      $input_html .= '</div>';
      if ($max_label) {
        $input_html .= '<span class="dfp-scale-label dfp-scale-label-max">' . $max_label . '</span>';
      }
      $input_html .= '</div>';
      break;

    case 'linear_scale':
      $min       = isset($scale['scale_min']) ? (int) $scale['scale_min'] : 1;
      $max       = isset($scale['scale_max']) ? (int) $scale['scale_max'] : 5;
      $min_label = isset($scale['scale_min_label']) ? check_plain($scale['scale_min_label']) : '';
      $max_label = isset($scale['scale_max_label']) ? check_plain($scale['scale_max_label']) : '';

      $input_html = '<div class="dfp-scale-wrap">';
      $input_html .= '<div class="dfp-scale-buttons">';
      for ($i = $min; $i <= $max; $i++) {
        $input_html .= '<button type="button" class="dfp-scale-btn" disabled>' . $i . '</button>';
      }
      $input_html .= '</div>';
      if ($min_label || $max_label) {
        $input_html .= '<div class="dfp-scale-labels">';
        $input_html .= '<span class="dfp-scale-label dfp-scale-label-min">' . $min_label . '</span>';
        $input_html .= '<span class="dfp-scale-label dfp-scale-label-max">' . $max_label . '</span>';
        $input_html .= '</div>';
      }
      $input_html .= '</div>';
      break;

    default:
      $input_html = '<input type="text" class="dfp-input" disabled'
        . ' placeholder="' . t('Answer') . '">';
      break;
  }

  $output  = '<div class="dfp-question" data-type="' . check_plain($type) . '">';
  $output .= '<div class="dfp-question-label">' . $label . $req_html . '</div>';
  $output .= $desc_html;
  $output .= '<div class="dfp-question-input">' . $input_html . '</div>';
  $output .= '</div>';

  return $output;
}
