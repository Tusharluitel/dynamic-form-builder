<?php

/**
 * @file
 * Page callbacks for REST API v1 — responses and answers.
 *
 * All callbacks:
 *  1. Enforce GET-only.
 *  2. Authenticate via Basic Auth (_dynamic_form_restws_authenticate).
 *  3. Check per-form permissions (_dynamic_form_restws_can_view_responses).
 *  4. Return JSON via _dynamic_form_restws_send_json / _dynamic_form_restws_send_error.
 */

/* =====================================================================
   ENDPOINT: GET api/v1/forms/{form_id}/responses
   ===================================================================== */

/**
 * Page callback: paginated list of submitted responses for one form.
 *
 * Query parameters:
 *   page      (int, 1-indexed, default 1)
 *   per_page  (int, 1–100, default 25)
 *
 * Success response shape:
 * {
 *   "api_version": "v1",
 *   "data": [ { response objects without answers } ],
 *   "meta": { "form_id", "form_title", "total", "page", "per_page", "total_pages" }
 * }
 *
 * @param string $form_id  Form primary key from the URL.
 */
function dynamic_form_restws_v1_responses_list($form_id) {
  if ($_SERVER['REQUEST_METHOD'] !== 'GET') {
    _dynamic_form_restws_send_error(405, 'Method not allowed. Use GET.');
  }

  $account = _dynamic_form_restws_authenticate();
  if (!$account) {
    _dynamic_form_restws_send_error(401, 'Valid credentials with the "access rest api" permission are required.');
  }

  $form_id = (int) $form_id;

  $form = db_select('dynamic_form_forms', 'f')
    ->fields('f', array('id', 'title', 'slug'))
    ->condition('id', $form_id)
    ->isNull('deleted_at')
    ->execute()
    ->fetchObject();

  if (!$form) {
    _dynamic_form_restws_send_error(404, 'Form not found.');
  }

  if (!_dynamic_form_restws_can_view_responses($form_id, $account)) {
    _dynamic_form_restws_send_error(403, 'You do not have permission to view responses for this form.');
  }

  // Pagination — clamp per_page to 1–100.
  $page     = max(1, isset($_GET['page'])     ? (int) $_GET['page']     : 1);
  $per_page = max(1, min(100, isset($_GET['per_page']) ? (int) $_GET['per_page'] : 25));
  $offset   = ($page - 1) * $per_page;

  $total = (int) db_select('dynamic_form_responses', 'r')
    ->condition('form_id', $form_id)
    ->condition('status', 1)
    ->isNull('deleted_at')
    ->countQuery()->execute()->fetchField();

  $rows = db_select('dynamic_form_responses', 'r')
    ->fields('r')
    ->condition('form_id', $form_id)
    ->condition('status', 1)
    ->isNull('deleted_at')
    ->orderBy('submitted_at', 'DESC')
    ->range($offset, $per_page)
    ->execute()
    ->fetchAll();

  // Batch-load registered user names and emails.
  $users = _dynamic_form_restws_load_users($rows);

  $data = array();
  foreach ($rows as $r) {
    $data[] = _dynamic_form_restws_format_response_summary($r, $users);
  }

  $total_pages = ($per_page > 0) ? (int) ceil($total / $per_page) : 0;

  _dynamic_form_restws_send_json(array(
    'api_version' => 'v1',
    'data'        => $data,
    'meta'        => array(
      'form_id'     => $form_id,
      'form_title'  => $form->title,
      'total'       => $total,
      'page'        => $page,
      'per_page'    => $per_page,
      'total_pages' => $total_pages,
    ),
  ));
}

/* =====================================================================
   ENDPOINT: GET api/v1/forms/{form_id}/responses/{response_id}
   ===================================================================== */

/**
 * Page callback: single response with all answers, grouped by question.
 *
 * Success response shape:
 * {
 *   "api_version": "v1",
 *   "data": {
 *     ...response fields...,
 *     "answers": [
 *       { "question_id", "question_label", "question_type", "value" }
 *     ]
 *   }
 * }
 *
 * @param string $form_id      Form primary key from the URL.
 * @param string $response_id  Response primary key from the URL.
 */
