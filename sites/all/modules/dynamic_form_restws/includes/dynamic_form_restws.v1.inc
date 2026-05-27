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
   ENDPOINT: GET api/v1/forms
   ===================================================================== */

/**
 * Page callback: paginated list of forms, with optional filters.
 *
 * Query parameters:
 *   status          Comma-separated status codes: 0=draft, 1=published, 2=closed
 *   visibility      Comma-separated codes: p=public, r=restricted, m=members only
 *   opens_at_from   ISO 8601 date (YYYY-MM-DD) or Unix timestamp — inclusive lower bound
 *   opens_at_to     ISO 8601 date (YYYY-MM-DD) or Unix timestamp — inclusive upper bound
 *   closes_at_from  ISO 8601 date (YYYY-MM-DD) or Unix timestamp — inclusive lower bound
 *   closes_at_to    ISO 8601 date (YYYY-MM-DD) or Unix timestamp — inclusive upper bound
 *   page            1-indexed page number (default 1)
 *   per_page        Items per page, 1–100 (default 25)
 *
 * Users with "view any form responses" see all non-deleted forms.
 * All other authenticated users see only forms they created or are members of.
 *
 * Success response shape:
 * {
 *   "api_version": "v1",
 *   "data": [ { form objects } ],
 *   "meta": { "total", "page", "per_page", "total_pages", "filters" }
 * }
 */
function dynamic_form_restws_v1_forms_list() {
  if ($_SERVER['REQUEST_METHOD'] !== 'GET') {
    _dynamic_form_restws_send_error(405, 'Method not allowed. Use GET.');
  }

  $account = _dynamic_form_restws_authenticate();
  if (!$account) {
    _dynamic_form_restws_send_error(401, 'Valid credentials with the "access rest api" permission are required.');
  }

  // Build base query.
  $query = db_select('dynamic_form_forms', 'f')
    ->fields('f')
    ->isNull('f.deleted_at');

  // Scope to forms the caller owns or is a member of when they lack global access.
  if (!user_access('view any form responses', $account)) {
    $member_sub = db_select('dynamic_form_members', 'm')
      ->fields('m', array('form_id'))
      ->condition('m.user_id', $account->uid)
      ->isNull('m.deleted_at');

    $or = db_or()
      ->condition('f.created_by', $account->uid)
      ->condition('f.id', $member_sub, 'IN');

    $query->condition($or);
  }

  // --- status filter ---
  $applied_filters = array();
  if (!empty($_GET['status'])) {
    $allowed_statuses = array(0, 1, 2);
    $raw = array_map('trim', explode(',', $_GET['status']));
    $statuses = array();
    foreach ($raw as $s) {
      $int = (int) $s;
      if (in_array($int, $allowed_statuses, TRUE)) {
        $statuses[] = $int;
      }
    }
    if (empty($statuses)) {
      _dynamic_form_restws_send_error(400, 'Invalid "status" filter. Allowed values: 0 (draft), 1 (published), 2 (closed).');
    }
    $query->condition('f.status', $statuses, 'IN');
    $applied_filters['status'] = $statuses;
  }

  // --- visibility filter ---
  if (!empty($_GET['visibility'])) {
    $allowed_vis = array('p', 'r', 'm');
    $raw = array_map('trim', explode(',', strtolower($_GET['visibility'])));
    $vis = array_values(array_intersect($raw, $allowed_vis));
    if (empty($vis)) {
      _dynamic_form_restws_send_error(400, 'Invalid "visibility" filter. Allowed values: p (public), r (restricted), m (members only).');
    }
    $query->condition('f.visibility', $vis, 'IN');
    $applied_filters['visibility'] = $vis;
  }

  // --- opens_at range filter ---
  if (!empty($_GET['opens_at_from'])) {
    $ts = _dynamic_form_restws_parse_date_param($_GET['opens_at_from']);
    if ($ts === FALSE) {
      _dynamic_form_restws_send_error(400, 'Invalid "opens_at_from". Use YYYY-MM-DD or a Unix timestamp.');
    }
    $query->condition('f.opens_at', $ts, '>=');
    $applied_filters['opens_at_from'] = date('c', $ts);
  }
  if (!empty($_GET['opens_at_to'])) {
    $ts = _dynamic_form_restws_parse_date_param($_GET['opens_at_to'], TRUE);
    if ($ts === FALSE) {
      _dynamic_form_restws_send_error(400, 'Invalid "opens_at_to". Use YYYY-MM-DD or a Unix timestamp.');
    }
    $query->condition('f.opens_at', $ts, '<=');
    $applied_filters['opens_at_to'] = date('c', $ts);
  }

  // --- closes_at range filter ---
  if (!empty($_GET['closes_at_from'])) {
    $ts = _dynamic_form_restws_parse_date_param($_GET['closes_at_from']);
    if ($ts === FALSE) {
      _dynamic_form_restws_send_error(400, 'Invalid "closes_at_from". Use YYYY-MM-DD or a Unix timestamp.');
    }
    $query->condition('f.closes_at', $ts, '>=');
    $applied_filters['closes_at_from'] = date('c', $ts);
  }
  if (!empty($_GET['closes_at_to'])) {
    $ts = _dynamic_form_restws_parse_date_param($_GET['closes_at_to'], TRUE);
    if ($ts === FALSE) {
      _dynamic_form_restws_send_error(400, 'Invalid "closes_at_to". Use YYYY-MM-DD or a Unix timestamp.');
    }
    $query->condition('f.closes_at', $ts, '<=');
    $applied_filters['closes_at_to'] = date('c', $ts);
  }

  // Pagination.
  $page     = max(1, isset($_GET['page'])     ? (int) $_GET['page']     : 1);
  $per_page = max(1, min(100, isset($_GET['per_page']) ? (int) $_GET['per_page'] : 25));
  $offset   = ($page - 1) * $per_page;

  $count_query = clone $query;
  $total = (int) $count_query->countQuery()->execute()->fetchField();

  $rows = $query
    ->orderBy('f.created_at', 'DESC')
    ->range($offset, $per_page)
    ->execute()
    ->fetchAll();

  $data = array();
  foreach ($rows as $row) {
    $data[] = _dynamic_form_restws_format_form($row);
  }

  _dynamic_form_restws_send_json(array(
    'api_version' => 'v1',
    'data'        => $data,
    'meta'        => array(
      'total'          => $total,
      'page'           => $page,
      'per_page'       => $per_page,
      'total_pages'    => ($per_page > 0) ? (int) ceil($total / $per_page) : 0,
      'filters_applied' => $applied_filters,
    ),
  ));
}

