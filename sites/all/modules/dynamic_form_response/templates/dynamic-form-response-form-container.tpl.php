<?php
/**

 *
 * Variables:
 * - $form_id:         (int)  Form record ID.
 * - $token:           CSRF token (already check_plain'd).
 * - $save_url:        AJAX save endpoint URL (already check_url'd).
 * - $submit_url:      AJAX submit endpoint URL (already check_url'd).
 * - $upload_url:      AJAX file-upload endpoint URL (already check_url'd).
 * - $thanks_url:      Thank-you page URL (already check_url'd).
 * - $guest_email_url: AJAX guest-email endpoint URL (already check_url'd).
 * - $total:           (int)  Total number of steps.
 * - $is_anonymous:    '1' for anonymous visitors, '0' for logged-in users.
 * - $response_id:     (int)  Existing draft response ID (0 if none).
 * - $has_progress:    TRUE when there is more than one step.
 * - $inner_html:      Pre-rendered steps HTML (dfr-step divs).
 */
?>
<div class="dfp-form-card">
  <div class="dfr-form-container"
       data-form-id="<?php print $form_id; ?>"
       data-token="<?php print $token; ?>"
       data-save-url="<?php print $save_url; ?>"
       data-submit-url="<?php print $submit_url; ?>"
       data-upload-url="<?php print $upload_url; ?>"
       data-thankyou-url="<?php print $thanks_url; ?>"
       data-total-steps="<?php print $total; ?>"
       data-is-anonymous="<?php print $is_anonymous; ?>"
       data-guest-email-url="<?php print $guest_email_url; ?>">

    <input type="hidden" id="dfr-response-id" value="<?php print $response_id; ?>">

    <?php if ($has_progress): ?>
      <div class="dfr-progress-wrap"><div class="dfr-progress-fill"></div></div>
      <div class="dfr-progress-text"></div>
    <?php endif; ?>

    <?php print $inner_html; ?>

  </div>
</div>
