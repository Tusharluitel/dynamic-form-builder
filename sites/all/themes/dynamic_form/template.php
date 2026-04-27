<?php

/**
 * @file
 * Theme preprocess functions for the Dynamic Form Builder theme.
 */

/**
 * Implements hook_preprocess_page().
 *
 * Adds body classes for page-specific styling (login, register, front).
 */
function dynamic_form_preprocess_page(&$variables) {
  $path = current_path();

  if ($path === 'login' || $path === 'user/login') {
    $variables['classes_array'][] = 'page-login';
  }
  elseif ($path === 'register' || $path === 'user/register') {
    $variables['classes_array'][] = 'page-register';
  }

  if (strpos($path, 'dashboard') === 0) {
    $variables['classes_array'][] = 'page-dashboard';
  }

  if (drupal_is_front_page()) {
    $variables['classes_array'][] = 'page-front';
  }
}

/**
 * Implements hook_preprocess_html().
 *
 * Adds page-specific body classes to the <html> element.
 */
function dynamic_form_preprocess_html(&$variables) {
  $path = current_path();

  if ($path === 'login' || $path === 'user/login') {
    $variables['classes_array'][] = 'page-login';
  }
  elseif ($path === 'register' || $path === 'user/register') {
    $variables['classes_array'][] = 'page-register';
  }

  if (strpos($path, 'dashboard') === 0) {
    $variables['classes_array'][] = 'page-dashboard';
  }
}
