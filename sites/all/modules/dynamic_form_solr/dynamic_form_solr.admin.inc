<?php

/**
 * @file
 * Admin form for Dynamic Form Solr Tags — configure URL and trigger re-index.
 */

/**
 * Admin settings form.
 */
function dynamic_form_solr_admin_form($form, &$form_state) {
  $form['dynamic_form_solr_url'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Solr Core URL'),
    '#description'   => t('Full URL to the Solr core (no trailing slash). Example: http://localhost:8983/solr/dynamic-form-builder'),
    '#default_value' => variable_get('dynamic_form_solr_url', DYNAMIC_FORM_SOLR_DEFAULT_URL),
    '#required'      => TRUE,
  );

  // Test connection button.
  $form['test'] = array(
    '#type'   => 'fieldset',
    '#title'  => t('Connection Test'),
  );

  $solr_url = variable_get('dynamic_form_solr_url', DYNAMIC_FORM_SOLR_DEFAULT_URL);
  $ping     = _dynamic_form_solr_request($solr_url . '/admin/ping', NULL, 'GET');

  if ($ping !== FALSE) {
    $form['test']['status'] = array(
      '#markup' => '<p style="color:green;font-weight:bold;">Connected to Solr successfully.</p>',
    );
  }
  else {
    $form['test']['status'] = array(
      '#markup' => '<p style="color:red;font-weight:bold;">Cannot connect to Solr at ' . check_plain($solr_url) . '</p>',
    );
  }

  // Re-index actions.
  $form['reindex'] = array(
    '#type'  => 'fieldset',
    '#title' => t('Re-index Tags'),
  );

  $form['reindex']['reindex_btn'] = array(
    '#type'   => 'submit',
    '#value'  => t('Re-index all tags now'),
    '#submit' => array('dynamic_form_solr_admin_reindex_submit'),
  );

  $form['reindex']['clear_btn'] = array(
    '#type'   => 'submit',
    '#value'  => t('Clear index and re-index'),
    '#submit' => array('dynamic_form_solr_admin_clear_reindex_submit'),
  );

  // Standard system settings save.
  return system_settings_form($form);
}

/**
 * Submit handler: re-index without clearing.
 */
function dynamic_form_solr_admin_reindex_submit($form, &$form_state) {
  $count = dynamic_form_solr_index_all(FALSE);
  drupal_set_message(t('Indexed @count tags to Solr.', array('@count' => $count)));
}

/**
 * Submit handler: clear and re-index.
 */
function dynamic_form_solr_admin_clear_reindex_submit($form, &$form_state) {
  $count = dynamic_form_solr_index_all(TRUE);
  drupal_set_message(t('Cleared index and re-indexed @count tags.', array('@count' => $count)));
}
