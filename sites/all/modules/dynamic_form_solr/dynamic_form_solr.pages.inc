<?php

/**
 * @file
 * Page callbacks for Solr-powered tag search.
 */

/**
 * AJAX endpoint: Solr-powered autocomplete for tags (Select2 format).
 *
 * URL: dynamic-form/ajax/tags/solr-autocomplete?term=php
 * Returns JSON array of {id, text} objects.
 *
 * Falls back to the DB query if Solr is unreachable.
 */
function dynamic_form_solr_tags_autocomplete() {
  $term = isset($_GET['term']) ? trim($_GET['term']) : '';

  $results = array();

  if (drupal_strlen($term) >= 1) {
    // Try Solr first.
    $results = dynamic_form_solr_search_tags($term, 20);

    if (!empty($results)) {
      drupal_add_http_header('X-Search-Source', 'Solr');
    }
    else {
      // Fallback to DB if Solr returned nothing (or failed).
      drupal_add_http_header('X-Search-Source', 'Database');
      $query = db_select('dynamic_form_tags', 't')
        ->fields('t', array('standardized_value'))
        ->condition('standardized_value', db_like($term) . '%', 'LIKE')
        ->orderBy('standardized_value', 'ASC')
        ->range(0, 20)
        ->execute();

      foreach ($query as $row) {
        $results[] = array(
          'id'   => $row->standardized_value,
          'text' => $row->standardized_value,
        );
      }
    }
  }

  drupal_json_output($results);
  drupal_exit();
}
