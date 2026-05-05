<?php

/**
 * @file
 * Invitation system for the Dynamic Form module.
 *
 * Handles send, accept, resend, revoke, and the accept landing page.
 * Registered users are added directly to dynamic_form_members.
 * Unregistered emails receive a token-based invite link (7-day expiry).
 */

/* =====================================================================
   PAGE CALLBACK — ACCEPT INVITATION
   ===================================================================== */

/**
 * Page callback: Accept an invitation via its token.
 *
 * Route: dynamic-form/invite/accept/TOKEN
 * Access: public (token is the secret).
 */
function dynamic_form_invite_accept_page($token) {
  global $user;

  $invitation = _dynamic_form_load_invitation_by_token($token);

  if (!$invitation) {
    drupal_set_message(t('Invalid invitation link.'), 'error');
    drupal_goto('<front>');
  }

  // Terminal states — inform and bail.
  if ($invitation->status === 'accepted') {
    drupal_set_message(t('This invitation has already been accepted.'), 'warning');
    drupal_goto('<front>');
  }

  if ($invitation->status === 'revoked') {
    drupal_set_message(t('This invitation has been revoked by the form owner.'), 'error');
    drupal_goto('<front>');
  }

  // Expire on the fly if the DB row hasn't been updated yet.
  if ($invitation->status === 'expired' || $invitation->expires_at < REQUEST_TIME) {
    if ($invitation->status !== 'expired') {
      db_update('dynamic_form_invitations')
        ->fields(array('status' => 'expired'))
        ->condition('id', $invitation->id)
        ->execute();
    }
    drupal_set_message(t('This invitation link has expired. Please ask the form owner to send a new one.'), 'error');
    drupal_goto('<front>');
  }

  // If not logged in: store token in session and send to login.
  if (empty($user->uid)) {
    $_SESSION['dfb_invite_token'] = $token;
    drupal_set_message(t('Please sign in or create an account to accept your invitation.'));
    drupal_goto('login', array('query' => array('destination' => 'dynamic-form/invite/accept/' . rawurlencode($token))));
  }

  // Verify the logged-in user's email matches the invitation.
  $user_email   = drupal_strtolower(trim($user->mail));
  $invite_email = drupal_strtolower(trim($invitation->email));

  if ($user_email !== $invite_email) {
    $masked = _dynamic_form_mask_email($invitation->email);
    drupal_set_message(t(
      'This invitation was sent to @email. Please sign in with that account to accept it.',
      array('@email' => $masked)
    ), 'error');
    drupal_goto('<front>');
  }

  // Verify the target form still exists and is not deleted.
  $form_record = db_select('dynamic_form_forms', 'f')
    ->fields('f', array('id', 'title'))
    ->condition('id', $invitation->form_id)
    ->condition('deleted_at', NULL, 'IS NULL')
    ->execute()
    ->fetchObject();

  if (!$form_record) {
    drupal_set_message(t('The form you were invited to no longer exists.'), 'error');
    drupal_goto('<front>');
  }

  $result = _dynamic_form_accept_invitation($invitation, $user->uid);

  if ($result === 'already_member') {
    drupal_set_message(t('You are already a member of "@title".', array('@title' => $form_record->title)), 'warning');
    drupal_goto('dashboard/forms/' . $invitation->form_id . '/builder');
  }

  if ($result === TRUE) {
    drupal_set_message(t(
      'You have joined "@title" as @role.',
      array('@title' => $form_record->title, '@role' => _dynamic_form_role_label($invitation->role))
    ));
    drupal_goto('dashboard/forms/' . $invitation->form_id . '/builder');
  }

  drupal_set_message(t('There was a problem accepting the invitation. Please try again.'), 'error');
  drupal_goto('<front>');
}

/* =====================================================================
   AJAX — SEND INVITATION
   ===================================================================== */

/**
 * AJAX page callback: Send an invitation by email.
 *
 * POST params: form_id, email, role
 */
