<?php

/**
 * @file
 * Page callbacks for Solr-powered tag search.
 */

/**
 * AJAX endpoint: Solr-powered autocomplete for tags (Select2 format).
 *
 * Response shape (success):
 * {
 *   "results":    [{id, text}, …],
 *   "spellcheck": {
 *     "found":       true|false,
 *     "suggestions": [{id, text}, …]
 *   }
 * }
 *
 * Response shape (Solr unavailable) — HTTP 503:
 * {
 *   "error":   true,
 *   "message": "Search service is currently unavailable."
 * }
 */
function dynamic_form_solr_tags_autocomplete() {
  $term = isset($_GET['term']) ? trim($_GET['term']) : '';

  $results    = array();
  $spellcheck = array('found' => FALSE, 'suggestions' => array());

  if (drupal_strlen($term) >= 1) {
    // 1. Solr prefix match — returns FALSE when Solr is unreachable.
    $results = dynamic_form_solr_search_tags($term, 20);

    if ($results === FALSE) {
      drupal_add_http_header('Status', '503 Service Unavailable');
      drupal_json_output(array(
        'error'   => TRUE,
        'message' => 'Search service is currently unavailable. Please try again later.',
      ));
      drupal_exit();
    }

    drupal_add_http_header('X-Search-Source', !empty($results) ? 'Solr' : 'Solr-Empty');

    // 2. Fuzzy spellcheck — only when prefix found nothing and term is long
    //    enough for edit-distance matching to be meaningful.
    if (empty($results) && drupal_strlen($term) >= 3) {
      $suggestions = dynamic_form_solr_fuzzy_search_tags($term, 6);
      if (!empty($suggestions)) {
        $spellcheck = array('found' => TRUE, 'suggestions' => $suggestions);
        drupal_add_http_header('X-Search-Source', 'Solr-Spellcheck');
      }
    }
  }

  drupal_json_output(array(
    'results'    => $results,
    'spellcheck' => $spellcheck,
  ));
  drupal_exit();
}