/* =====================================================================
   ENDPOINT: GET api/v1/forms/questions/{question_id}
   ===================================================================== */

/**
 * Page callback: question detail with paginated, searchable answers.
 *
 * Loads the question (and its options for option-bearing types), then returns
 * all submitted answers joined with their parent response so respondent info
 * is included inline. Only submitted (status=1) non-deleted responses are
 * included.
 *
 * Query parameters:
 *   answer_search  Substring match on the raw answer value (case-insensitive).
 *   page           1-indexed page number (default 1).
 *   per_page       Items per page, 1–100 (default 25).
 *
 * Access: same permission check as viewing responses for the parent form.
 *
 * Success response shape:
 * {
 *   "api_version": "v1",
 *   "data": {
 *     "question": { ...question fields..., "options": [...] },
 *     "answers": [
 *       {
 *         "id", "response_id", "value", "parsed_value",
 *         "respondent": { "type", "uid", "name", "email" },
 *         "submitted_at", "created_at", "updated_at"
 *       }
 *     ]
 *   },
 *   "meta": { "question_id", "form_id", "total", "page", "per_page",
 *             "total_pages", "filters_applied" }
 * }
 *
 * @param string $question_id  Question primary key from the URL.
 */
function dynamic_form_restws_v1_question_answers($question_id) {
  if ($_SERVER['REQUEST_METHOD'] !== 'GET') {
    _dynamic_form_restws_send_error(405, 'Method not allowed. Use GET.');
  }

  $account = _dynamic_form_restws_authenticate();
  if (!$account) {
    _dynamic_form_restws_send_error(401, 'Valid credentials with the "access rest api" permission are required.');
  }

  $question_id = (int) $question_id;

  // Load the question.
  $question = db_select('dynamic_form_questions', 'q')
    ->fields('q')
    ->condition('q.id', $question_id)
    ->isNull('q.deleted_at')
    ->execute()
    ->fetchObject();

  if (!$question) {
    _dynamic_form_restws_send_error(404, 'Question not found.');
  }

  $form_id = (int) $question->form_id;

  // Verify the parent form exists.
  $form_exists = (bool) db_select('dynamic_form_forms', 'f')
    ->condition('f.id', $form_id)
    ->isNull('f.deleted_at')
    ->countQuery()->execute()->fetchField();

  if (!$form_exists) {
    _dynamic_form_restws_send_error(404, 'Parent form not found.');
  }

  if (!_dynamic_form_restws_can_view_responses($form_id, $account)) {
    _dynamic_form_restws_send_error(403, 'You do not have permission to view answers for this question.');
  }

  // Load options for option-bearing question types.
  $option_bearing = array('select', 'radio', 'checkbox', 'tags');
  $options = array();
  if (in_array($question->type, $option_bearing, TRUE)) {
    $opt_rows = db_select('dynamic_form_question_options', 'o')
      ->fields('o', array('id', 'option_label', 'position'))
      ->condition('o.question_id', $question_id)
      ->isNull('o.deleted_at')
      ->orderBy('o.position', 'ASC')
      ->execute()
      ->fetchAll();

    foreach ($opt_rows as $opt) {
      $options[] = array(
        'id'       => (int) $opt->id,
        'label'    => $opt->option_label,
        'position' => (int) $opt->position,
      );
    }
  }

  // Build the answers query — JOIN answers → responses to get respondent info.
  $query = db_select('dynamic_form_answers', 'a');
  $query->join('dynamic_form_responses', 'r', 'r.id = a.response_id');
  $query
    ->fields('a', array('id', 'response_id', 'value', 'created_at', 'updated_at'))
    ->fields('r', array('user_id', 'guest_email', 'submitted_at'))
    ->condition('a.question_id', $question_id)
    ->isNull('a.deleted_at')
    ->condition('r.status', 1)       // submitted only
    ->isNull('r.deleted_at')
    ->orderBy('r.submitted_at', 'DESC');

  $applied_filters = array();
  $answer_search = isset($_GET['answer_search']) ? trim($_GET['answer_search']) : '';
  if ($answer_search !== '') {
    $query->condition('a.value', '%' . db_like($answer_search) . '%', 'LIKE');
    $applied_filters['answer_search'] = $answer_search;
  }

  // Pagination.
  $page     = max(1, isset($_GET['page'])     ? (int) $_GET['page']     : 1);
  $per_page = max(1, min(100, isset($_GET['per_page']) ? (int) $_GET['per_page'] : 25));
  $offset   = ($page - 1) * $per_page;

  $count_query = clone $query;
  $total = (int) $count_query->countQuery()->execute()->fetchField();

  $answer_rows = $query
    ->range($offset, $per_page)
    ->execute()
    ->fetchAll();

  // Batch-load registered user names in one query.
  $uid_map = array();
  foreach ($answer_rows as $ar) {
    if ((int) $ar->user_id) {
      $uid_map[(int) $ar->user_id] = TRUE;
    }
  }
  $users = array();
  if (!empty($uid_map)) {
    $user_rows = db_select('users', 'u')
      ->fields('u', array('uid', 'name', 'mail'))
      ->condition('u.uid', array_keys($uid_map), 'IN')
      ->execute()
      ->fetchAll();
    foreach ($user_rows as $u) {
      $users[(int) $u->uid] = $u;
    }
  }

  // Format answers.
  $answers = array();
  foreach ($answer_rows as $ar) {
    $uid = (int) $ar->user_id;
    if ($uid && isset($users[$uid])) {
      $respondent = array(
        'type'  => 'registered',
        'uid'   => $uid,
        'name'  => $users[$uid]->name,
        'email' => $users[$uid]->mail,
      );
    }
    elseif (!$uid && !empty($ar->guest_email)) {
      $respondent = array('type' => 'guest', 'uid' => NULL, 'name' => NULL, 'email' => $ar->guest_email);
    }
    else {
      $respondent = array('type' => 'anonymous', 'uid' => NULL, 'name' => NULL, 'email' => NULL);
    }

    $answers[] = array(
      'id'           => (int) $ar->id,
      'response_id'  => (int) $ar->response_id,
      'value'        => _dynamic_form_restws_parse_answer($ar->value, $question->type),
      'respondent'   => $respondent,
      'submitted_at' => $ar->submitted_at ? date('c', $ar->submitted_at) : NULL,
      'created_at'   => date('c', $ar->created_at),
      'updated_at'   => date('c', $ar->updated_at),
    );
  }

  // Build the question payload (reuse the existing formatter, inject options).
  $options_by_question = array($question_id => $options);
  $question_data = _dynamic_form_restws_format_question($question, $options_by_question);

  _dynamic_form_restws_send_json(array(
    'api_version' => 'v1',
    'data'        => array(
      'question' => $question_data,
      'answers'  => $answers,
    ),
    'meta'        => array(
      'question_id'     => $question_id,
      'form_id'         => $form_id,
      'total'           => $total,
      'page'            => $page,
      'per_page'        => $per_page,
      'total_pages'     => ($per_page > 0) ? (int) ceil($total / $per_page) : 0,
      'filters_applied' => $applied_filters,
    ),
  ));
}