function dynamic_form_ajax_invitation_send() {
  global $user;

  $form_id = isset($_POST['form_id']) ? (int) $_POST['form_id'] : 0;
  $email   = isset($_POST['email'])   ? trim($_POST['email'])   : '';
  $role    = isset($_POST['role'])    ? trim($_POST['role'])    : '';

  if (!$form_id || $email === '' || $role === '') {
    drupal_json_output(array('status' => 'error', 'message' => t('Missing required fields.')));
    return;
  }

  if (!_dynamic_form_edit_access($form_id)) {
    drupal_json_output(array('status' => 'error', 'message' => t('Access denied.')));
    return;
  }

  if (!valid_email_address($email)) {
    drupal_json_output(array('status' => 'error', 'message' => t('Please enter a valid email address.')));
    return;
  }

  $valid_roles = array('editor', 'form_applicant', 'viewer', 'reviewer');
  if (!in_array($role, $valid_roles)) {
    drupal_json_output(array('status' => 'error', 'message' => t('Invalid role.')));
    return;
  }

  $email_norm = drupal_strtolower($email);

  if (drupal_strtolower($user->mail) === $email_norm) {
    drupal_json_output(array('status' => 'error', 'message' => t('You cannot invite yourself.')));
    return;
  }

  // Verify the form exists and is not deleted.
  $form_record = db_select('dynamic_form_forms', 'f')
    ->fields('f', array('id', 'title'))
    ->condition('id', $form_id)
    ->condition('deleted_at', NULL, 'IS NULL')
    ->execute()
    ->fetchObject();

  if (!$form_record) {
    drupal_json_output(array('status' => 'error', 'message' => t('Form not found.')));
    return;
  }

  // Case-insensitive lookup for an existing account.
  $existing_uid = db_select('users', 'u')
    ->fields('u', array('uid'))
    ->where('LOWER(u.mail) = :email', array(':email' => $email_norm))
    ->condition('u.status', 1)
    ->condition('u.uid', 0, '>')
    ->execute()
    ->fetchField();

  if ($existing_uid) {
    return _dynamic_form_invite_existing_user((int) $existing_uid, $form_id, $form_record->title, $role, $email);
  }

  return _dynamic_form_invite_new_email($email_norm, $form_id, $form_record->title, $role);
}

/**
 * Adds an existing registered user directly as a form member.
 */
function _dynamic_form_invite_existing_user($uid, $form_id, $form_title, $role, $display_email) {
  global $user;
  $now = REQUEST_TIME;

  $account = user_load($uid);
  if (!$account) {
    drupal_json_output(array('status' => 'error', 'message' => t('User account not found.')));
    return;
  }

  $existing = db_select('dynamic_form_members', 'm')
    ->fields('m', array('id', 'role', 'deleted_at'))
    ->condition('form_id', $form_id)
    ->condition('user_id', $uid)
    ->execute()
    ->fetchObject();

  if ($existing && is_null($existing->deleted_at)) {
    drupal_json_output(array(
      'status'  => 'error',
      'message' => t('@email is already a member of this form.', array('@email' => $display_email)),
    ));
    return;
  }

  if ($existing) {
    // Restore a previously removed member.
    db_update('dynamic_form_members')
      ->fields(array(
        'role'       => $role,
        'deleted_at' => NULL,
        'deleted_by' => NULL,
        'updated_at' => $now,
      ))
      ->condition('id', $existing->id)
      ->execute();
    $member_id = (int) $existing->id;
  }
  else {
    $member_id = (int) db_insert('dynamic_form_members')
      ->fields(array(
        'form_id'    => $form_id,
        'user_id'    => $uid,
        'role'       => $role,
        'created_by' => $user->uid,
        'created_at' => $now,
        'updated_at' => $now,
      ))
      ->execute();
  }

  _dynamic_form_audit_log('member', $member_id, 'created', NULL, array(
    'form_id' => $form_id,
    'user_id' => $uid,
    'role'    => $role,
  ));

  // Re-use existing member notification queue.
  module_load_include('inc', 'dynamic_form', 'includes/dynamic_form.members');
  _dynamic_form_queue_member_notifications($form_id, $form_title, array(
    array('user_id' => $uid, 'role' => $role),
  ));

  drupal_json_output(array(
    'status'  => 'added',
    'message' => t('@name has been added as @role.', array(
      '@name' => $account->name,
      '@role' => _dynamic_form_role_label($role),
    )),
  ));
}

/**
 * Creates a new invitation record for an unregistered email.
 */
