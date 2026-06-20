<?php


/**
 * AJAX page callback: Search users by name or e-mail.
 *
 * Route: dynamic-form/ajax/members/search?q=<query>
 */
function dynamic_form_ajax_member_search() {
  global $user;
  $uid = (int) $user->uid;
  if (!flood_is_allowed('dfb_member_search', 60, 3600, $uid)) {
    drupal_add_http_header('Status', '429 Too Many Requests');
    drupal_json_output(array());
    drupal_exit();
  }
  flood_register_event('dfb_member_search', 3600, $uid);

  $q = isset($_GET['s']) ? trim($_GET['s']) : '';

  if (drupal_strlen($q) < 2) {
    drupal_json_output(array());
    return;
  }

  $like = '%' . db_like($q) . '%';

  $rows = db_select('users', 'u')
    ->fields('u', array('uid', 'name', 'mail'))
    ->condition('u.status', 1)
    ->condition('u.uid', 0, '>')
    ->where('u.name LIKE :like OR u.mail LIKE :like', array(':like' => $like))
    ->orderBy('u.name')
    ->range(0, 10)
    ->execute()
    ->fetchAll();

  $results = array();
  foreach ($rows as $row) {
    $results[] = array(
      'uid'  => (int) $row->uid,
      'name' => $row->name,
      'mail' => $row->mail,
    );
  }

  drupal_json_output($results);
}

/* =====================================================================
   HELPERS — LOAD / SAVE / NOTIFY
   ===================================================================== */

/**
 * Loads existing active members for a form as a JS-ready array.
 *
 * @param int $form_id
 *
 * @return array  [{uid, name, role}, …]
 */
function _dynamic_form_load_existing_members($form_id) {
  $q = db_select('dynamic_form_members', 'm');
  $q->join('users', 'u', 'u.uid = m.user_id');
  $q->fields('m', array('user_id', 'role'));
  $q->addField('u', 'name');
  $q->condition('m.form_id', $form_id);
  $q->condition('m.deleted_at', NULL, 'IS NULL');
  $q->orderBy('m.created_at');

  $members = array();
  foreach ($q->execute()->fetchAll() as $row) {
    $members[] = array(
      'uid'  => (int) $row->user_id,
      'name' => $row->name,
      'role' => $row->role,
    );
  }
  return $members;
}

/**
 * Persists form members from a submitted JSON string.
 */
function _dynamic_form_save_members($form_id, $members_json, $is_new = TRUE) {
  global $user;
  $now = REQUEST_TIME;

  $submitted = array();
  if (!empty($members_json)) {
    $decoded = json_decode($members_json, TRUE);
    if (is_array($decoded)) {
      $submitted = $decoded;
    }
  }

  $valid_roles   = array('editor', 'form_applicant');
  $submitted_ids = array();
  $newly_added   = array();

  foreach ($submitted as $m) {
    $uid  = isset($m['uid'])  ? (int) $m['uid']  : 0;
    $role = isset($m['role']) ? $m['role']        : '';

    if (!$uid || !in_array($role, $valid_roles)) {
      continue;
    }
    $submitted_ids[] = $uid;

    $existing = db_select('dynamic_form_members', 'm')
      ->fields('m', array('id', 'role', 'deleted_at'))
      ->condition('form_id', $form_id)
      ->condition('user_id', $uid)
      ->execute()
      ->fetchObject();

    if ($existing) {
      $was_deleted  = !is_null($existing->deleted_at);
      $role_changed = ($existing->role !== $role);

      if ($was_deleted || $role_changed) {
        db_update('dynamic_form_members')
          ->fields(array(
            'role'       => $role,
            'deleted_at' => NULL,
            'deleted_by' => NULL,
            'updated_at' => $now,
          ))
          ->condition('id', $existing->id)
          ->execute();

        if ($was_deleted) {
          $newly_added[] = array('user_id' => $uid, 'role' => $role);
        }
      }
    }
    else {
      db_insert('dynamic_form_members')
        ->fields(array(
          'form_id'    => $form_id,
          'user_id'    => $uid,
          'role'       => $role,
          'created_by' => $user->uid,
          'created_at' => $now,
          'updated_at' => $now,
        ))
        ->execute();
      $newly_added[] = array('user_id' => $uid, 'role' => $role);
    }
  }

  // On edit: soft-delete members that were removed from the submitted list.
  if (!$is_new) {
    $del_q = db_select('dynamic_form_members', 'm')
      ->fields('m', array('id'))
      ->condition('form_id', $form_id)
      ->condition('deleted_at', NULL, 'IS NULL');

    if (!empty($submitted_ids)) {
      $del_q->condition('user_id', $submitted_ids, 'NOT IN');
    }

    foreach ($del_q->execute()->fetchAll() as $em) {
      db_update('dynamic_form_members')
        ->fields(array(
          'deleted_at' => $now,
          'deleted_by' => $user->uid,
          'updated_at' => $now,
        ))
        ->condition('id', $em->id)
        ->execute();
    }
  }

  return $newly_added;
}

/**
 * Queues email notifications for newly-added form members and sends them immediately.
 */
function _dynamic_form_queue_member_notifications($form_id, $form_title, array $newly_added) {
  if (empty($newly_added)) {
    return;
  }

  $queue    = DrupalQueue::get('dynamic_form_member_notify');
  $form_url = url('dashboard/forms/' . $form_id . '/builder', array('absolute' => TRUE));

  foreach ($newly_added as $m) {
    $queue->createItem(array(
      'form_id'    => $form_id,
      'form_title' => $form_title,
      'user_id'    => $m['user_id'],
      'role'       => $m['role'],
      'form_url'   => $form_url,
    ));
    $claimed = $queue->claimItem(60);
    if ($claimed) {
      _dynamic_form_send_member_notification($claimed->data);
      $queue->deleteItem($claimed);
    }
  }
}

/**
 * Sends a member-added notification email. Called by the queue worker.
 */
function _dynamic_form_send_member_notification(array $item) {
  $account = user_load($item['user_id']);
  if (!$account || empty($account->mail)) {
    return;
  }

  $role_labels = array(
    'editor'         => t('Editor'),
    'form_applicant' => t('Form Applicant'),
  );
  $role_label = isset($role_labels[$item['role']]) ? $role_labels[$item['role']] : $item['role'];

  drupal_mail(
    'dynamic_form',
    'member_added',
    $account->mail,
    user_preferred_language($account),
    array(
      'form_title' => $item['form_title'],
      'user_name'  => $account->name,
      'role_label' => $role_label,
      'form_url'   => $item['form_url'],
    )
  );
}
