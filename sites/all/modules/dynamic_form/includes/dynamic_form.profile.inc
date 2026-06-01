<?php

/**
 * @file
 * User profile page callback for the Dynamic Form Builder dashboard.
 */

/**
 * Page callback: dashboard/profile
 */
function dynamic_form_profile_page() {
  global $user;

  drupal_set_title(t('My Profile'));

  module_load_include('inc', 'dynamic_form', 'includes/dynamic_form.dashboard');

  $module_path = drupal_get_path('module', 'dynamic_form');
  drupal_add_css($module_path . '/css/profile.css');

  $account = user_load($user->uid);

  // Count forms created by this user.
  $forms_count = (int) db_select('dynamic_form_forms', 'f')
    ->condition('f.created_by', $user->uid)
    ->isNull('f.deleted_at')
    ->countQuery()
    ->execute()
    ->fetchField();

  // Count responses submitted by this user.
  $responses_count = (int) db_select('dynamic_form_responses', 'r')
    ->condition('r.user_id', $user->uid)
    ->isNull('r.deleted_at')
    ->countQuery()
    ->execute()
    ->fetchField();

  // Derive display role label (highest-priority custom role).
  $role_priority = array('administrator', 'editor', 'form_applicant', 'authenticated user');
  $display_role  = t('Member');
  foreach ($role_priority as $role_name) {
    foreach ($account->roles as $rid => $name) {
      if ($name === $role_name) {
        $display_role = check_plain(ucwords(str_replace('_', ' ', $name)));
        break 2;
      }
    }
  }

  // Build initials avatar from username.
  $initials = drupal_strtoupper(drupal_substr($account->name, 0, 1));

  $out  = '<div class="dfb-profile-page">';

  // Profile card.
  $out .= '<div class="dfb-profile-card">';
  $out .= '<div class="dfb-profile-avatar">' . check_plain($initials) . '</div>';
  $out .= '<div class="dfb-profile-info">';
  $out .= '<h2 class="dfb-profile-name">' . check_plain($account->name) . '</h2>';
  $out .= '<p class="dfb-profile-email">' . check_plain($account->mail) . '</p>';
  $out .= '<span class="dfb-badge dfb-badge-shared dfb-profile-role">' . $display_role . '</span>';
  $out .= '</div>';
  $out .= '<div class="dfb-profile-actions">';
  $out .= l(t('Edit Profile'), 'user/' . $user->uid . '/edit', array('attributes' => array('class' => array('dfb-btn-edit-profile'))));
  $out .= '</div>';
  $out .= '</div>';

  // Stats row.
  $out .= '<div class="dfb-profile-stats">';
  $out .= _dfb_profile_stat(t('Member Since'), format_date($account->created, 'custom', 'M j, Y'), '<i class="fa-regular fa-calendar"></i>');
  $out .= _dfb_profile_stat(t('Forms Created'), $forms_count, '<i class="fa-regular fa-file-lines"></i>');
  $out .= _dfb_profile_stat(t('Responses Submitted'), $responses_count, '<i class="fa-regular fa-pen-to-square"></i>');
  $out .= '</div>';

  $out .= '</div>';

  $sidebar_html = _dynamic_form_dashboard_sidebar('profile');

  return theme('dynamic_form_dashboard', array(
    'sidebar' => $sidebar_html,
    'content' => $out,
  ));
}

/**
 * Renders a single profile stat tile.
 */
function _dfb_profile_stat($label, $value, $icon) {
  $out  = '<div class="dfb-profile-stat-tile">';
  $out .= '<span class="dfb-profile-stat-icon">' . $icon . '</span>';
  $out .= '<span class="dfb-profile-stat-value">' . $value . '</span>';
  $out .= '<span class="dfb-profile-stat-label">' . $label . '</span>';
  $out .= '</div>';
  return $out;
}