function _dynamic_form_invite_new_email($email_norm, $form_id, $form_title, $role) {
  global $user;

  // Block duplicate pending invitations.
  $existing_id = db_select('dynamic_form_invitations', 'i')
    ->fields('i', array('id'))
    ->where('LOWER(i.email) = :email', array(':email' => $email_norm))
    ->condition('i.form_id', $form_id)
    ->condition('i.status', 'pending')
    ->execute()
    ->fetchField();

  if ($existing_id) {
    drupal_json_output(array(
      'status'  => 'error',
      'message' => t('A pending invitation has already been sent to @email.', array('@email' => $email_norm)),
    ));
    return;
  }

  $token   = _dynamic_form_generate_invite_token();
  $now     = REQUEST_TIME;
  $expires = $now + (7 * 24 * 60 * 60);

  $invite_id = (int) db_insert('dynamic_form_invitations')
    ->fields(array(
      'form_id'    => $form_id,
      'email'      => $email_norm,
      'role'       => $role,
      'token'      => $token,
      'status'     => 'pending',
      'invited_by' => $user->uid,
      'expires_at' => $expires,
      'created_at' => $now,
    ))
    ->execute();

  _dynamic_form_audit_log('invitation', $invite_id, 'created', NULL, array(
    'form_id'    => $form_id,
    'email'      => $email_norm,
    'role'       => $role,
    'invited_by' => $user->uid,
  ));

  _dynamic_form_queue_invite_notification($invite_id, $form_title);

  drupal_json_output(array(
    'status'  => 'invited',
    'message' => t('Invitation sent to @email.', array('@email' => $email_norm)),
    'invite'  => array(
      'id'              => $invite_id,
      'email'           => $email_norm,
      'role'            => $role,
      'role_label'      => _dynamic_form_role_label($role),
      'status'          => 'pending',
      'created_at'      => $now,
      'invited_by_name' => $GLOBALS['user']->name,
    ),
  ));
}

/* =====================================================================
   AJAX — RESEND INVITATION
   ===================================================================== */

/**
 * AJAX page callback: Resend a pending invitation, extending its expiry.
 *
 * Route: dynamic-form/ajax/invitations/{id}/resend
 */
function dynamic_form_ajax_invitation_resend($id) {
  $invitation = _dynamic_form_load_invitation_by_id((int) $id);

  if (!$invitation) {
    drupal_json_output(array('status' => 'error', 'message' => t('Invitation not found.')));
    return;
  }

  if ($invitation->status !== 'pending') {
    drupal_json_output(array('status' => 'error', 'message' => t('Only pending invitations can be resent.')));
    return;
  }

  if (!_dynamic_form_edit_access($invitation->form_id)) {
    drupal_json_output(array('status' => 'error', 'message' => t('Access denied.')));
    return;
  }

  $now     = REQUEST_TIME;
  $expires = $now + (7 * 24 * 60 * 60);
  $old     = (array) $invitation;

  db_update('dynamic_form_invitations')
    ->fields(array(
      'expires_at'     => $expires,
      'resend_count'   => (int) $invitation->resend_count + 1,
      'last_resent_at' => $now,
    ))
    ->condition('id', $invitation->id)
    ->execute();

  _dynamic_form_audit_log('invitation', $invitation->id, 'updated', $old, array_merge($old, array(
    'expires_at'     => $expires,
    'resend_count'   => (int) $invitation->resend_count + 1,
    'last_resent_at' => $now,
  )));

  $form_title = db_select('dynamic_form_forms', 'f')
    ->fields('f', array('title'))
    ->condition('id', $invitation->form_id)
    ->execute()
    ->fetchField();

  _dynamic_form_queue_invite_notification($invitation->id, $form_title ?: '');

  drupal_json_output(array(
    'status'  => 'ok',
    'message' => t('Invitation resent to @email.', array('@email' => $invitation->email)),
  ));
}

/* =====================================================================
   AJAX — REVOKE INVITATION
   ===================================================================== */

/**
 * AJAX page callback: Revoke a pending invitation.
 *
 * Route: dynamic-form/ajax/invitations/{id}/revoke
 */
