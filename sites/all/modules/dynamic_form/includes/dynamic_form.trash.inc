<?php

/**
 * @file
 * Trash management: view soft-deleted items, restore, and permanently delete.
 */

/* =====================================================================
   TRASH — PAGE CALLBACK
   ===================================================================== */

/**
 * Page callback for /dashboard/trash.
 */
function dynamic_form_dashboard_trash_page() {
  drupal_set_title(t('Trash'));

  module_load_include('inc', 'dynamic_form', 'includes/dynamic_form.dashboard');

  $module_path = drupal_get_path('module', 'dynamic_form');
  drupal_add_css($module_path . '/css/dashboard-delete.css');
  drupal_add_js($module_path . '/js/dashboard-delete.js');

  $sidebar_html = _dynamic_form_dashboard_sidebar('trash');
  $content_html = _dynamic_form_dashboard_trash_content();
  $content_html .= _dynamic_form_restore_confirm_modal();

  return theme('dynamic_form_dashboard', array(
    'sidebar' => $sidebar_html,
    'content' => $content_html,
  ));
}

/* =====================================================================
   TRASH — CONTENT RENDERER
   ===================================================================== */

/**
 * Renders all three trash sections: deleted forms, sections, and questions.
 */
function _dynamic_form_dashboard_trash_content() {
  global $user;
  $is_admin   = user_access('edit any form');
  $can_act    = user_access('restore deleted');

  $output  = '<div class="dfb-dashboard-header">';
  $output .= '<h2>' . t('Trash') . '</h2>';
  $output .= '<p class="dfb-dashboard-subtitle">' . t('Soft-deleted forms, sections, and questions. Restore them or remove permanently.') . '</p>';
  $output .= '</div>';

  // ---- Deleted forms ----
  $fq = db_select('dynamic_form_forms', 'f')
    ->fields('f', array('id', 'title', 'status', 'deleted_at'))
    ->condition('f.deleted_at', NULL, 'IS NOT NULL')
    ->orderBy('f.deleted_at', 'DESC');
  if (!$is_admin) {
    $fq->condition('f.created_by', $user->uid);
  }
  $deleted_forms = $fq->execute()->fetchAll();

  $output .= _dynamic_form_trash_section_block(
    t('Forms'), '&#128196;', count($deleted_forms),
    array(t('Title'), t('Status'), t('Deleted On'), t('Actions')),
    $deleted_forms, 'form', $can_act
  );

  // ---- Deleted sections ----
  $sq = db_select('dynamic_form_sections', 's');
  $sq->leftJoin('dynamic_form_forms', 'f', 's.form_id = f.id');
  $sq->fields('s', array('id', 'name', 'deleted_at'));
  $sq->addField('f', 'title', 'form_title');
  $sq->condition('s.deleted_at', NULL, 'IS NOT NULL')
    ->orderBy('s.deleted_at', 'DESC');
  if (!$is_admin) {
    $sq->condition('s.created_by', $user->uid);
  }
  $deleted_sections = $sq->execute()->fetchAll();

  $output .= _dynamic_form_trash_section_block(
    t('Sections'), '&#128193;', count($deleted_sections),
    array(t('Section Name'), t('Parent Form'), t('Deleted On'), t('Actions')),
    $deleted_sections, 'section', $can_act
  );

  // ---- Deleted questions ----
  $qq = db_select('dynamic_form_questions', 'q');
  $qq->leftJoin('dynamic_form_forms', 'f', 'q.form_id = f.id');
  $qq->fields('q', array('id', 'label', 'type', 'deleted_at'));
  $qq->addField('f', 'title', 'form_title');
  $qq->condition('q.deleted_at', NULL, 'IS NOT NULL')
    ->orderBy('q.deleted_at', 'DESC');
  if (!$is_admin) {
    $qq->condition('q.created_by', $user->uid);
  }
  $deleted_questions = $qq->execute()->fetchAll();

  $output .= _dynamic_form_trash_section_block(
    t('Questions'), '&#10067;', count($deleted_questions),
    array(t('Question'), t('Type'), t('Parent Form'), t('Deleted On'), t('Actions')),
    $deleted_questions, 'question', $can_act
  );

  return $output;
}