/* =====================================================================
   ENDPOINT: GET api/v1/forms/{form_id}
   ===================================================================== */

/**
 * Page callback: form detail with eagerly loaded sections and questions.
 *
 * Sections are ordered by position. Questions within each section are ordered
 * by position. For select/radio/checkbox/tags questions, options
 * are included as a sub-array ordered by position.
 *
 * Query parameters (all optional):
 *   section_search   Substring match on section name (case-insensitive).
 *   question_search  Substring match on question label (case-insensitive).
 *   question_type    Comma-separated list of question type keys to include.
 *
 * Success response shape:
 * {
 *   "api_version": "v1",
 *   "data": {
 *     ...form fields...,
 *     "sections": [
 *       {
 *         ...section fields...,
 *         "questions": [ { ...question fields..., "options": [...] } ]
 *       }
 *     ],
 *     "unsectioned_questions": [ { ...question fields..., "options": [...] } ]
 *   },
 *   "meta": { "filters_applied": {...} }
 * }
 *
 * @param string $form_id  Form primary key from the URL.
 */
function dynamic_form_restws_v1_form_detail($form_id) {
  if ($_SERVER['REQUEST_METHOD'] !== 'GET') {
    _dynamic_form_restws_send_error(405, 'Method not allowed. Use GET.');
  }

  $account = _dynamic_form_restws_authenticate();
  if (!$account) {
    _dynamic_form_restws_send_error(401, 'Valid credentials with the "access rest api" permission are required.');
  }

  $form_id = (int) $form_id;

  $form = db_select('dynamic_form_forms', 'f')
    ->fields('f')
    ->condition('f.id', $form_id)
    ->isNull('f.deleted_at')
    ->execute()
    ->fetchObject();

  if (!$form) {
    _dynamic_form_restws_send_error(404, 'Form not found.');
  }

  if (!_dynamic_form_restws_can_view_responses($form_id, $account)) {
    _dynamic_form_restws_send_error(403, 'You do not have permission to view this form.');
  }

  $applied_filters = array();

  // ------------------------------------------------------------------
  // 1. Load sections.
  // ------------------------------------------------------------------
  $section_query = db_select('dynamic_form_sections', 's')
    ->fields('s')
    ->condition('s.form_id', $form_id)
    ->isNull('s.deleted_at')
    ->orderBy('s.position', 'ASC');

  $section_search = isset($_GET['section_search']) ? trim($_GET['section_search']) : '';
  if ($section_search !== '') {
    $section_query->condition('s.name', '%' . db_like($section_search) . '%', 'LIKE');
    $applied_filters['section_search'] = $section_search;
  }

  $section_rows = $section_query->execute()->fetchAll();

  // Build a set of section IDs that survived the filter (used to scope
  // question loading when section_search is active).
  $section_ids = array();
  $sections_by_id = array();
  foreach ($section_rows as $sr) {
    $sid = (int) $sr->id;
    $section_ids[] = $sid;
    $sections_by_id[$sid] = _dynamic_form_restws_format_section($sr);
    $sections_by_id[$sid]['questions'] = array();
  }

  // ------------------------------------------------------------------
  // 2. Load questions.
  // ------------------------------------------------------------------
  $question_query = db_select('dynamic_form_questions', 'q')
    ->fields('q')
    ->condition('q.form_id', $form_id)
    ->isNull('q.deleted_at')
    ->orderBy('q.section_id', 'ASC')
    ->orderBy('q.position', 'ASC');

  // When section_search is active, restrict questions to matching sections
  // (questions without a section are excluded since their section was filtered).
  if ($section_search !== '' && !empty($section_ids)) {
    $question_query->condition('q.section_id', $section_ids, 'IN');
  }
  elseif ($section_search !== '' && empty($section_ids)) {
    // No sections matched — no questions to return.
    $question_query->condition('q.id', 0);
  }

  $question_search = isset($_GET['question_search']) ? trim($_GET['question_search']) : '';
  if ($question_search !== '') {
    $question_query->condition('q.label', '%' . db_like($question_search) . '%', 'LIKE');
    $applied_filters['question_search'] = $question_search;
  }

  $question_type_filter = array();
  if (!empty($_GET['question_type'])) {
    $question_type_filter = array_filter(array_map('trim', explode(',', $_GET['question_type'])));
    if (!empty($question_type_filter)) {
      $question_query->condition('q.type', $question_type_filter, 'IN');
      $applied_filters['question_type'] = array_values($question_type_filter);
    }
  }

  $question_rows = $question_query->execute()->fetchAll();

  // ------------------------------------------------------------------
  // 3. Batch-load options for option-bearing question types.
  // ------------------------------------------------------------------
  $option_bearing = array('select', 'radio', 'checkbox', 'tags');
  $question_ids_needing_options = array();
  foreach ($question_rows as $qr) {
    if (in_array($qr->type, $option_bearing, TRUE)) {
      $question_ids_needing_options[] = (int) $qr->id;
    }
  }

  $options_by_question = array();
  if (!empty($question_ids_needing_options)) {
    $option_rows = db_select('dynamic_form_question_options', 'o')
      ->fields('o', array('id', 'question_id', 'option_label', 'position'))
      ->condition('o.question_id', $question_ids_needing_options, 'IN')
      ->isNull('o.deleted_at')
      ->orderBy('o.position', 'ASC')
      ->execute()
      ->fetchAll();

    foreach ($option_rows as $opt) {
      $qid = (int) $opt->question_id;
      $options_by_question[$qid][] = array(
        'id'       => (int) $opt->id,
        'label'    => $opt->option_label,
        'position' => (int) $opt->position,
      );
    }
  }

  // ------------------------------------------------------------------
  // 4. Stitch questions into sections.
  // ------------------------------------------------------------------
  $unsectioned = array();
  foreach ($question_rows as $qr) {
    $formatted = _dynamic_form_restws_format_question($qr, $options_by_question);
    $sid = $qr->section_id !== NULL ? (int) $qr->section_id : NULL;

    if ($sid !== NULL && isset($sections_by_id[$sid])) {
      $sections_by_id[$sid]['questions'][] = $formatted;
    }
    elseif ($sid === NULL) {
      $unsectioned[] = $formatted;
    }
    // Questions whose section was filtered out are silently dropped.
  }

  _dynamic_form_restws_send_json(array(
    'api_version' => 'v1',
    'data'        => array(
      'form'                  => _dynamic_form_restws_format_form($form),
      'sections'              => array_values($sections_by_id),
    ),
    'meta'        => array(
      'filters_applied' => $applied_filters,
    ),
  ));
}