function dynamic_form_ajax_invitation_revoke($id) {
  global $user;

  $invitation = _dynamic_form_load_invitation_by_id((int) $id);

  if (!$invitation) {
    drupal_json_output(array('status' => 'error', 'message' => t('Invitation not found.')));
    return;
  }

  if ($invitation->status !== 'pending') {
    drupal_json_output(array('status' => 'error', 'message' => t('Only pending invitations can be revoked.')));
    return;
  }

  if (!_dynamic_form_edit_access($invitation->form_id)) {
    drupal_json_output(array('status' => 'error', 'message' => t('Access denied.')));
    return;
  }

  $now = REQUEST_TIME;
  $old = (array) $invitation;

  db_update('dynamic_form_invitations')
    ->fields(array(
      'status'     => 'revoked',
      'revoked_at' => $now,
      'revoked_by' => $user->uid,
    ))
    ->condition('id', $invitation->id)
    ->execute();

  _dynamic_form_audit_log('invitation', $invitation->id, 'updated', $old, array_merge($old, array(
    'status'     => 'revoked',
    'revoked_at' => $now,
    'revoked_by' => $user->uid,
  )));

  drupal_json_output(array('status' => 'ok', 'message' => t('Invitation revoked.')));
}

/* =====================================================================
   AJAX — LIST INVITATIONS
   ===================================================================== */

/**
 * AJAX page callback: List all invitations for a form.
 *
 * Route: dynamic-form/ajax/invitations/{form_id}/list
 */
function dynamic_form_ajax_invitation_list($form_id) {
  $form_id = (int) $form_id;

  if (!_dynamic_form_edit_access($form_id)) {
    drupal_json_output(array('status' => 'error', 'message' => t('Access denied.')));
    return;
  }

  $q = db_select('dynamic_form_invitations', 'i');
  $q->join('users', 'u', 'u.uid = i.invited_by');
  $q->fields('i', array('id', 'email', 'role', 'status', 'created_at', 'expires_at', 'resend_count'));
  $q->addField('u', 'name', 'invited_by_name');
  $q->condition('i.form_id', $form_id);
  $q->orderBy('i.created_at', 'DESC');
  $q->range(0, 50);

  $rows = array();
  foreach ($q->execute()->fetchAll() as $row) {
    $rows[] = array(
      'id'              => (int) $row->id,
      'email'           => $row->email,
      'role'            => $row->role,
      'role_label'      => _dynamic_form_role_label($row->role),
      'status'          => $row->status,
      'created_at'      => (int) $row->created_at,
      'expires_at'      => (int) $row->expires_at,
      'resend_count'    => (int) $row->resend_count,
      'invited_by_name' => $row->invited_by_name,
    );
  }

  drupal_json_output(array('status' => 'ok', 'invitations' => $rows));
}

/* =====================================================================
   CORE ACCEPT HELPER
   ===================================================================== */

/**
 * Accepts an invitation: adds the user to dynamic_form_members and
 * marks the invitation record as accepted.
 *
 * @return TRUE|'already_member'|FALSE
 */
function _dynamic_form_accept_invitation($invitation, $uid) {
  $now = REQUEST_TIME;

  $existing = db_select('dynamic_form_members', 'm')
    ->fields('m', array('id', 'deleted_at'))
    ->condition('form_id', $invitation->form_id)
    ->condition('user_id', $uid)
    ->execute()
    ->fetchObject();

  if ($existing && is_null($existing->deleted_at)) {
    // Already an active member — still close out the invitation.
    db_update('dynamic_form_invitations')
      ->fields(array(
        'status'      => 'accepted',
        'user_id'     => $uid,
        'accepted_at' => $now,
      ))
      ->condition('id', $invitation->id)
      ->execute();

    unset($_SESSION['dfb_invite_token']);
    return 'already_member';
  }

  if ($existing) {
    // Restore a previously soft-deleted membership.
    db_update('dynamic_form_members')
      ->fields(array(
        'role'       => $invitation->role,
        'deleted_at' => NULL,
        'deleted_by' => NULL,
        'updated_at' => $now,
      ))
      ->condition('id', $existing->id)
      ->execute();
    $member_id = (int) $existing->id;
  }
  else {
    $member_id = (int) db_insert('dynamic_form_members')
      ->fields(array(
        'form_id'    => $invitation->form_id,
        'user_id'    => $uid,
        'role'       => $invitation->role,
        'created_by' => (int) $invitation->invited_by,
        'created_at' => $now,
        'updated_at' => $now,
      ))
      ->execute();
  }

  $old_invite = array(
    'status'      => $invitation->status,
    'user_id'     => $invitation->user_id,
    'accepted_at' => $invitation->accepted_at,
  );

  db_update('dynamic_form_invitations')
    ->fields(array(
      'status'      => 'accepted',
      'user_id'     => $uid,
      'accepted_at' => $now,
    ))
    ->condition('id', $invitation->id)
    ->execute();

  _dynamic_form_audit_log('member', $member_id, 'created', NULL, array(
    'form_id'    => (int) $invitation->form_id,
    'user_id'    => $uid,
    'role'       => $invitation->role,
    'via_invite' => (int) $invitation->id,
  ));

  _dynamic_form_audit_log('invitation', (int) $invitation->id, 'updated', $old_invite, array_merge($old_invite, array(
    'status'      => 'accepted',
    'user_id'     => $uid,
    'accepted_at' => $now,
  )));

  unset($_SESSION['dfb_invite_token']);
  return TRUE;
}