/**
 * Builds a single entity-type block (title bar + table or empty notice).
 */
function _dynamic_form_trash_section_block($title, $icon, $count, $header, $records, $entity_type, $can_act) {
  $out  = '<div class="dfb-trash-section">';
  $out .= '<h3 class="dfb-trash-section-title">';
  $out .= '<span class="dfb-trash-section-icon">' . $icon . '</span>';
  $out .= check_plain($title);
  $out .= ' <span class="dfb-trash-count">(' . (int) $count . ')</span>';
  $out .= '</h3>';

  if (!empty($records)) {
    $rows = array();
    foreach ($records as $record) {
      $rows[] = _dynamic_form_trash_build_row($record, $entity_type, $can_act);
    }
    $out .= theme('table', array(
      'header'     => $header,
      'rows'       => $rows,
      'attributes' => array('class' => array('dfb-dashboard-table', 'dfb-trash-table')),
    ));
  }
  else {
    $out .= '<div class="dfb-trash-empty"><p>' . t('No deleted items.') . '</p></div>';
  }

  $out .= '</div>';
  return $out;
}

/**
 * Builds one table row for a deleted entity.
 */
function _dynamic_form_trash_build_row($record, $entity_type, $can_act) {
  switch ($entity_type) {
    case 'form':     $entity_name = $record->title; break;
    case 'section':  $entity_name = $record->name;  break;
    case 'question': $entity_name = truncate_utf8($record->label, 60, TRUE, TRUE); break;
    default:         $entity_name = '';
  }

  $actions = array();
  if ($can_act) {
    $actions[] = l(
      t('Restore'),
      'dashboard/trash/' . $entity_type . '/' . $record->id . '/restore',
      array('attributes' => array(
        'class'            => array('dfb-action-restore', 'dfb-restore-trigger'),
        'data-entity-type' => $entity_type,
        'data-entity-id'   => (string) $record->id,
        'data-entity-name' => $entity_name,
      ))
    );
    $actions[] = l(
      t('Delete Permanently'),
      'dashboard/trash/' . $entity_type . '/' . $record->id . '/delete-permanent',
      array('attributes' => array('class' => array('dfb-action-delete')))
    );
  }
  $actions_html = implode(' &middot; ', $actions);
  $deleted_date = format_date($record->deleted_at, 'short');

  switch ($entity_type) {
    case 'form':
      $status_labels = array(
        0 => '<span class="dfb-badge dfb-badge-draft">' . t('Draft') . '</span>',
        1 => '<span class="dfb-badge dfb-badge-published">' . t('Published') . '</span>',
        2 => '<span class="dfb-badge dfb-badge-closed">' . t('Closed') . '</span>',
      );
      return array(
        check_plain($record->title),
        array(
          'data'  => isset($status_labels[$record->status]) ? $status_labels[$record->status] : t('Unknown'),
          'class' => array('status-cell'),
        ),
        $deleted_date,
        $actions_html,
      );

    case 'section':
      $parent = !empty($record->form_title)
        ? check_plain($record->form_title)
        : '<em class="dfb-text-muted">' . t('(form deleted)') . '</em>';
      return array(check_plain($record->name), $parent, $deleted_date, $actions_html);

    case 'question':
      $parent = !empty($record->form_title)
        ? check_plain($record->form_title)
        : '<em class="dfb-text-muted">' . t('(form deleted)') . '</em>';
      return array(
        check_plain(truncate_utf8($record->label, 60, TRUE, TRUE)),
        '<code class="dfb-type-badge">' . check_plain($record->type) . '</code>',
        $parent,
        $deleted_date,
        $actions_html,
      );
  }
  return array();
}

/* =====================================================================
   TRASH — RESTORE CONFIRM
   ===================================================================== */