/* =====================================================================
   ENDPOINT: GET api/v1/forms/{form_id}/responses
   ===================================================================== */

/**
 * Page callback: paginated, filterable list of submitted responses for one form.
 *
 * Query parameters:
 *   page              (int, 1-indexed, default 1)
 *   per_page          (int, 1–100, default 25)
 *   submitted_from    YYYY-MM-DD or Unix timestamp — inclusive lower bound on submitted_at
 *   submitted_to      YYYY-MM-DD or Unix timestamp — inclusive upper bound on submitted_at
 *   respondent_search Substring matched against the respondent's name, email, or guest email
 *
 * Success response shape:
 * {
 *   "api_version": "v1",
 *   "data": [ { response objects without answers } ],
 *   "meta": { "form_id", "form_title", "total", "page", "per_page",
 *             "total_pages", "filters_applied" }
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

  $applied_filters = array();

  // Base query — always scoped to submitted, non-deleted responses for this form.
  $query = db_select('dynamic_form_responses', 'r')
    ->fields('r')
    ->condition('r.form_id', $form_id)
    ->condition('r.status', 1)
    ->isNull('r.deleted_at')
    ->orderBy('r.submitted_at', 'DESC');

  // --- respondent_search ---
  // LEFT JOIN users so we can match on name/email; guest_email lives on the
  // response row itself. The join is added only when the filter is active.
  $respondent_search = isset($_GET['respondent_search']) ? trim($_GET['respondent_search']) : '';
  if ($respondent_search !== '') {
    $query->leftJoin('users', 'u', 'u.uid = r.user_id');
    $or = db_or()
      ->condition('u.name',       '%' . db_like($respondent_search) . '%', 'LIKE')
      ->condition('u.mail',       '%' . db_like($respondent_search) . '%', 'LIKE')
      ->condition('r.guest_email','%' . db_like($respondent_search) . '%', 'LIKE');
    $query->condition($or);
    $applied_filters['respondent_search'] = $respondent_search;
  }

  // --- submitted_from / submitted_to ---
  if (!empty($_GET['submitted_from'])) {
    $ts = _dynamic_form_restws_parse_date_param($_GET['submitted_from']);
    if ($ts === FALSE) {
      _dynamic_form_restws_send_error(400, 'Invalid "submitted_from". Use YYYY-MM-DD or a Unix timestamp.');
    }
    $query->condition('r.submitted_at', $ts, '>=');
    $applied_filters['submitted_from'] = date('c', $ts);
  }
  if (!empty($_GET['submitted_to'])) {
    $ts = _dynamic_form_restws_parse_date_param($_GET['submitted_to'], TRUE);
    if ($ts === FALSE) {
      _dynamic_form_restws_send_error(400, 'Invalid "submitted_to". Use YYYY-MM-DD or a Unix timestamp.');
    }
    $query->condition('r.submitted_at', $ts, '<=');
    $applied_filters['submitted_to'] = date('c', $ts);
  }

  // Pagination — clamp per_page to 1–100.
  $page     = max(1, isset($_GET['page'])     ? (int) $_GET['page']     : 1);
  $per_page = max(1, min(100, isset($_GET['per_page']) ? (int) $_GET['per_page'] : 25));
  $offset   = ($page - 1) * $per_page;

  $count_query = clone $query;
  $total = (int) $count_query->countQuery()->execute()->fetchField();

  $rows = $query
    ->range($offset, $per_page)
    ->execute()
    ->fetchAll();

  // Batch-load registered user names and emails.
  $users = _dynamic_form_restws_load_users($rows);

  $data = array();
  foreach ($rows as $r) {
    $data[] = _dynamic_form_restws_format_response_summary($r, $users);
  }

  _dynamic_form_restws_send_json(array(
    'api_version' => 'v1',
    'data'        => $data,
    'meta'        => array(
      'form_id'         => $form_id,
      'form_title'      => $form->title,
      'total'           => $total,
      'page'            => $page,
      'per_page'        => $per_page,
      'total_pages'     => ($per_page > 0) ? (int) ceil($total / $per_page) : 0,
      'filters_applied' => $applied_filters,
    ),
  ));
}

/* =====================================================================
   ENDPOINT: GET api/v1/forms/{form_id}/responses/{response_id}
   ===================================================================== */

