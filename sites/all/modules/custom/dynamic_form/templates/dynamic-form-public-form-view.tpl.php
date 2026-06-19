<?php
/**
 *
 * Variables:
 * - $title:          Form title (already check_plain'd).
 * - $description:    Form description with nl2br applied, or empty string.
 * - $question_label: Pre-formatted question count string (via format_plural).
 * - $response_label: Pre-formatted response count string (via format_plural).
 * - $form_body:      Rendered response form HTML, or fallback message.
 */
?>
<div class="dfb-form-view">
  <div class="dfb-form-view-header">
    <h1><?php print $title; ?></h1>
    <?php if ($description): ?>
      <p class="dfb-form-view-desc"><?php print $description; ?></p>
    <?php endif; ?>
    <div class="dfb-form-view-meta">
      <span><?php print $question_label; ?></span>
      <span class="dfb-dot">&middot;</span>
      <span><?php print $response_label; ?></span>
    </div>
  </div>
  <div class="dfb-form-view-body">
    <?php print $form_body; ?>
  </div>
</div>