/**
 * Form builder: confirm restoration of a soft-deleted item.
 *
 * @param string $entity_type  One of: form, section, question.
 * @param int    $entity_id    Primary key of the entity.
 */
function dynamic_form_trash_restore_confirm($form, &$form_state, $entity_type, $entity_id) {
  if (!in_array($entity_type, array('form', 'section', 'question'))) {
    drupal_not_found();
    drupal_exit();
  }

  $record = _dynamic_form_trash_load_entity($entity_type, $entity_id);
  if (!$record) {
    drupal_set_message(t('Item not found in trash.'), 'error');
    drupal_goto('dashboard/trash');
  }

  $label = _dynamic_form_trash_label($entity_type, $record);

  $form_state['trash_entity_type'] = $entity_type;
  $form_state['trash_entity_id']   = (int) $entity_id;
  $form_state['trash_record']      = $record;

  return confirm_form(
    $form,
    t('Restore "@label"?', array('@label' => $label)),
    'dashboard/trash',
    t('This @type will be moved out of the trash and become active again.', array('@type' => $entity_type)),
    t('Restore'),
    t('Cancel')
  );
}

/**
 * Submit: clear deleted_at/deleted_by to restore the entity.
 */
function dynamic_form_trash_restore_confirm_submit($form, &$form_state) {
  $entity_type = $form_state['trash_entity_type'];
  $entity_id   = $form_state['trash_entity_id'];
  $record      = $form_state['trash_record'];
  $table       = _dynamic_form_trash_table($entity_type);

  db_update($table)
    ->fields(array(
      'deleted_at' => NULL,
      'deleted_by' => NULL,
      'updated_at' => REQUEST_TIME,
    ))
    ->condition('id', $entity_id)
    ->execute();

  drupal_set_message(t('"@label" has been restored.', array('@label' => _dynamic_form_trash_label($entity_type, $record))));
  $form_state['redirect'] = 'dashboard/trash';
}

/* =====================================================================
   TRASH — PERMANENT DELETE CONFIRM
   ===================================================================== */

/**
 * Form builder: confirm permanent deletion of a soft-deleted item.
 */
function dynamic_form_trash_delete_permanent_confirm($form, &$form_state, $entity_type, $entity_id) {
  if (!in_array($entity_type, array('form', 'section', 'question'))) {
    drupal_not_found();
    drupal_exit();
  }

  $record = _dynamic_form_trash_load_entity($entity_type, $entity_id);
  if (!$record) {
    drupal_set_message(t('Item not found in trash.'), 'error');
    drupal_goto('dashboard/trash');
  }

  $label = _dynamic_form_trash_label($entity_type, $record);

  $form_state['trash_entity_type'] = $entity_type;
  $form_state['trash_entity_id']   = (int) $entity_id;
  $form_state['trash_record']      = $record;

  return confirm_form(
    $form,
    t('Permanently delete "@label"?', array('@label' => $label)),
    'dashboard/trash',
    t('This cannot be undone. All data for this @type — including any associated questions, answers, and files — will be permanently removed from the database.', array('@type' => $entity_type)),
    t('Delete Permanently'),
    t('Cancel')
  );
}

/**
 * Submit: cascade-delete the entity and all dependent records.
 */
function dynamic_form_trash_delete_permanent_confirm_submit($form, &$form_state) {
  $entity_type = $form_state['trash_entity_type'];
  $entity_id   = $form_state['trash_entity_id'];
  $record      = $form_state['trash_record'];

  switch ($entity_type) {
    case 'form':
      _dynamic_form_trash_hard_delete_form($entity_id);
      break;
    case 'section':
      _dynamic_form_trash_hard_delete_section($entity_id);
      break;
    case 'question':
      _dynamic_form_trash_hard_delete_question($entity_id);
      break;
  }

  drupal_set_message(t('"@label" has been permanently deleted.', array('@label' => _dynamic_form_trash_label($entity_type, $record))));
  $form_state['redirect'] = 'dashboard/trash';
}

/* =====================================================================
   TRASH — HARD-DELETE HELPERS
   ===================================================================== */

