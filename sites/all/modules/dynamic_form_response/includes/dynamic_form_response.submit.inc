<?php

/**
 * @file
 * Public-facing form rendering, AJAX save/submit endpoints, and thank-you page.
 */

/* =====================================================================
   PUBLIC FORM RENDERING — called from dynamic_form.front.inc
   ===================================================================== */

/**
 * Returns the HTML for the fillable form body.
 *
 * Called by dynamic_form_public_form_view_page() when this module is enabled.
 * The surrounding header (title, description, meta counts) is rendered by
 * the main module; this function provides the interactive form content.
 *
 * @param object $record  A dynamic_form_forms DB row.
 * @return string         HTML to insert inside .dfb-form-view-body.
 */
function dynamic_form_response_render_form($record) {
  global $user;

  // Issue/read a persistent guest token cookie for anonymous visitors.
  $guest_token = $user->uid ? NULL : _dynamic_form_response_get_guest_token();

  // Already-submitted check for both authenticated and anonymous users.
  if ($user->uid) {
    $already = db_select('dynamic_form_responses', 'r')
      ->condition('form_id', $record->id)
      ->condition('user_id', $user->uid)
      ->condition('status', 1)
      ->isNull('deleted_at')
      ->countQuery()->execute()->fetchField();
  }
  else {
    $already_q = db_select('dynamic_form_responses', 'r');
    $already_q->join('dynamic_form_response_sessions', 's', 's.response_id = r.id');
    $already = $already_q
      ->condition('r.form_id', $record->id)
      ->condition('r.status', 1)
      ->condition('s.guest_token', $guest_token)
      ->isNull('r.deleted_at')
      ->countQuery()->execute()->fetchField();
  }

  if ($already) {
    return '<div class="dfp-form-card"><div class="dfr-already-submitted">'
      . '<div class="dfr-check-icon">&#10003;</div>'
      . '<p>' . t('You have already submitted a response to this form.') . '</p>'
      . '</div></div>';
  }

  // Draft recovery — pre-fill answers from any existing draft.
  $response_id      = 0;
  $existing_answers = array();
  if ($user->uid) {
    $draft_id = db_select('dynamic_form_responses', 'r')
      ->fields('r', array('id'))
      ->condition('form_id', $record->id)
      ->condition('user_id', $user->uid)
      ->condition('status', 0)
      ->isNull('deleted_at')
      ->execute()
      ->fetchField();
  }
  else {
    $draft_q = db_select('dynamic_form_responses', 'r');
    $draft_q->join('dynamic_form_response_sessions', 's', 's.response_id = r.id');
    $draft_id = $draft_q
      ->fields('r', array('id'))
      ->condition('r.form_id', $record->id)
      ->condition('r.status', 0)
      ->condition('s.guest_token', $guest_token)
      ->isNull('r.deleted_at')
      ->execute()
      ->fetchField();
  }

  if ($draft_id) {
    $response_id = (int) $draft_id;
    $rows = db_select('dynamic_form_answers', 'a')
      ->fields('a', array('question_id', 'value'))
      ->condition('response_id', $response_id)
      ->execute()
      ->fetchAll();
    foreach ($rows as $row) {
      $existing_answers[(int) $row->question_id] = $row->value;
    }
  }

  // Add JS and conditional third-party assets.
  $mp = drupal_get_path('module', 'dynamic_form_response');
  drupal_add_js($mp . '/js/response-form.js');
  _dynamic_form_response_maybe_load_select2($record->id);
  _dynamic_form_response_maybe_load_tinymce($record->id);
  drupal_add_css(drupal_get_path('module', 'dynamic_form') . '/css/preview.css');

  // Load sections ordered by position.
  $sections = db_select('dynamic_form_sections', 's')
    ->fields('s')
    ->condition('form_id', $record->id)
    ->isNull('deleted_at')
    ->orderBy('position', 'ASC')
    ->execute()
    ->fetchAll();

  // Load questions ordered by position.
  $questions = db_select('dynamic_form_questions', 'q')
    ->fields('q')
    ->condition('form_id', $record->id)
    ->isNull('deleted_at')
    ->orderBy('position', 'ASC')
    ->execute()
    ->fetchAll();

  if (empty($questions)) {
    return '<p class="dfr-empty">' . t('This form has no questions yet.') . '</p>';
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

  // Load scale / allow_multiple meta rules, grouped by question.
  $scale_by_q = array();
  if (!empty($question_ids)) {
    $metas = db_select('dynamic_form_question_validations', 'v')
      ->fields('v')
      ->condition('question_id', $question_ids, 'IN')
      ->condition('rule_type', array('scale_min', 'scale_max', 'scale_min_label', 'scale_max_label', 'allow_multiple'), 'IN')
      ->execute()
      ->fetchAll();
    foreach ($metas as $m) {
      $scale_by_q[(int) $m->question_id][$m->rule_type] = $m->rule_value;
    }
  }

  // Flatten questions in section order, carrying section context.
  $flat = array();
  foreach ($sections as $sec) {
    $sid = (int) $sec->id;
    $qs  = isset($q_by_section[$sid]) ? $q_by_section[$sid] : array();
    foreach ($qs as $q) {
      $flat[] = array('q' => $q, 'section' => $sec);
    }
  }
  // Unsectioned questions (section_id IS NULL).
  if (!empty($q_by_section[0])) {
    foreach ($q_by_section[0] as $q) {
      $flat[] = array('q' => $q, 'section' => NULL);
    }
  }

  if (empty($flat)) {
    return '<p class="dfr-empty">' . t('This form has no questions yet.') . '</p>';
  }

  // Divide into steps. question_per_page = 0 means "show all at once".
  $qpp   = (int) $record->question_per_page;
  $qpp   = ($qpp > 0) ? $qpp : count($flat);
  $steps = array_chunk($flat, $qpp);
  $total = count($steps);

  $token      = drupal_get_token('dfr_submit');
  $save_url   = url('dynamic-form-response/ajax/save');
  $submit_url = url('dynamic-form-response/ajax/submit');
  $upload_url = url('dynamic-form-response/ajax/upload');
  $thanks_url = url('forms/' . check_plain($record->slug) . '/thank-you');

  $col_map = array('full' => 12, 'half' => 6, 'third' => 4, 'quarter' => 3);

  $out  = '<div class="dfp-form-card">';
  $out .= '<div class="dfr-form-container"';
  $out .= ' data-form-id="'     . (int) $record->id . '"';
  $out .= ' data-token="'       . check_plain($token) . '"';
  $out .= ' data-save-url="'    . check_url($save_url) . '"';
  $out .= ' data-submit-url="'  . check_url($submit_url) . '"';
  $out .= ' data-upload-url="'  . check_url($upload_url) . '"';
  $out .= ' data-thankyou-url="'. check_url($thanks_url) . '"';
  $out .= ' data-total-steps="' . $total . '">';

  $out .= '<input type="hidden" id="dfr-response-id" value="' . (int) $response_id . '">';

  if ($total > 1) {
    $out .= '<div class="dfr-progress-wrap"><div class="dfr-progress-fill"></div></div>';
    $out .= '<div class="dfr-progress-text"></div>';
  }

  foreach ($steps as $step_idx => $step_items) {
    $step_num = $step_idx + 1;
    $is_first = ($step_num === 1);
    $is_last  = ($step_num === $total);
    $active   = $is_first ? ' dfr-step-active' : '';

    $out .= '<div class="dfr-step' . $active . '" data-step="' . $step_num . '">';

    $prev_sid = -1;
    $row_open = FALSE;
    $row_cols = 0;

    foreach ($step_items as $item) {
      $q   = $item['q'];
      $sec = $item['section'];
      $sid = $sec ? (int) $sec->id : 0;

      $width  = (!empty($q->width) && isset($col_map[$q->width])) ? $q->width : 'full';
      $q_cols = $col_map[$width];

      // Section boundary.
      if ($sid !== $prev_sid) {
        if ($row_open) {
          $out .= '</div>'; // .dfr-question-row
          $row_open = FALSE;
          $row_cols = 0;
        }
        if ($prev_sid !== -1) {
          $out .= '</div>'; // .dfr-response-section
        }
        $out .= '<div class="dfp-section">';
        if ($sec) {
          $out .= '<div class="dfp-section-header">';
          $out .= '<h2 class="dfp-section-title">' . check_plain($sec->name) . '</h2>';
          if (!empty($sec->description)) {
            $out .= '<p class="dfp-section-desc">' . check_plain($sec->description) . '</p>';
          }
          $out .= '</div>';
        }
        $prev_sid = $sid;
      }

      // Column row management.
      if (!$row_open) {
        $out .= '<div class="dfp-question-row">';
        $row_open = TRUE;
        $row_cols = 0;
      } elseif ($row_cols + $q_cols > 12) {
        $out .= '</div><div class="dfp-question-row">';
        $row_cols = 0;
      }
      $row_cols += $q_cols;

      $qid   = (int) $q->id;
      $opts  = isset($options_by_q[$qid]) ? $options_by_q[$qid] : array();
      $scale = isset($scale_by_q[$qid])   ? $scale_by_q[$qid]   : array();
      $val   = isset($existing_answers[$qid]) ? $existing_answers[$qid] : NULL;

      $out .= '<div class="dfp-question-col dfp-col-' . check_plain($width) . '">';
      $out .= _dynamic_form_response_render_question($q, $opts, $scale, $val);
      $out .= '</div>';
    }

    if ($row_open) {
      $out .= '</div>'; // .dfr-question-row
    }
    if ($prev_sid !== -1) {
      $out .= '</div>'; // .dfr-response-section
    }

    // Navigation bar.
    $out .= '<div class="dfr-step-nav">';
    if (!$is_first) {
      $out .= '<button type="button" class="dfr-btn-prev" data-step="' . ($step_num - 1) . '">'
        . t('&larr; Previous') . '</button>';
    } else {
      $out .= '<span class="dfr-nav-spacer"></span>';
    }
    if ($is_last) {
      $out .= '<button type="button" class="dfr-btn-submit">' . t('Submit') . '</button>';
    } else {
      $out .= '<button type="button" class="dfr-btn-next" data-step="' . ($step_num + 1) . '">'
        . t('Next &rarr;') . '</button>';
    }
    $out .= '</div>'; // .dfr-step-nav

    $out .= '</div>'; // .dfr-step
  }

  $out .= '</div>'; // .dfr-form-container
  $out .= '</div>'; // .dfp-form-card

  return $out;
}

/* =====================================================================
   QUESTION RENDERERS
   ===================================================================== */

/**
 * Renders a question's outer container with all accessibility / data attributes.
 */
function _dynamic_form_response_render_question($q, array $opts, array $scale, $existing_value = NULL) {
  $qid  = (int) $q->id;
  $type = $q->type;
  $req  = (int) $q->is_required;

  $req_html  = $req
    ? ' <span class="dfp-req" title="' . t('Required') . '">*</span>'
    : '';
  $desc_html = !empty($q->description)
    ? '<p class="dfp-help-text">' . check_plain($q->description) . '</p>'
    : '';

  $out  = '<div class="dfp-question"';
  $out .= ' data-question-id="' . $qid . '"';
  $out .= ' data-type="'        . check_plain($type) . '"';
  $out .= ' data-required="'    . $req . '">';
  $out .= '<div class="dfp-question-label">' . check_plain($q->label) . $req_html . '</div>';
  $out .= $desc_html;
  $out .= '<div class="dfp-question-input">';
  $out .= _dynamic_form_response_render_input($q, $opts, $scale, $existing_value);
  $out .= '</div>';
  $out .= '<div class="dfr-field-error"></div>';
  $out .= '</div>';

  return $out;
}

/**
 * Renders the interactive input element(s) for one question type.
 *
 * @param object      $q              Question DB row.
 * @param array       $opts           Option rows (radio / checkbox / select).
 * @param array       $scale          Keyed scale / allow_multiple meta values.
 * @param string|null $existing_value Stored answer to pre-fill; NULL = blank.
 * @return string HTML.
 */
function _dynamic_form_response_render_input($q, array $opts, array $scale, $existing_value = NULL) {
  $qid  = (int) $q->id;
  $type = $q->type;
  $ph   = !empty($q->placeholder) ? ' placeholder="' . check_plain($q->placeholder) . '"' : '';
  $val  = ($existing_value !== NULL) ? check_plain($existing_value) : '';

  switch ($type) {

    case 'text':
    case 'email':
    case 'number':
      $itype = ($type === 'email') ? 'email' : ($type === 'number' ? 'number' : 'text');
      return '<input type="' . $itype . '" class="dfp-input"' . $ph . ' value="' . $val . '">';

    case 'textarea':
      return '<textarea class="dfp-textarea" rows="4"' . $ph . '>' . $val . '</textarea>';

    case 'text_editor':
      $eid = 'dfr-tinymce-' . $qid;
      return '<textarea id="' . $eid . '" class="dfp-textarea dfr-tinymce-area" rows="6">' . $val . '</textarea>';

    case 'date':
      return '<input type="date" class="dfp-input" value="' . $val . '">';

    case 'time':
      return '<input type="time" class="dfp-input" value="' . $val . '">';

    case 'radio':
      if (empty($opts)) {
        return '<p class="dfp-no-options">' . t('No options defined.') . '</p>';
      }
      $html = '<div class="dfp-options-list">';
      foreach ($opts as $opt) {
        $lbl     = check_plain($opt->option_label);
        $checked = ($existing_value === $opt->option_label) ? ' checked' : '';
        $html   .= '<label class="dfp-option-label">'
          . '<input type="radio" name="dfr-radio-' . $qid . '" value="' . $lbl . '"' . $checked . '>'
          . $lbl . '</label>';
      }
      return $html . '</div>';

    case 'checkbox':
      if (empty($opts)) {
        return '<p class="dfp-no-options">' . t('No options defined.') . '</p>';
      }
      $checked_vals = array();
      if ($existing_value !== NULL) {
        $dec          = @json_decode($existing_value, TRUE);
        $checked_vals = is_array($dec) ? $dec : array();
      }
      $html = '<div class="dfp-options-list">';
      foreach ($opts as $opt) {
        $lbl     = check_plain($opt->option_label);
        $checked = in_array($opt->option_label, $checked_vals) ? ' checked' : '';
        $html   .= '<label class="dfp-option-label">'
          . '<input type="checkbox" value="' . $lbl . '"' . $checked . '>'
          . $lbl . '</label>';
      }
      return $html . '</div>';

    case 'select':
      if (empty($opts)) {
        return '<p class="dfp-no-options">' . t('No options defined.') . '</p>';
      }
      $allow_multi = !empty($scale['allow_multiple']);
      $multi_attr  = $allow_multi ? ' multiple' : '';
      $class       = $allow_multi ? 'dfp-select dfp-select-multi dfp-select2' : 'dfp-select dfp-select2';

      $sel_vals = array();
      if ($allow_multi && $existing_value !== NULL) {
        $dec      = @json_decode($existing_value, TRUE);
        $sel_vals = is_array($dec) ? $dec : array();
      } elseif (!$allow_multi && $existing_value !== NULL) {
        $sel_vals = array($existing_value);
      }

      $html = '<select class="' . $class . '"' . $multi_attr . '>';
      if (!$allow_multi) {
        $html .= '<option value="">' . t('— Select an option —') . '</option>';
      }
      foreach ($opts as $opt) {
        $lbl      = check_plain($opt->option_label);
        $selected = in_array($opt->option_label, $sel_vals) ? ' selected' : '';
        $html    .= '<option value="' . $lbl . '"' . $selected . '>' . $lbl . '</option>';
      }
      $html .= '</select>';
      if ($allow_multi) {
        $html .= '<p class="dfp-multi-select-hint">'
          . '<svg xmlns="http://www.w3.org/2000/svg" width="12" height="12" viewBox="0 0 24 24" fill="none"'
          . ' stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round">'
          . '<polyline points="9 11 12 14 22 4"/>'
          . '<path d="M21 12v7a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h11"/>'
          . '</svg>'
          . ' ' . t('Multiple options can be selected') . '</p>';
      }
      return $html;

    case 'rating':
      $min       = isset($scale['scale_min']) ? (int) $scale['scale_min'] : 1;
      $max       = isset($scale['scale_max']) ? (int) $scale['scale_max'] : 5;
      $min_label = isset($scale['scale_min_label']) ? check_plain($scale['scale_min_label']) : '';
      $max_label = isset($scale['scale_max_label']) ? check_plain($scale['scale_max_label']) : '';
      $cur       = ($existing_value !== NULL) ? (int) $existing_value : 0;

      $html = '<div class="dfp-rating-wrap">';
      if ($min_label) {
        $html .= '<span class="dfp-scale-label dfp-scale-label-min">' . $min_label . '</span>';
      }
      $html .= '<div class="dfp-stars">';
      for ($i = $min; $i <= $max; $i++) {
        $active = ($i <= $cur) ? ' dfp-star-active' : '';
        $html  .= '<span class="dfp-star' . $active . '" data-value="' . $i . '">&#9733;</span>';
      }
      $html .= '</div>';
      if ($max_label) {
        $html .= '<span class="dfp-scale-label dfp-scale-label-max">' . $max_label . '</span>';
      }
      $html .= '<input type="hidden" class="dfr-rating-value" value="' . $cur . '">';
      $html .= '</div>';
      return $html;

    case 'linear_scale':
      $min       = isset($scale['scale_min']) ? (int) $scale['scale_min'] : 1;
      $max       = isset($scale['scale_max']) ? (int) $scale['scale_max'] : 5;
      $min_label = isset($scale['scale_min_label']) ? check_plain($scale['scale_min_label']) : '';
      $max_label = isset($scale['scale_max_label']) ? check_plain($scale['scale_max_label']) : '';
      $cur       = ($existing_value !== NULL) ? (int) $existing_value : NULL;

      $html = '<div class="dfp-scale-wrap"><div class="dfp-scale-buttons">';
      for ($i = $min; $i <= $max; $i++) {
        $active = ($cur !== NULL && $cur === $i) ? ' dfp-scale-active' : '';
        $html  .= '<button type="button" class="dfp-scale-btn' . $active . '">' . $i . '</button>';
      }
      $html .= '</div>';
      if ($min_label || $max_label) {
        $html .= '<div class="dfp-scale-labels">'
          . '<span class="dfp-scale-label dfp-scale-label-min">' . $min_label . '</span>'
          . '<span class="dfp-scale-label dfp-scale-label-max">' . $max_label . '</span>'
          . '</div>';
      }
      $html .= '<input type="hidden" class="dfr-scale-value" value="' . ($cur !== NULL ? $cur : '') . '">';
      $html .= '</div>';
      return $html;

    case 'tags':
      $ac_url = check_url(url('dynamic-form/ajax/tags/autocomplete'));
      $tv     = '';
      if ($existing_value !== NULL) {
        $dec = @json_decode($existing_value, TRUE);
        if (is_array($dec)) {
          $tv = implode(',', array_map('check_plain', $dec));
        }
      }
      return '<input type="hidden" class="dfp-tags-select2"'
        . ' data-autocomplete-url="' . $ac_url . '"'
        . ' value="' . $tv . '" style="width:100%">'
        . '<p class="dfp-help-text">' . t('Type a tag and press Enter, or search existing tags.') . '</p>';

    case 'file':
      $badges = '';
      if (!empty($q->allow_file_types)) {
        foreach (array_filter(array_map('trim', explode(',', $q->allow_file_types))) as $ft) {
          $badges .= '<span class="dfp-file-type-badge">' . check_plain(drupal_strtoupper($ft)) . '</span>';
        }
      }
      $size_pill  = !empty($q->max_file_size)
        ? '<span class="dfp-file-size-pill">'
          . t('Max @mb MB', array('@mb' => round($q->max_file_size / 1048576, 1)))
          . '</span>'
        : '';
      $files_pill = !empty($q->max_files)
        ? '<span class="dfp-file-size-pill">'
          . t('Up to @n file(s)', array('@n' => (int) $q->max_files))
          . '</span>'
        : '';
      $meta = '';
      if ($badges || $size_pill || $files_pill) {
        $meta = '<div class="dfp-file-meta">'
          . ($badges ? '<div class="dfp-file-types">' . $badges . '</div>' : '')
          . $size_pill . $files_pill . '</div>';
      }

      // accept / multiple attributes for the native file picker.
      $accept_list = array();
      if (!empty($q->allow_file_types)) {
        foreach (array_filter(array_map('trim', explode(',', $q->allow_file_types))) as $ft) {
          $accept_list[] = '.' . $ft;
        }
      }
      $accept_attr = $accept_list ? ' accept="' . check_plain(implode(',', $accept_list)) . '"' : '';
      $multi_attr  = (!empty($q->max_files) && (int) $q->max_files > 1) ? ' multiple' : '';

      // Pre-fill from draft: parse previously stored file JSON.
      $file_val_attr  = '';
      $file_name_disp = '';
      if ($existing_value !== NULL && $existing_value !== '') {
        $fdec = @json_decode($existing_value, TRUE);
        if (is_array($fdec) && isset($fdec['filename'])) {
          $file_val_attr  = check_plain($existing_value);
          $file_name_disp = check_plain($fdec['filename']);
        }
      }

      $svg = '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none"'
        . ' stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">'
        . '<path d="M20.39 18.39A5 5 0 0 0 18 9h-1.26A8 8 0 1 0 3 16.3"/>'
        . '<polyline points="16 16 12 12 8 16"/><line x1="12" y1="12" x2="12" y2="21"/></svg>';
      return '<div class="dfp-file-dropzone' . ($file_name_disp ? ' dfp-file-done' : '') . '">'
        . '<input type="hidden" class="dfp-file-value" value="' . $file_val_attr . '">'
        . '<input type="file" class="dfp-file-hidden" data-question-id="' . $qid . '"' . $accept_attr . $multi_attr . '>'
        . '<div class="dfp-file-dropzone-inner">'
        . '<div class="dfp-file-upload-icon">' . $svg . '</div>'
        . '<div class="dfp-file-upload-prompt">'
        . '<span class="dfp-file-upload-btn">' . t('Click to upload') . '</span>'
        . '<span class="dfp-file-upload-or"> ' . t('or drag and drop') . '</span></div>'
        . '<div class="dfp-file-name"' . ($file_name_disp ? '' : ' style="display:none"') . '>'
        . $file_name_disp . '</div>' . $meta
        . '</div></div>';

    default:
      return '<input type="text" class="dfp-input"' . $ph . ' value="' . $val . '">';
  }
}

/* =====================================================================
   ASSET LOADERS
   ===================================================================== */

/**
 * Loads Select2 and inline initialisation JS when the form has select/tags questions.
 */
function _dynamic_form_response_maybe_load_select2($form_id) {
  $has = db_select('dynamic_form_questions', 'q')
    ->fields('q', array('id'))
    ->condition('form_id', $form_id)
    ->condition('type', array('select', 'tags'), 'IN')
    ->isNull('deleted_at')
    ->range(0, 1)
    ->execute()
    ->fetchField();

  if (!$has) {
    return;
  }

  $base = 'sites/all/libraries/select2';
  if (!file_exists(DRUPAL_ROOT . '/' . $base . '/select2.min.js')) {
    return;
  }

  drupal_add_css($base . '/select2.css');
  drupal_add_js($base . '/select2.min.js', array('weight' => 10));

  $ac_url = url('dynamic-form/ajax/tags/autocomplete');
  $js     = <<<JS
(function ($) {
  Drupal.behaviors.dfrSelect2Init = {
    attach: function (context, settings) {
      $('.dfp-select2', context).not('.dfp-tags-select2').once('dfp-s2', function () {
        if (typeof $.fn.select2 === 'undefined') { return; }
        var isMulti = $(this).hasClass('dfp-select-multi');
        $(this).select2({
          placeholder: isMulti ? 'Select options…' : '— Select —',
          allowClear: !isMulti,
          width: '100%',
          dropdownCssClass: 'dfp-s2-preview-drop'
        });
      });
      $('.dfp-tags-select2', context).once('dfp-tags-s2', function () {
        if (typeof $.fn.select2 === 'undefined') { return; }
        var acUrl = $(this).data('autocomplete-url') || '{$ac_url}';
        $(this).select2({
          tags: [], tokenSeparators: [','], width: '100%',
          placeholder: 'Type to search or add tags…', minimumInputLength: 1,
          dropdownCssClass: 'dfp-s2-preview-drop',
          ajax: {
            url: acUrl, dataType: 'json', quietMillis: 250,
            data:    function (term) { return { term: term }; },
            results: function (data) { return { results: data }; }
          },
          createSearchChoice: function (term, data) {
            for (var i = 0; i < data.length; i++) {
              if (data[i].text.toLowerCase() === term.toLowerCase()) { return; }
            }
            return { id: term, text: term };
          }
        });
      });
    }
  };
})(jQuery);
JS;
  drupal_add_js($js, array('type' => 'inline', 'scope' => 'footer', 'weight' => 20));
}

/**
 * Loads TinyMCE and inline initialisation JS when the form has text_editor questions.
 */
function _dynamic_form_response_maybe_load_tinymce($form_id) {
  $has = db_select('dynamic_form_questions', 'q')
    ->fields('q', array('id'))
    ->condition('form_id', $form_id)
    ->condition('type', 'text_editor')
    ->isNull('deleted_at')
    ->range(0, 1)
    ->execute()
    ->fetchField();

  if (!$has) {
    return;
  }

  $tinymce = 'sites/all/libraries/tinymce/js/tinymce/tinymce.min.js';
  if (!file_exists(DRUPAL_ROOT . '/' . $tinymce)) {
    return;
  }

  drupal_add_js($tinymce, array('type' => 'file', 'weight' => 10));

  $js = <<<JS
(function ($) {
  Drupal.behaviors.dfrTinymceInit = {
    attach: function (context, settings) {
      $('.dfr-tinymce-area', context).once('dfr-tinymce', function () {
        if (typeof tinymce === 'undefined') { return; }
        tinymce.init({ selector: '#' + this.id, height: 220, menubar: false, statusbar: false,
          plugins: 'lists link', toolbar: 'bold italic underline | bullist numlist | link' });
      });
    }
  };
})(jQuery);
JS;
  drupal_add_js($js, array('type' => 'inline', 'scope' => 'footer', 'weight' => 20));
}

/* =====================================================================
   AJAX ENDPOINTS
   ===================================================================== */

/**
 * AJAX callback: POST dynamic-form-response/ajax/save
 *
 * Creates or updates a draft response with the given answers.
 * Returns JSON: { status: 'ok', response_id: N } or { status: 'error', message: '...' }
 */
function dynamic_form_response_ajax_save() {
  drupal_json_output(_dynamic_form_response_do_save());
}

function _dynamic_form_response_do_save() {
  $token = isset($_POST['token']) ? $_POST['token'] : '';
  if (!drupal_valid_token($token, 'dfr_submit', TRUE)) {
    return array('status' => 'error', 'message' => t('Invalid security token.'));
  }

  $form_id     = isset($_POST['form_id'])     ? (int) $_POST['form_id']     : 0;
  $response_id = isset($_POST['response_id']) ? (int) $_POST['response_id'] : 0;
  $answers     = isset($_POST['answers'])     ? $_POST['answers']            : array();

  if (!$form_id) {
    return array('status' => 'error', 'message' => t('Missing form ID.'));
  }

  $form = db_select('dynamic_form_forms', 'f')
    ->fields('f', array('id', 'status', 'closes_at', 'opens_at', 'visibility'))
    ->condition('id', $form_id)
    ->condition('status', 1)
    ->isNull('deleted_at')
    ->execute()
    ->fetchObject();

  if (!$form) {
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
    // Verify the caller owns this draft.
    $draft = db_select('dynamic_form_responses', 'r')
      ->fields('r', array('id', 'user_id'))
      ->condition('id', $response_id)
      ->condition('form_id', $form_id)
      ->condition('status', 0)
      ->isNull('deleted_at')
      ->execute()
      ->fetchObject();

    if (!$draft) {
      return array('status' => 'error', 'message' => t('Draft not found.'));
    }

    if ($user->uid) {
      if ((int) $draft->user_id !== (int) $user->uid) {
        return array('status' => 'error', 'message' => t('Access denied.'));
      }
    } else {
      $guest_token = _dynamic_form_response_get_guest_token();
      $match = db_select('dynamic_form_response_sessions', 's')
        ->condition('response_id', $response_id)
        ->condition('guest_token', $guest_token)
        ->countQuery()->execute()->fetchField();
      if (!$match) {
        return array('status' => 'error', 'message' => t('Access denied.'));
      }
    }

    db_update('dynamic_form_responses')
      ->fields(array('updated_at' => $now))
      ->condition('id', $response_id)
      ->execute();
  } else {
    // Create a new draft response.
    $response_id = db_insert('dynamic_form_responses')
      ->fields(array(
        'form_id'    => $form_id,
        'user_id'    => (int) $user->uid,
        'status'     => 0,
        'created_at' => $now,
        'updated_at' => $now,
      ))
      ->execute();

    db_insert('dynamic_form_response_sessions')
      ->fields(array(
        'response_id' => (int) $response_id,
        'ip_address'  => ip_address(),
        'user_agent'  => isset($_SERVER['HTTP_USER_AGENT'])
          ? substr($_SERVER['HTTP_USER_AGENT'], 0, 512) : '',
        'session_id'  => hash('sha256', session_id()),
        'guest_token' => $user->uid ? NULL : _dynamic_form_response_get_guest_token(),
        'created_at'  => $now,
      ))
      ->execute();
  }

  // Fetch question types for all answers being saved so tags can be routed
  // to the dedicated tag-save function.
  $q_types = array();
  $q_ids   = array_filter(array_map('intval', array_keys($answers)));
  if (!empty($q_ids)) {
    $q_types = db_select('dynamic_form_questions', 'q')
      ->fields('q', array('id', 'type'))
      ->condition('id', $q_ids, 'IN')
      ->execute()
      ->fetchAllKeyed(0, 1);
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
    else {
      if (is_array($value)) {
        $value = json_encode(array_values(array_filter($value, 'strlen')));
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
  drupal_json_output(_dynamic_form_response_do_submit());
}

function _dynamic_form_response_do_submit() {
  $token = isset($_POST['token']) ? $_POST['token'] : '';
  if (!drupal_valid_token($token, 'dfr_submit', TRUE)) {
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
    $form = db_select('dynamic_form_forms', 'f')
      ->fields('f', array('visibility'))
      ->condition('id', $form_id)
      ->condition('status', 1)
      ->isNull('deleted_at')
      ->execute()
      ->fetchObject();
    if (!$form || $form->visibility !== 'p') {
      return array('status' => 'error', 'message' => t('You must be logged in to submit this form.'));
    }
  }

  $response = db_select('dynamic_form_responses', 'r')
    ->fields('r')
    ->condition('id', $response_id)
    ->condition('form_id', $form_id)
    ->condition('status', 0)
    ->isNull('deleted_at')
    ->execute()
    ->fetchObject();

  if (!$response) {
    return array('status' => 'error', 'message' => t('Response not found or already submitted.'));
  }

  // Ownership check.
  if ($user->uid) {
    if ((int) $response->user_id !== (int) $user->uid) {
      return array('status' => 'error', 'message' => t('Access denied.'));
    }
  } else {
    $guest_token = _dynamic_form_response_get_guest_token();
    $match = db_select('dynamic_form_response_sessions', 's')
      ->condition('response_id', $response_id)
      ->condition('guest_token', $guest_token)
      ->countQuery()->execute()->fetchField();
    if (!$match) {
      return array('status' => 'error', 'message' => t('Access denied.'));
    }
  }

  // Validate required fields.
  $required = db_select('dynamic_form_questions', 'q')
    ->fields('q', array('id', 'label'))
    ->condition('form_id', $form_id)
    ->condition('is_required', 1)
    ->isNull('deleted_at')
    ->execute()
    ->fetchAllAssoc('id');

  $answers = db_select('dynamic_form_answers', 'a')
    ->fields('a', array('question_id', 'value'))
    ->condition('response_id', $response_id)
    ->execute()
    ->fetchAllKeyed(0, 1);

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

  $now = REQUEST_TIME;
  db_update('dynamic_form_responses')
    ->fields(array(
      'status'       => 1,
      'submitted_at' => $now,
      'updated_at'   => $now,
    ))
    ->condition('id', $response_id)
    ->execute();

  _dynamic_form_audit_log(
    'response', (int) $response_id, 'submitted',
    NULL, array('form_id' => $form_id, 'submitted_at' => $now)
  );

  return array('status' => 'ok');
}

/* =====================================================================
   HELPERS
   ===================================================================== */

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

  $existing_id = db_select('dynamic_form_answers', 'a')
    ->fields('a', array('id'))
    ->condition('response_id', $response_id)
    ->condition('question_id', $question_id)
    ->execute()
    ->fetchField();

  if ($existing_id) {
    db_update('dynamic_form_answers')
      ->fields(array('value' => $value, 'updated_at' => $now))
      ->condition('id', $existing_id)
      ->execute();
  } else {
    db_insert('dynamic_form_answers')
      ->fields(array(
        'response_id' => $response_id,
        'question_id' => $question_id,
        'value'       => $value,
        'created_at'  => $now,
        'updated_at'  => $now,
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
  $mp = drupal_get_path('module', 'dynamic_form_response');
  drupal_add_css($mp . '/css/response-form.css');

  $out  = '<div class="dfr-thankyou">';
  $out .= '<div class="dfr-check-icon">&#10003;</div>';
  $out .= '<h2>' . t('Thank you for your response!') . '</h2>';
  $out .= '<p>' . t('Your response to &ldquo;@title&rdquo; has been submitted successfully.',
    array('@title' => $record->title)) . '</p>';
  $out .= l('&larr; ' . t('Back to public forms'), 'forms',
    array('html' => TRUE, 'attributes' => array('class' => array('dfr-btn-back'))));
  $out .= '</div>';

  return $out;
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
  drupal_json_output(_dynamic_form_response_do_upload());
}

function _dynamic_form_response_do_upload() {
  $token = isset($_POST['token']) ? $_POST['token'] : '';
  if (!drupal_valid_token($token, 'dfr_submit', TRUE)) {
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
  $form = db_select('dynamic_form_forms', 'f')
    ->fields('f', array('id', 'status'))
    ->condition('id', $form_id)
    ->condition('status', 1)
    ->isNull('deleted_at')
    ->execute()
    ->fetchObject();
  if (!$form) {
    return array('status' => 'error', 'message' => t('Form not found.'));
  }

  // Load question constraints.
  $question = db_select('dynamic_form_questions', 'q')
    ->fields('q', array('id', 'type', 'allow_file_types', 'max_file_size'))
    ->condition('id', $question_id)
    ->condition('form_id', $form_id)
    ->condition('type', 'file')
    ->isNull('deleted_at')
    ->execute()
    ->fetchObject();
  if (!$question) {
    return array('status' => 'error', 'message' => t('File question not found.'));
  }

  $file_tmp  = $_FILES['file']['tmp_name'];
  $file_name = $_FILES['file']['name'];
  $file_size = $_FILES['file']['size'];

  // Validate file extension.
  if (!empty($question->allow_file_types)) {
    $allowed = array_filter(array_map('strtolower', array_map('trim', explode(',', $question->allow_file_types))));
    $ext = strtolower(pathinfo($file_name, PATHINFO_EXTENSION));
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
  $safe_name = file_munge_filename(drupal_basename($file_name), '');
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
