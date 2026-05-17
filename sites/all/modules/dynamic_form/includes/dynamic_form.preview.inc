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

  // Load TinyMCE when the form has text_editor questions.
  _dynamic_form_preview_maybe_load_tinymce($form_id);

  // Load Select2 when the form has select or tags questions.
  _dynamic_form_preview_maybe_load_select2($form_id);

  // Interactive behaviours for rating stars and linear scale buttons.
  $interactive_js = <<<JS
(function ($) {
  Drupal.behaviors.dfpPreviewInteractive = {
    attach: function (context, settings) {
      // Star rating — highlight stars on hover/click.
      $('.dfp-stars', context).once('dfp-stars-init', function () {
        var stars = $(this).find('.dfp-star');
        stars.bind('mouseover', function () {
          var idx = stars.index(this);
          stars.each(function (i) {
            $(this)[i <= idx ? 'addClass' : 'removeClass']('dfp-star-active');
          });
        }).bind('mouseout', function () {
          stars.each(function () {
            if (!$(this).data('selected')) {
              $(this).removeClass('dfp-star-active');
            }
          });
        }).bind('click', function () {
          var idx = stars.index(this);
          stars.each(function (i) {
            $(this).data('selected', i <= idx);
            $(this)[i <= idx ? 'addClass' : 'removeClass']('dfp-star-active');
          });
        });
      });

      // Linear scale — toggle active button.
      $('.dfp-scale-buttons', context).once('dfp-scale-init', function () {
        var btns = $(this).find('.dfp-scale-btn');
        btns.bind('click', function () {
          btns.removeClass('dfp-scale-active');
          $(this).addClass('dfp-scale-active');
        });
      });

      // File upload — show selected filename(s) below the dropzone prompt.
      $('.dfp-file-hidden', context).once('dfp-file-name-init', function () {
        var fileInput   = $(this);
        var fileNameDiv = fileInput.closest('.dfp-file-dropzone').find('.dfp-file-name');
        fileInput.bind('change', function () {
          var files = this.files;
          if (!files || files.length === 0) {
            fileNameDiv.text('').hide();
            return;
          }
          var names = [];
          for (var i = 0; i < files.length; i++) {
            names.push(files[i].name);
          }
          fileNameDiv.text(names.join(', ')).show();
        });
      });
    }
  };
})(jQuery);
JS;

  drupal_add_js($interactive_js, array('type' => 'inline', 'scope' => 'footer', 'weight' => 15));

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
  // Pagination setup — Drupal server-side pager
  // -----------------------------------------------------------------------
  $qpp = max(1, (int) $form_record->question_per_page);

  // Flatten all questions in section order, carrying section context.
  $flat = array();
  foreach ($sections as $section) {
    $sid = (int) $section->id;
    $qs  = isset($questions_by_section[$sid]) ? $questions_by_section[$sid] : array();
    foreach ($qs as $q) {
      $flat[] = array('q' => $q, 'section' => $section);
    }
  }

  $total_q   = count($flat);
  $use_pager = $total_q > $qpp;

  // pager_default_initialize() registers the pager globally and returns the
  // current 0-based page derived from $_GET['page'].
  $current_page = $use_pager ? pager_default_initialize($total_q, $qpp) : 0;
  $page_items   = array_slice($flat, $current_page * $qpp, $qpp);

  // -----------------------------------------------------------------------
  // Render
  // -----------------------------------------------------------------------
  $output  = '<div class="dfp-preview-container">';

  // Banner.
  $output .= '<div class="dfp-preview-banner">';
  $output .= '<span class="dfp-preview-badge">' . t('Preview Mode') . '</span>';
  $output .= '<span class="dfp-preview-note">' . t('This is how respondents will see your form.') . '</span>';
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

  if (empty($flat)) {
    $output .= '<p class="dfp-empty">' . t('No sections or questions have been added yet.') . '</p>';
  }
  else {
    $output .= '<div class="dfp-paginated-form">';

    // Column widths mapped to a 12-column grid.
    // Questions accumulate into a row until adding the next would exceed 12
    // columns, at which point a new row opens automatically.
    $col_map  = array('full' => 12, 'half' => 6, 'third' => 4, 'quarter' => 3);
    $prev_sid = -1;
    $row_cols = 0;
    $row_open = FALSE;

    foreach ($page_items as $item) {
      $q   = $item['q'];
      $sec = $item['section'];
      $sid = (int) $sec->id;

      $width  = (!empty($q->width) && isset($col_map[$q->width])) ? $q->width : 'full';
      $q_cols = $col_map[$width];

      // Section change: close any open row then close the old section.
      if ($sid !== $prev_sid) {
        if ($row_open) {
          $output .= '</div>'; // close .dfp-question-row
          $row_open = FALSE;
          $row_cols = 0;
        }
        if ($prev_sid !== -1) {
          $output .= '</div>'; // close .dfp-section
        }
        $output .= '<div class="dfp-section">';
        $output .= '<div class="dfp-section-header">';
        $output .= '<h2 class="dfp-section-title">' . check_plain($sec->name) . '</h2>';
        if (!empty($sec->description)) {
          $output .= '<p class="dfp-section-desc">' . check_plain($sec->description) . '</p>';
        }
        $output .= '</div>';
        $prev_sid = $sid;
      }

      // Row management: start a new row when the current one is full or
      // adding this question would exceed 12 columns.
      if (!$row_open) {
        $output .= '<div class="dfp-question-row">';
        $row_open = TRUE;
        $row_cols = 0;
      }
      elseif ($row_cols + $q_cols > 12) {
        $output .= '</div><div class="dfp-question-row">';
        $row_cols = 0;
      }

      $row_cols += $q_cols;

      $qid   = (int) $q->id;
      $opts  = isset($options_by_question[$qid]) ? $options_by_question[$qid] : array();
      $scale = isset($scale_by_question[$qid])   ? $scale_by_question[$qid]   : array();

      $output .= '<div class="dfp-question-col dfp-col-' . check_plain($width) . '">';
      $output .= _dynamic_form_render_preview_question($q, $opts, $scale);
      $output .= '</div>';
    }

    // Close the final open row and section.
    if ($row_open) {
      $output .= '</div>'; // close .dfp-question-row
    }
    if ($prev_sid !== -1) {
      $output .= '</div>'; // close last .dfp-section
    }

    if ($use_pager) {
      $output .= theme('pager', array('element' => 0));
    }

    $output .= '</div>'; // close .dfp-paginated-form
  }

  $output .= '</div>'; // .dfp-form-card
  $output .= '</div>'; // .dfp-preview-container

  return theme('dynamic_form_dashboard', array(
    'sidebar' => $sidebar_html,
    'content' => $output,
  ));
}