/**
 * Permanently deletes a form and all cascading data.
 */
function _dynamic_form_trash_hard_delete_form($form_id) {
  $question_ids = db_select('dynamic_form_questions', 'q')
    ->fields('q', array('id'))
    ->condition('form_id', $form_id)
    ->execute()->fetchCol();

  $response_ids = db_select('dynamic_form_responses', 'r')
    ->fields('r', array('id'))
    ->condition('form_id', $form_id)
    ->execute()->fetchCol();

  if (!empty($question_ids)) {
    $answer_ids = db_select('dynamic_form_answers', 'a')
      ->fields('a', array('id'))
      ->condition('question_id', $question_ids, 'IN')
      ->execute()->fetchCol();
    if (!empty($answer_ids)) {
      db_delete('dynamic_form_answer_files')->condition('answer_id', $answer_ids, 'IN')->execute();
    }
    db_delete('dynamic_form_question_options')->condition('question_id', $question_ids, 'IN')->execute();
    db_delete('dynamic_form_question_validations')->condition('question_id', $question_ids, 'IN')->execute();
  }

  if (!empty($response_ids)) {
    db_delete('dynamic_form_answers')->condition('response_id', $response_ids, 'IN')->execute();
    db_delete('dynamic_form_response_sessions')->condition('response_id', $response_ids, 'IN')->execute();
  }

  db_delete('dynamic_form_responses')->condition('form_id', $form_id)->execute();
  db_delete('dynamic_form_questions')->condition('form_id', $form_id)->execute();
  db_delete('dynamic_form_sections')->condition('form_id', $form_id)->execute();
  db_delete('dynamic_form_members')->condition('form_id', $form_id)->execute();
  db_delete('dynamic_form_forms')->condition('id', $form_id)->execute();
}

/**
 * Permanently deletes a section and all its questions.
 */
function _dynamic_form_trash_hard_delete_section($section_id) {
  $question_ids = db_select('dynamic_form_questions', 'q')
    ->fields('q', array('id'))
    ->condition('section_id', $section_id)
    ->execute()->fetchCol();

  if (!empty($question_ids)) {
    $answer_ids = db_select('dynamic_form_answers', 'a')
      ->fields('a', array('id'))
      ->condition('question_id', $question_ids, 'IN')
      ->execute()->fetchCol();
    if (!empty($answer_ids)) {
      db_delete('dynamic_form_answer_files')->condition('answer_id', $answer_ids, 'IN')->execute();
      db_delete('dynamic_form_answers')->condition('id', $answer_ids, 'IN')->execute();
    }
    db_delete('dynamic_form_question_options')->condition('question_id', $question_ids, 'IN')->execute();
    db_delete('dynamic_form_question_validations')->condition('question_id', $question_ids, 'IN')->execute();
    db_delete('dynamic_form_questions')->condition('id', $question_ids, 'IN')->execute();
  }

  db_delete('dynamic_form_sections')->condition('id', $section_id)->execute();
}

/**
 * Permanently deletes a question and its options, validations, and answers.
 */
function _dynamic_form_trash_hard_delete_question($question_id) {
  $answer_ids = db_select('dynamic_form_answers', 'a')
    ->fields('a', array('id'))
    ->condition('question_id', $question_id)
    ->execute()->fetchCol();

  if (!empty($answer_ids)) {
    db_delete('dynamic_form_answer_files')->condition('answer_id', $answer_ids, 'IN')->execute();
    db_delete('dynamic_form_answers')->condition('id', $answer_ids, 'IN')->execute();
  }

  db_delete('dynamic_form_question_options')->condition('question_id', $question_id)->execute();
  db_delete('dynamic_form_question_validations')->condition('question_id', $question_id)->execute();
  db_delete('dynamic_form_questions')->condition('id', $question_id)->execute();
}

/* =====================================================================
   TRASH — SHARED HELPERS
   ===================================================================== */

/**
 * Maps entity type to its database table name.
 */
