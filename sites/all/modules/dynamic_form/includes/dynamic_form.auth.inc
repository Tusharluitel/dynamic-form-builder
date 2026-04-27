<?php

/**
 * @file
 * Authentication page callbacks and forms for the Dynamic Form module.
 *
 * Contains login and register form builders, validators, submit handlers,
 * and the auth page HTML wrapper.
 */

/* =====================================================================
   LOGIN
   ===================================================================== */

/**
 * Page callback for /login.
 */
function dynamic_form_login_page()
{
  drupal_set_title('');
  $form = drupal_get_form('dynamic_form_login_form');
  return _dynamic_form_auth_wrapper(drupal_render($form), 'login');
}

/**
 * Login form builder.
 */
function dynamic_form_login_form($form, &$form_state)
{
  $form['name'] = array(
    '#type' => 'textfield',
    '#title' => t('Email or Username'),
    '#required' => TRUE,
    '#attributes' => array('placeholder' => t('you@example.com'), 'autofocus' => 'autofocus'),
  );

  $form['pass'] = array(
    '#type' => 'password',
    '#title' => t('Password'),
    '#required' => TRUE,
    '#attributes' => array('placeholder' => t('Enter your password')),
  );

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Sign In'),
  );

  return $form;
}

/**
 * Login form validation.
 */
function dynamic_form_login_form_validate($form, &$form_state)
{
  $name = trim($form_state['values']['name']);
  $pass = $form_state['values']['pass'];

  if (empty($name) || empty($pass)) {
    return;
  }

  // Check flood control.
  if (!flood_is_allowed('failed_login_attempt_ip', variable_get('user_failed_login_ip_limit', 50))) {
    form_set_error('name', t('Sorry, too many failed login attempts. Please try again later.'));
    return;
  }

  $uid = user_authenticate($name, $pass);

  if ($uid) {
    $form_state['uid'] = $uid;
    flood_clear_event('failed_login_attempt_ip');
  } else {
    flood_register_event('failed_login_attempt_ip');
    form_set_error('name', t('Incorrect email or password for login.'));
  }
}

/**
 * Login form submit.
 */
function dynamic_form_login_form_submit($form, &$form_state)
{
  global $user;

  $account = user_load($form_state['uid']);
  $user = $account;
  user_login_finalize();

  // Let Login Destination module handle redirect; fallback to front page.
  $form_state['redirect'] = '<front>';
}

/* =====================================================================
   REGISTER
   ===================================================================== */

/**
 * Page callback for /register.
 */
function dynamic_form_register_page()
{
  drupal_set_title('');
  $form = drupal_get_form('dynamic_form_register_form');
  return _dynamic_form_auth_wrapper(drupal_render($form), 'register');
}

/**
 * Register form builder.
 */
function dynamic_form_register_form($form, &$form_state)
{

  $form['mail'] = array(
    '#type' => 'textfield',
    '#title' => t('Email Address'),
    '#required' => TRUE,
    '#attributes' => array('placeholder' => t('you@example.com')),
  );

  $form['name'] = array(
    '#type' => 'textfield',
    '#title' => t('Username'),
    '#required' => TRUE,
    '#attributes' => array('placeholder' => t('Choose a username')),
  );

  $form['pass'] = array(
    '#type' => 'password',
    '#title' => t('Password'),
    '#required' => TRUE,
    '#attributes' => array('placeholder' => t('Min. 8 characters')),
  );

  $form['pass_confirm'] = array(
    '#type' => 'password',
    '#title' => t('Confirm Password'),
    '#required' => TRUE,
    '#attributes' => array('placeholder' => t('Re-enter your password')),
  );

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Create Account'),
  );

  return $form;
}

/**
 * Register form validation.
 */
function dynamic_form_register_form_validate($form, &$form_state)
{
  $name  = trim($form_state['values']['name']);
  $mail  = trim($form_state['values']['mail']);
  $pass  = $form_state['values']['pass'];
  $pass2 = $form_state['values']['pass_confirm'];

  // Username validation.
  if ($error = user_validate_name($name)) {
    form_set_error('name', $error);
  } elseif (user_load_by_name($name)) {
    form_set_error('name', t('The username %name is already taken.', array('%name' => $name)));
  }

  // Email validation.
  if (!valid_email_address($mail)) {
    form_set_error('mail', t('Please enter a valid email address.'));
  } elseif (user_load_by_mail($mail)) {
    form_set_error('mail', t('An account with this email already exists.'));
  }

  // Password validation.
  if (drupal_strlen($pass) < 8) {
    form_set_error('pass', t('Password must be at least 8 characters long.'));
  }
  if ($pass !== $pass2) {
    form_set_error('pass_confirm', t('Passwords do not match.'));
  }
}

/**
 * Register form submit — create account, assign member role, auto-login.
 */
function dynamic_form_register_form_submit($form, &$form_state)
{
  global $user;

  $values = $form_state['values'];

  // Look up the 'member' role.
  $member_role = user_role_load_by_name('member');
  $roles = array();
  if ($member_role) {
    $roles[$member_role->rid] = $member_role->name;
  }

  // Create the new user account.
  $new_user = array(
    'name'   => trim($values['name']),
    'mail'   => trim($values['mail']),
    'pass'   => $values['pass'],
    'status' => 1,
    'init'   => trim($values['mail']),
    'roles'  => $roles,
  );

  $account = user_save(drupal_anonymous_user(), $new_user);

  if ($account) {
    // Auto-login.
    $user = $account;
    user_login_finalize();

    drupal_set_message(t('Welcome, @name! Your account has been created.', array('@name' => $account->name)));
    $form_state['redirect'] = '<front>';
  } else {
    drupal_set_message(t('There was a problem creating your account. Please try again.'), 'error');
  }
}

/* =====================================================================
   AUTH PAGE WRAPPER
   ===================================================================== */

/**
 * Builds the auth page wrapper HTML.
 *
 * @param string $form_html
 *   Rendered form HTML.
 * @param string $type
 *   Either 'login' or 'register'.
 *
 * @return string
 *   Full auth page HTML.
 */
function _dynamic_form_auth_wrapper($form_html, $type)
{
  if ($type === 'login') {
    $title = t('Welcome back');
    $subtitle = t('Sign in to your account');
    $footer = t('Don\'t have an account? <a href="!url">Sign up</a>', array('!url' => url('register')));
  } else {
    $title = t('Create your account');
    $subtitle = t('Start building forms in minutes');
    $footer = t('Already have an account? <a href="!url">Sign in</a>', array('!url' => url('login')));
  }

  $output  = '<div class="dfb-auth-wrapper">';
  $output .= '  <div class="dfb-auth-card">';
  $output .= '    <h2>' . $title . '</h2>';
  $output .= '    <p class="auth-subtitle">' . $subtitle . '</p>';
  $output .= '    ' . $form_html;
  $output .= '    <div class="dfb-auth-footer">' . $footer . '</div>';
  $output .= '  </div>';
  $output .= '</div>';

  return $output;
}