/**
 * Loads TinyMCE assets for the preview page if text_editor questions exist.
 *
 * Uses the library at sites/all/libraries/tinymce/js/tinymce/tinymce.min.js
 * and initialises each .dfp-text-editor-area textarea in read-only mode.
 *
 * @param int $form_id  The dynamic_form_forms record ID.
 */
function _dynamic_form_preview_maybe_load_tinymce($form_id) {
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

  $tinymce_js = 'sites/all/libraries/tinymce/js/tinymce/tinymce.min.js';
  if (!file_exists(DRUPAL_ROOT . '/' . $tinymce_js)) {
    return;
  }

  drupal_add_js($tinymce_js, array('type' => 'file', 'weight' => 10));

  $init_js = <<<JS
(function ($) {
  Drupal.behaviors.dfpTinymcePreview = {
    attach: function (context, settings) {
      \$('.dfp-text-editor-area', context).once('dfp-tinymce-init', function () {
        if (typeof tinymce === 'undefined') { return; }
        tinymce.init({
          selector  : '#' + this.id,
          height    : 200,
          menubar   : false,
          statusbar : false,
          plugins   : 'lists link',
          toolbar   : 'bold italic underline strikethrough | bullist numlist blockquote | link'
        });
      });
    }
  };
})(jQuery);
JS;

  drupal_add_js($init_js, array('type' => 'inline', 'scope' => 'footer', 'weight' => 20));
}