/**
 * Page callback: single response detail — form-scoped URL (form_id + response_id).
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
    ->condition('form_id', $form_id)  // enforce URL-scoping
    ->condition('status', 1)
    ->isNull('deleted_at')
    ->execute()
    ->fetchObject();

  if (!$response) {
    $can_view_all
      ? _dynamic_form_restws_send_error(404, 'Response not found.')
      : _dynamic_form_restws_send_error(403, 'You do not have permission to view this response.');
  }

  if (!$can_view_all && (int) $response->user_id !== (int) $account->uid) {
    _dynamic_form_restws_send_error(403, 'You do not have permission to view this response.');
  }

  _dynamic_form_restws_render_response_detail($response, $form, $account);
}

/* =====================================================================
   ENDPOINT: GET api/v1/responses/{response_id}
   ===================================================================== */

/**
 * Page callback: single response detail — standalone URL (response_id only).
 *
 * Derives form_id from the response row itself so callers do not need to
 * know which form the response belongs to.
 *
 * @param string $response_id  Response primary key from the URL.
 */
function dynamic_form_restws_v1_response_standalone($response_id) {
  if ($_SERVER['REQUEST_METHOD'] !== 'GET') {
    _dynamic_form_restws_send_error(405, 'Method not allowed. Use GET.');
  }

  $account = _dynamic_form_restws_authenticate();
  if (!$account) {
    _dynamic_form_restws_send_error(401, 'Valid credentials with the "access rest api" permission are required.');
  }

  $response_id = (int) $response_id;

  $response = db_select('dynamic_form_responses', 'r')
    ->fields('r')
    ->condition('r.id', $response_id)
    ->condition('r.status', 1)
    ->isNull('r.deleted_at')
    ->execute()
    ->fetchObject();

  if (!$response) {
    _dynamic_form_restws_send_error(404, 'Response not found.');
  }

  $form_id = (int) $response->form_id;

  $form = db_select('dynamic_form_forms', 'f')
    ->fields('f', array('id', 'title', 'slug'))
    ->condition('id', $form_id)
    ->isNull('deleted_at')
    ->execute()
    ->fetchObject();

  if (!$form) {
    _dynamic_form_restws_send_error(404, 'Parent form not found.');
  }

  $can_view_all = _dynamic_form_restws_can_view_responses($form_id, $account);

  if (!$can_view_all && (int) $response->user_id !== (int) $account->uid) {
    _dynamic_form_restws_send_error(403, 'You do not have permission to view this response.');
  }

  _dynamic_form_restws_render_response_detail($response, $form, $account);
}