/* =====================================================================
   EMAIL QUEUE
   ===================================================================== */

/**
 * Pushes an invitation email job onto the queue.
 */
function _dynamic_form_queue_invite_notification($invite_id, $form_title) {
  $queue = DrupalQueue::get('dynamic_form_invite_notify');
  $queue->createItem(array(
    'invite_id'  => (int) $invite_id,
    'form_title' => $form_title,
  ));
}

/**
 * Queue worker: sends one invitation email.
 *
 * @param array $item  Keys: invite_id, form_title.
 */
function _dynamic_form_send_invite_notification(array $item) {
  $invitation = db_select('dynamic_form_invitations', 'i')
    ->fields('i')
    ->condition('id', $item['invite_id'])
    ->execute()
    ->fetchObject();

  // Guard: don't send if the invitation is no longer pending.
  if (!$invitation || $invitation->status !== 'pending') {
    return;
  }

  $inviter      = user_load($invitation->invited_by);
  $inviter_name = ($inviter && $inviter->name) ? $inviter->name : t('A form owner');
  $accept_url   = url('dynamic-form/invite/accept/' . $invitation->token, array('absolute' => TRUE));
  $role_label   = _dynamic_form_role_label($invitation->role);

  drupal_mail(
    'dynamic_form',
    'invitation_sent',
    $invitation->email,
    language_default(),
    array(
      'form_title'   => $item['form_title'],
      'inviter_name' => $inviter_name,
      'role_label'   => $role_label,
      'accept_url'   => $accept_url,
    )
  );
}

/* =====================================================================
   HELPERS
   ===================================================================== */

/**
 * Generates a cryptographically secure 48-character hex invite token.
 */
function _dynamic_form_generate_invite_token() {
  return bin2hex(drupal_random_bytes(24));
}

/**
 * Loads an invitation row by its token string.
 */
function _dynamic_form_load_invitation_by_token($token) {
  if (empty($token)) {
    return FALSE;
  }
  return db_select('dynamic_form_invitations', 'i')
    ->fields('i')
    ->condition('token', $token)
    ->execute()
    ->fetchObject();
}

/**
 * Loads an invitation row by its primary key.
 */
function _dynamic_form_load_invitation_by_id($id) {
  return db_select('dynamic_form_invitations', 'i')
    ->fields('i')
    ->condition('id', (int) $id)
    ->execute()
    ->fetchObject();
}

/**
 * Returns a human-readable label for a member role.
 */
function _dynamic_form_role_label($role) {
  $labels = array(
    'editor'         => t('Editor'),
    'form_applicant' => t('Form Applicant'),
    'viewer'         => t('Viewer'),
    'reviewer'       => t('Reviewer'),
    'owner'          => t('Owner'),
  );
  return isset($labels[$role]) ? $labels[$role] : drupal_ucfirst($role);
}

/**
 * Masks an email address for safe display (e.g. t***@example.com).
 */
function _dynamic_form_mask_email($email) {
  $parts = explode('@', $email, 2);
  if (count($parts) !== 2) {
    return $email;
  }
  $local  = $parts[0];
  $masked = drupal_substr($local, 0, 1) . str_repeat('*', max(3, drupal_strlen($local) - 1));
  return $masked . '@' . $parts[1];
}