function dynamic_form_restws_v1_response_detail($form_id, $response_id) {
  if ($_SERVER['REQUEST_METHOD'] !== 'GET') {
    _dynamic_form_restws_send_error(405, 'Method not allowed. Use GET.');
  }

  $account = _dynamic_form_restws_authenticate();
  if (!$account) {
    _dynamic_form_restws_send_error(401, 'Valid credentials with the "access rest api" permission are required.');
  }

  $form_id     = (int) $form_id;
  $response_id = (int) $response_id;

  $form = db_select('dynamic_form_forms', 'f')
    ->fields('f', array('id', 'title', 'slug'))
    ->condition('id', $form_id)
    ->isNull('deleted_at')
    ->execute()
    ->fetchObject();

  if (!$form) {
    _dynamic_form_restws_send_error(404, 'Form not found.');
  }

  $can_view_all = _dynamic_form_restws_can_view_responses($form_id, $account);

  $response = db_select('dynamic_form_responses', 'r')
    ->fields('r')
    ->condition('id', $response_id)
    ->condition('form_id', $form_id)
    ->condition('status', 1)
    ->isNull('deleted_at')
    ->execute()
    ->fetchObject();

  if (!$response) {
    // Avoid leaking existence of responses to users without list-level access.
    $can_view_all
      ? _dynamic_form_restws_send_error(404, 'Response not found.')
      : _dynamic_form_restws_send_error(403, 'You do not have permission to view this response.');
  }

  // Form owners/editors may view all; respondents may view their own response.
  if (!$can_view_all && (int) $response->user_id !== (int) $account->uid) {
    _dynamic_form_restws_send_error(403, 'You do not have permission to view this response.');
  }

  // Respondent info.
  $uid = (int) $response->user_id;
  if ($uid) {
    $u = db_select('users', 'u')
      ->fields('u', array('name', 'mail'))
      ->condition('uid', $uid)
      ->execute()
      ->fetchObject();
    $respondent = $u
      ? array('type' => 'registered', 'uid' => $uid, 'name' => $u->name, 'email' => $u->mail)
      : array('type' => 'registered', 'uid' => $uid, 'name' => NULL, 'email' => NULL);
  }
  elseif ($response->guest_email) {
    $respondent = array('type' => 'guest', 'uid' => NULL, 'name' => NULL, 'email' => $response->guest_email);
  }
  else {
    $respondent = array('type' => 'anonymous', 'uid' => NULL, 'name' => NULL, 'email' => NULL);
  }

  // Questions ordered by position (via the cached helper).
  $questions = dynamic_form_get_questions($form_id);

  // Answers keyed by question_id (integer index required by fetchAllKeyed).
  $raw_answers = db_select('dynamic_form_answers', 'a')
    ->fields('a', array('question_id', 'value'))
    ->condition('response_id', $response_id)
    ->execute()
    ->fetchAllKeyed(0, 1);

  $answers = array();
  foreach ($questions as $q) {
    $qid      = (int) $q->id;
    $raw      = isset($raw_answers[$qid]) ? $raw_answers[$qid] : NULL;
    $answers[] = array(
      'question_id'    => $qid,
      'question_label' => $q->label,
      'question_type'  => $q->type,
      'value'          => _dynamic_form_restws_parse_answer($raw, $q->type),
    );
  }

  _dynamic_form_restws_send_json(array(
    'api_version' => 'v1',
    'data'        => array(
      'id'           => (int) $response->id,
      'form_id'      => $form_id,
      'form_title'   => $form->title,
      'respondent'   => $respondent,
      'status'       => 'submitted',
      'submitted_at' => $response->submitted_at ? date('c', $response->submitted_at) : NULL,
      'created_at'   => date('c', $response->created_at),
      'updated_at'   => date('c', $response->updated_at),
      'answers'      => $answers,
    ),
  ));
}

/* =====================================================================
   PRIVATE HELPERS
   ===================================================================== */

/**
 * Loads Drupal user records for any registered respondents in $rows.
 *
 * @param array $rows  DB result rows from dynamic_form_responses.
 * @return array  Users keyed by uid.
 */
function _dynamic_form_restws_load_users(array $rows) {
  $uid_map = array();
  foreach ($rows as $r) {
    if ((int) $r->user_id) {
      $uid_map[(int) $r->user_id] = TRUE;
    }
  }
  $users = array();
  if (!empty($uid_map)) {
    $result = db_select('users', 'u')
      ->fields('u', array('uid', 'name', 'mail'))
      ->condition('uid', array_keys($uid_map), 'IN')
      ->execute()
      ->fetchAll();
    foreach ($result as $u) {
      $users[(int) $u->uid] = $u;
    }
  }
  return $users;
}

/**
 * Formats a response DB row into the summary shape used by the list endpoint.
 *
 * @param object $r      Response DB row.
 * @param array  $users  User objects keyed by uid.
 * @return array
 */
function _dynamic_form_restws_format_response_summary($r, array $users) {
  $uid = (int) $r->user_id;
  if ($uid && isset($users[$uid])) {
    $respondent = array(
      'type'  => 'registered',
      'uid'   => $uid,
      'name'  => $users[$uid]->name,
      'email' => $users[$uid]->mail,
    );
  }
  elseif (!$uid && $r->guest_email) {
    $respondent = array('type' => 'guest',     'uid' => NULL, 'name' => NULL, 'email' => $r->guest_email);
  }
  else {
    $respondent = array('type' => 'anonymous', 'uid' => NULL, 'name' => NULL, 'email' => NULL);
  }

  return array(
    'id'           => (int) $r->id,
    'form_id'      => (int) $r->form_id,
    'respondent'   => $respondent,
    'status'       => 'submitted',
    'submitted_at' => $r->submitted_at ? date('c', $r->submitted_at) : NULL,
    'created_at'   => date('c', $r->created_at),
    'updated_at'   => date('c', $r->updated_at),
  );
}

/**
 * Parses a raw answer value from the database into an API-friendly form.
 *
 * File upload answers are stored as JSON objects; multi-select answers are
 * stored as JSON arrays. Plain text answers are returned as strings.
 *
 * @param string|NULL $raw   Raw value from dynamic_form_answers.value.
 * @param string      $type  Question type (e.g. 'text', 'checkbox', 'file').
 * @return mixed  NULL, string, array, or object depending on answer type.
 */
function _dynamic_form_restws_parse_answer($raw, $type) {
  if ($raw === NULL || $raw === '') {
    return NULL;
  }
  $decoded = json_decode($raw, TRUE);
  if (json_last_error() === JSON_ERROR_NONE) {
    return $decoded;
  }
  return $raw;
}