/* =====================================================================
   PRIVATE HELPERS
   ===================================================================== */

/**
 * Renders and sends the full detail payload for a single response.
 *
 * Used by both the form-scoped and standalone response detail endpoints.
 * Access checks must be completed by the caller before invoking this.
 *
 * @param object $response  Row from dynamic_form_responses.
 * @param object $form      Row from dynamic_form_forms (id, title, slug).
 * @param object $account   Authenticated Drupal user.
 */
function _dynamic_form_restws_render_response_detail($response, $form, $account) {
  $form_id     = (int) $form->id;
  $response_id = (int) $response->id;

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
  elseif (!empty($response->guest_email)) {
    $respondent = array('type' => 'guest', 'uid' => NULL, 'name' => NULL, 'email' => $response->guest_email);
  }
  else {
    $respondent = array('type' => 'anonymous', 'uid' => NULL, 'name' => NULL, 'email' => NULL);
  }

  // Questions ordered by position.
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
      'id'           => $response_id,
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
 * Formats a form DB row into the shape returned by the forms list endpoint.
 *
 * @param object $row  Row from dynamic_form_forms.
 * @return array
 */
function _dynamic_form_restws_format_form($row) {
  static $status_labels   = array(0 => 'draft', 1 => 'published', 2 => 'closed');
  static $visibility_labels = array('p' => 'public', 'r' => 'restricted', 'm' => 'members only');

  $status = (int) $row->status;
  $vis    = $row->visibility;

  return array(
    'id'               => (int) $row->id,
    'title'            => $row->title,
    'description'      => $row->description,
    'slug'             => $row->slug,
    'status'           => $status,
    'status_label'     => isset($status_labels[$status]) ? $status_labels[$status] : 'unknown',
    'visibility'       => $vis,
    'visibility_label' => isset($visibility_labels[$vis]) ? $visibility_labels[$vis] : 'unknown',
    'opens_at'         => $row->opens_at  ? date('c', $row->opens_at)  : NULL,
    'closes_at'        => $row->closes_at ? date('c', $row->closes_at) : NULL,
    'published_at'     => $row->published_at ? date('c', $row->published_at) : NULL,
    'created_by'       => (int) $row->created_by,
    'created_at'       => date('c', $row->created_at),
    'updated_at'       => date('c', $row->updated_at),
  );
}

/**
 * Formats a section DB row into the shape used in form detail responses.
 *
 * @param object $row  Row from dynamic_form_sections.
 * @return array
 */
function _dynamic_form_restws_format_section($row) {
  return array(
    'id'          => (int) $row->id,
    'name'        => $row->name,
    'description' => $row->description,
    'position'    => (int) $row->position,
    'created_by'  => (int) $row->created_by,
    'created_at'  => date('c', $row->created_at),
    'updated_at'  => date('c', $row->updated_at),
  );
}

/**
 * Formats a question DB row (plus its pre-loaded options) for the API.
 *
 * @param object $row               Row from dynamic_form_questions.
 * @param array  $options_by_question  Options keyed by question_id.
 * @return array
 */
function _dynamic_form_restws_format_question($row, array $options_by_question) {
  $qid = (int) $row->id;
  return array(
    'id'               => $qid,
    'section_id'       => $row->section_id !== NULL ? (int) $row->section_id : NULL,
    'label'            => $row->label,
    'description'      => $row->description,
    'placeholder'      => $row->placeholder,
    'type'             => $row->type,
    'is_required'      => (bool) $row->is_required,
    'is_readonly'      => (bool) $row->is_readonly,
    'allow_filter'     => (bool) $row->allow_filter,
    'position'         => (int) $row->position,
    'options'          => isset($options_by_question[$qid]) ? $options_by_question[$qid] : array(),
    'created_by'       => (int) $row->created_by,
    'created_at'       => date('c', $row->created_at),
    'updated_at'       => date('c', $row->updated_at),
  );
}

/**
 * Parses a date query parameter into a Unix timestamp.
 *
 * Accepts a YYYY-MM-DD string (treated as UTC) or a raw Unix timestamp integer.
 * When $end_of_day is TRUE and a YYYY-MM-DD string is supplied, the timestamp
 * is set to 23:59:59 of that day so it acts as an inclusive upper bound.
 *
 * @param string $value       Raw query-string value.
 * @param bool   $end_of_day  Set to end-of-day for upper-bound date strings.
 * @return int|FALSE  Unix timestamp or FALSE on parse failure.
 */
function _dynamic_form_restws_parse_date_param($value, $end_of_day = FALSE) {
  $value = trim($value);

  // YYYY-MM-DD
  if (preg_match('/^\d{4}-\d{2}-\d{2}$/', $value)) {
    $parts = explode('-', $value);
    $ts = gmmktime(
      $end_of_day ? 23 : 0,
      $end_of_day ? 59 : 0,
      $end_of_day ? 59 : 0,
      (int) $parts[1],
      (int) $parts[2],
      (int) $parts[0]
    );
    return ($ts === FALSE) ? FALSE : $ts;
  }

  // Unix timestamp (digits only).
  if (ctype_digit($value)) {
    return (int) $value;
  }

  return FALSE;
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