/**
 * Loads Select2 assets when the form has 'select' or 'tags' questions.
 *
 * Adds Select2 CSS/JS from sites/all/libraries/select2/ and inline
 * Drupal behaviours for both question types.
 *
 * @param int $form_id  The dynamic_form_forms record ID.
 */
function _dynamic_form_preview_maybe_load_select2($form_id) {
  $has_select2 = db_select('dynamic_form_questions', 'q')
    ->fields('q', array('id'))
    ->condition('form_id', $form_id)
    ->condition('type', array('select', 'tags'), 'IN')
    ->condition('deleted_at', NULL, 'IS NULL')
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

  $autocomplete_url = url('dynamic-form/ajax/tags/autocomplete');

  $init_js = <<<JS
(function ($) {
  // Regular dropdown (select type).
  Drupal.behaviors.dfpSelect2Preview = {
    attach: function (context, settings) {
      $('.dfp-select2', context).once('dfp-s2-init', function () {
        try {
          var isMulti = $(this).hasClass('dfp-select-multi');
          $(this).select2({
            placeholder:      isMulti ? 'Select one or more options…' : '— Select an option —',
            allowClear:       !isMulti,
            width:            '100%',
            dropdownCssClass: 'dfp-s2-preview-drop'
          });
        } catch (e) {}
      });
    }
  };

  // Tags input — Select2 v3 on a hidden input with `tags: []`.
  // Select2 v3 free-form tagging requires an <input type="hidden"> element and
  // `tags` set to an array (empty = no pre-loaded choices). Combined with AJAX
  // autocomplete + createSearchChoice this gives a proper tag-pill input field.
  Drupal.behaviors.dfpTagsPreview = {
    attach: function (context, settings) {
      $('.dfp-tags-select2', context).once('dfp-tags-s2-init', function () {
        try {
          $(this).select2({
            tags:               [],
            tokenSeparators:    [','],
            width:              '100%',
            placeholder:        'Type to search or add tags…',
            minimumInputLength: 1,
            ajax: {
              url:         '{$autocomplete_url}',
              dataType:    'json',
              quietMillis: 250,
              data: function (term, page) {
                return { term: term };
              },
              results: function (data) {
                // Legacy: plain array (no spellcheck).
                if ($.isArray(data)) { return { results: data }; }
                // Exact/prefix results — return directly.
                if (data.results && data.results.length > 0) {
                  return { results: data.results };
                }
                // Spellcheck suggestions — show as "Did you mean?" group.
                if (data.spellcheck && data.spellcheck.found && data.spellcheck.suggestions.length) {
                  return { results: [{ text: Drupal.t('Did you mean?'), children: data.spellcheck.suggestions }] };
                }
                return { results: [] };
              }
            },
            createSearchChoice: function (term, data) {
              var termLC = term.toLowerCase();
              for (var i = 0; i < data.length; i++) {
                // Handle both flat items and grouped children.
                var items = data[i].children ? data[i].children : [data[i]];
                for (var j = 0; j < items.length; j++) {
                  if (items[j].text && items[j].text.toLowerCase() === termLC) { return; }
                }
              }
              return { id: term, text: term };
            }
          });
        } catch (e) {}
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
      $input_html = '<input type="text" class="dfp-input"'
        . ' placeholder="' . t('Short answer text') . '">';
      break;

    case 'textarea':
      $input_html = '<textarea class="dfp-textarea" rows="4"'
        . ' placeholder="' . t('Long answer text') . '"></textarea>';
      break;

    case 'text_editor':
      $editor_id  = 'dfp-tinymce-' . (int) $q->id;
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
            . '<input type="radio" name="q' . (int) $q->id . '"> '
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
            . '<input type="checkbox"> '
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
        $input_html   = '<select class="' . $select_class . '"' . $multi_attr . '>';
        if (!$allow_multi) {
          $input_html .= '<option value="">' . t('— Select an option —') . '</option>';
        }
        foreach ($opts as $opt) {
          $input_html .= '<option>' . check_plain($opt->option_label) . '</option>';
        }
        $input_html .= '</select>';
        if ($allow_multi) {
          $input_html .= '<p class="dfp-multi-select-hint">'
            . '<svg xmlns="http://www.w3.org/2000/svg" width="12" height="12" viewBox="0 0 24 24" fill="none"'
            . ' stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round">'
            . '<polyline points="9 11 12 14 22 4"/>'
            . '<path d="M21 12v7a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h11"/>'
            . '</svg>'
            . ' ' . t('Multiple options can be selected') . '</p>';
        }
      }
      break;

    case 'file':
      // File type badges.
      $type_badges = '';
      if (!empty($q->allow_file_types)) {
        $types = array_filter(array_map('trim', explode(',', $q->allow_file_types)));
        foreach ($types as $ft) {
          $type_badges .= '<span class="dfp-file-type-badge">' . check_plain(drupal_strtoupper($ft)) . '</span>';
        }
      }

      // Size pill.
      $size_pill = '';
      if (!empty($q->max_file_size)) {
        $mb        = round($q->max_file_size / 1048576, 1);
        $size_pill = '<span class="dfp-file-size-pill">' . t('Max @mb MB', array('@mb' => $mb)) . '</span>';
      }

      // Max files pill.
      $files_pill = '';
      if (!empty($q->max_files)) {
        $files_pill = '<span class="dfp-file-size-pill">'
          . t('Up to @n file(s)', array('@n' => (int) $q->max_files))
          . '</span>';
      }

      $meta_html = '';
      if ($type_badges || $size_pill || $files_pill) {
        $meta_html = '<div class="dfp-file-meta">';
        if ($type_badges) {
          $meta_html .= '<div class="dfp-file-types">' . $type_badges . '</div>';
        }
        $meta_html .= $size_pill;
        $meta_html .= $files_pill;
        $meta_html .= '</div>';
      }

      $input_html = '<div class="dfp-file-dropzone">'
        . '<input type="file" class="dfp-file-hidden">'
        . '<div class="dfp-file-dropzone-inner">'
        . '<div class="dfp-file-upload-icon">'
        . '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none"'
        . ' stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">'
        . '<path d="M20.39 18.39A5 5 0 0 0 18 9h-1.26A8 8 0 1 0 3 16.3"/>'
        . '<polyline points="16 16 12 12 8 16"/>'
        . '<line x1="12" y1="12" x2="12" y2="21"/>'
        . '</svg>'
        . '</div>'
        . '<div class="dfp-file-upload-prompt">'
        . '<span class="dfp-file-upload-btn">' . t('Click to upload') . '</span>'
        . '<span class="dfp-file-upload-or"> ' . t('or drag and drop') . '</span>'
        . '</div>'
        . '<div class="dfp-file-name"></div>'
        . $meta_html
        . '</div>'
        . '</div>';
      break;

    case 'date':
      $input_html = '<input type="date" class="dfp-input">';
      break;

    case 'time':
      $input_html = '<input type="time" class="dfp-input">';
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
        $input_html .= '<button type="button" class="dfp-scale-btn">' . $i . '</button>';
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

    case 'tags':
      $input_html  = '<input type="hidden" class="dfp-tags-select2" style="width:100%">';
      $input_html .= '<p class="dfp-help-text">'
        . t('Type a tag and press Enter, or search existing tags.')
        . '</p>';
      break;

    default:
      $input_html = '<input type="text" class="dfp-input"'
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
