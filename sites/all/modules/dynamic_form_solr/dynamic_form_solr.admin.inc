<?php

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

  $form['test'] = array(
    '#type'  => 'fieldset',
    '#title' => t('Connection Test'),
  );

  $solr_url = variable_get('dynamic_form_solr_url', DYNAMIC_FORM_SOLR_DEFAULT_URL);
  $ping     = _dynamic_form_solr_request($solr_url . '/admin/ping', NULL, 'GET');

  $form['test']['status'] = array(
    '#markup' => $ping !== FALSE
      ? '<p style="color:green;font-weight:bold;">Connected to Solr successfully.</p>'
      : '<p style="color:red;font-weight:bold;">Cannot connect to Solr at ' . check_plain($solr_url) . '</p>',
  );

  $form['reindex_tags'] = array(
    '#type'  => 'fieldset',
    '#title' => t('Re-index Tags'),
  );
  $form['reindex_tags']['reindex_btn'] = array(
    '#type'   => 'submit',
    '#value'  => t('Re-index all tags now'),
    '#submit' => array('dynamic_form_solr_admin_reindex_submit'),
  );
  $form['reindex_tags']['clear_btn'] = array(
    '#type'   => 'submit',
    '#value'  => t('Clear tag index and re-index'),
    '#submit' => array('dynamic_form_solr_admin_clear_reindex_submit'),
  );

  $form['reindex_responses'] = array(
    '#type'        => 'fieldset',
    '#title'       => t('Re-index Responses'),
    '#description' => t('Clears all answer documents (and any legacy response documents) then re-indexes every submitted response.'),
  );
  $form['reindex_responses']['reindex_responses_btn'] = array(
    '#type'   => 'submit',
    '#value'  => t('Clear and re-index all responses'),
    '#submit' => array('dynamic_form_solr_admin_reindex_responses_submit'),
  );

  return system_settings_form($form);
}

function dynamic_form_solr_admin_reindex_submit($form, &$form_state) {
  $count = dynamic_form_solr_index_all(FALSE);
  drupal_set_message(t('Indexed @count tags to Solr.', array('@count' => $count)));
}

function dynamic_form_solr_admin_clear_reindex_submit($form, &$form_state) {
  $count = dynamic_form_solr_index_all(TRUE);
  drupal_set_message(t('Cleared tag index and re-indexed @count tags.', array('@count' => $count)));
}

function dynamic_form_solr_admin_reindex_responses_submit($form, &$form_state) {
  $count = dynamic_form_solr_index_all_responses();
  drupal_set_message(t('Cleared response index and indexed @count answer documents.', array('@count' => $count)));
}
