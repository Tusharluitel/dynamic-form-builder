<?php

/**
 * @file
 * Admin UI for managing Dynamic Form REST API Bearer tokens.
 *
 * Provides three pages:
 *  - Token list  (admin/config/services/dynamic-form-api-tokens)
 *  - Generate    (admin/config/services/dynamic-form-api-tokens/add)
 *  - Revoke      (admin/config/services/dynamic-form-api-tokens/{id}/revoke)
 */

/* =====================================================================
   TOKEN LIST PAGE
   ===================================================================== */

/**
 * Page callback: lists all API tokens with status and actions.
 */
function dynamic_form_restws_admin_tokens_page() {
  $result = db_select('dynamic_form_api_tokens', 't')
    ->fields('t')
    ->orderBy('created_at', 'DESC')
    ->execute()
    ->fetchAll();

  // Batch-load associated Drupal users.
  $uid_set = array();
  foreach ($result as $row) {
    $uid_set[$row->uid] = TRUE;
  }
  $users = empty($uid_set) ? array() : user_load_multiple(array_keys($uid_set));

  $header = array(
    t('Label'),
    t('User'),
    t('Created'),
    t('Last Used'),
    t('Status'),
    t('Actions'),
  );

  $rows = array();
  foreach ($result as $token) {
    $user   = isset($users[$token->uid]) ? $users[$token->uid] : NULL;
    $active = empty($token->revoked_at);

    $rows[] = array(
      check_plain($token->label),
      $user ? check_plain($user->name) : t('uid @uid', array('@uid' => $token->uid)),
      format_date($token->created_at, 'short'),
      $token->last_used_at ? format_date($token->last_used_at, 'short') : t('Never'),
      $active ? t('Active') : t('Revoked'),
      $active
        ? l(t('Revoke'), 'admin/config/services/dynamic-form-api-tokens/' . $token->id . '/revoke')
        : '',
    );
  }

  $output  = theme('table', array(
    'header' => $header,
    'rows'   => $rows,
    'empty'  => t('No API tokens found.'),
  ));
  $output .= '<p>' . l(t('Generate new token'), 'admin/config/services/dynamic-form-api-tokens/add') . '</p>';

  return $output;
}

/* =====================================================================
   GENERATE TOKEN FORM
   ===================================================================== */

/**
 * Form: generate a new Bearer token.
 */
function dynamic_form_restws_admin_token_add_form($form, &$form_state) {
  $form['label'] = array(
    '#type'        => 'textfield',
    '#title'       => t('Label'),
    '#description' => t('A descriptive name for this token, e.g. "Frontend App – Production".'),
    '#required'    => TRUE,
    '#maxlength'   => 255,
  );

  $form['username'] = array(
    '#type'              => 'textfield',
    '#title'             => t('Associated User'),
    '#description'       => t('The Drupal account this token will authenticate as. The account must have the <em>Access Dynamic Form REST API</em> permission.'),
    '#required'          => TRUE,
    '#autocomplete_path' => 'user/autocomplete',
  );

  $form['submit'] = array(
    '#type'  => 'submit',
    '#value' => t('Generate Token'),
  );

  return $form;
}

/**
 * Validate: ensure the username resolves and has the required permission.
 */
function dynamic_form_restws_admin_token_add_form_validate($form, &$form_state) {
  $username = trim($form_state['values']['username']);
  $account  = user_load_by_name($username);

  if (!$account) {
    form_set_error('username', t('User %name does not exist.', array('%name' => $username)));
    return;
  }

  if (!user_access('access rest api', $account)) {
    form_set_error('username', t(
      'User %name does not have the <em>Access Dynamic Form REST API</em> permission. Grant it at <a href="@url">admin/people/permissions</a> first.',
      array('%name' => $username, '@url' => url('admin/people/permissions'))
    ));
    return;
  }

  $form_state['values']['uid'] = $account->uid;
}

/**
 * Submit: generate a cryptographically random token, store its SHA-256 hash,
 * and display the plain token once via a status message.
 */
function dynamic_form_restws_admin_token_add_form_submit($form, &$form_state) {
  $raw_token  = bin2hex(drupal_random_bytes(32)); // 64-char hex, 256 bits of entropy.
  $token_hash = hash('sha256', $raw_token);

  db_insert('dynamic_form_api_tokens')
    ->fields(array(
      'token_hash'  => $token_hash,
      'label'       => $form_state['values']['label'],
      'uid'         => $form_state['values']['uid'],
      'created_at'  => REQUEST_TIME,
    ))
    ->execute();

  // Show the plain token exactly once — it is never stored and cannot be recovered.
  drupal_set_message(t(
    '<strong>Token generated. Copy it now — it will not be shown again:</strong><br><br>'
    . '<code style="word-break:break-all;">@token</code>',
    array('@token' => $raw_token)
  ), 'warning', FALSE);

  $form_state['redirect'] = 'admin/config/services/dynamic-form-api-tokens';
}

/* =====================================================================
   REVOKE TOKEN FORM
   ===================================================================== */

/**
 * Form: confirm revoking a token.
 *
 * @param int $token_id  Token primary key from the URL.
 */
function dynamic_form_restws_admin_token_revoke_form($form, &$form_state, $token_id) {
  $token = db_select('dynamic_form_api_tokens', 't')
    ->fields('t')
    ->condition('id', (int) $token_id)
    ->execute()
    ->fetchObject();

  if (!$token || $token->revoked_at) {
    drupal_set_message(t('Token not found or already revoked.'), 'error');
    drupal_goto('admin/config/services/dynamic-form-api-tokens');
  }

  $form['token_id'] = array('#type' => 'value', '#value' => (int) $token_id);

  return confirm_form(
    $form,
    t('Revoke token %label?', array('%label' => $token->label)),
    'admin/config/services/dynamic-form-api-tokens',
    t('Any client currently using this token will immediately lose API access. This cannot be undone.'),
    t('Revoke'),
    t('Cancel')
  );
}

/**
 * Submit: soft-delete the token by setting revoked_at.
 */
function dynamic_form_restws_admin_token_revoke_form_submit($form, &$form_state) {
  db_update('dynamic_form_api_tokens')
    ->fields(array('revoked_at' => REQUEST_TIME))
    ->condition('id', $form_state['values']['token_id'])
    ->execute();

  drupal_set_message(t('Token revoked successfully.'));
  $form_state['redirect'] = 'admin/config/services/dynamic-form-api-tokens';
}
