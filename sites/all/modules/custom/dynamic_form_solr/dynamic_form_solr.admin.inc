<?php

/**
 * Admin settings form.
 *
 * Solr connection URL is configured via Apache Solr module settings:
 *   admin/config/search/apachesolr
 */
function dynamic_form_solr_admin_form($form, &$form_state) {
  $solr_settings_url = url('admin/config/search/apachesolr');

  $form['solr_info'] = array(
    '#markup' => '<p>' . t(
        'The Solr connection URL is managed by the <a href="@url">Apache Solr</a> module. Configure your Solr environment there.',
        array('@url' => $solr_settings_url)
      ) . '</p>',
  );

  // Connection status via the apachesolr service object.
  $form['test'] = array(
    '#type'  => 'fieldset',
    '#title' => t('Connection Test'),
  );

  try {
    $solr = apachesolr_get_solr();
    $ping = $solr->ping();
  }
  catch (Exception $e) {
    $ping = FALSE;
  }

  if ($ping) {
    $status_html = '<p style="color:green;font-weight:bold;">' . t('Connected to Solr successfully.') . '</p>';
  }
  else {
    $status_html = '<p style="color:red;font-weight:bold;">'
      . t('Cannot connect to Solr. Check your <a href="@url">Apache Solr environment settings</a>.', array('@url' => $solr_settings_url))
      . '</p>';
  }

  $form['test']['status'] = array(
    '#markup' => $status_html,
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
    '#description' => t('Clears all response documents then re-indexes every submitted response.'),
  );
  $form['reindex_responses']['reindex_responses_btn'] = array(
    '#type'   => 'submit',
    '#value'  => t('Clear and re-index all responses'),
    '#submit' => array('dynamic_form_solr_admin_reindex_responses_submit'),
  );

  return $form;
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
  drupal_set_message(t('Cleared response index and indexed @count response documents.', array('@count' => $count)));
}