function _dynamic_form_trash_table($entity_type) {
  $map = array(
    'form'     => 'dynamic_form_forms',
    'section'  => 'dynamic_form_sections',
    'question' => 'dynamic_form_questions',
  );
  return isset($map[$entity_type]) ? $map[$entity_type] : NULL;
}

/**
 * Loads a soft-deleted entity (deleted_at IS NOT NULL).
 *
 * Returns FALSE if the entity does not exist or is not in the trash.
 */
function _dynamic_form_trash_load_entity($entity_type, $entity_id) {
  $table = _dynamic_form_trash_table($entity_type);
  if (!$table) {
    return FALSE;
  }
  return db_select($table, 'e')
    ->fields('e')
    ->condition('id', (int) $entity_id)
    ->condition('deleted_at', NULL, 'IS NOT NULL')
    ->execute()
    ->fetchObject();
}

/**
 * Returns a sanitised human-readable label for an entity record.
 */
function _dynamic_form_trash_label($entity_type, $record) {
  switch ($entity_type) {
    case 'form':
      return check_plain($record->title);
    case 'section':
      return check_plain($record->name);
    case 'question':
      return check_plain(truncate_utf8($record->label, 60, TRUE, TRUE));
  }
  return t('Item');
}

/**
 * Returns the HTML for the restore-confirmation modal on the trash page.
 */
function _dynamic_form_restore_confirm_modal() {
  return '
<div id="dfb-restore-modal" role="alertdialog" aria-modal="true" aria-labelledby="dfb-restore-modal-title" style="display:none;">
  <div class="dfb-delete-modal-dialog">
    <span class="dfb-delete-modal-icon">&#9850;</span>
    <h3 class="dfb-delete-modal-title" id="dfb-restore-modal-title">'
      . t('Restore this') . ' <span class="dfb-restore-modal-type"></span>?
    </h3>
    <p class="dfb-delete-modal-body">'
      . t('You are about to restore') . ' <strong class="dfb-restore-modal-name"></strong>. '
      . t('The item will become active again.') . '
    </p>
    <div class="dfb-delete-modal-actions">
      <button class="dfb-restore-confirm-btn" type="button">' . t('Restore') . '</button>
      <button class="dfb-restore-cancel-btn"  type="button">' . t('Cancel') . '</button>
    </div>
  </div>
</div>';
}

/* =====================================================================
   TRASH — AJAX RESTORE
   ===================================================================== */

/**
 * AJAX endpoint: restores a soft-deleted entity.
 *
 * Route: POST dynamic-form/ajax/trash/{entity_type}/{entity_id}/restore
 * Returns JSON: {status: "ok"|"error", message: "..."}
 */
function dynamic_form_ajax_entity_restore($entity_type, $entity_id) {
  if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    drupal_json_output(array('status' => 'error', 'message' => t('Invalid request method.')));
    drupal_exit();
  }

  if (!in_array($entity_type, array('form', 'section', 'question'))) {
    drupal_json_output(array('status' => 'error', 'message' => t('Invalid entity type.')));
    drupal_exit();
  }

  $entity_id = (int) $entity_id;
  $table     = _dynamic_form_trash_table($entity_type);

  $record = db_select($table, 'e')
    ->fields('e')
    ->condition('id', $entity_id)
    ->condition('deleted_at', NULL, 'IS NOT NULL')
    ->execute()
    ->fetchObject();

  if (!$record) {
    drupal_json_output(array('status' => 'error', 'message' => t('Item not found in trash.')));
    drupal_exit();
  }

  db_update($table)
    ->fields(array(
      'deleted_at' => NULL,
      'deleted_by' => NULL,
      'updated_at' => REQUEST_TIME,
    ))
    ->condition('id', $entity_id)
    ->execute();

  $label = _dynamic_form_trash_label($entity_type, $record);

  _dynamic_form_audit_log($entity_type, $entity_id, 'created', NULL, (array) $record);

  drupal_json_output(array(
    'status'  => 'ok',
    'message' => t('"@label" has been restored.', array('@label' => $label)),
  ));
  drupal_exit();
}
