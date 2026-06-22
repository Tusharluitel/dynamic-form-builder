<?php
/**
 *
 * Variables:
 * - $label:      Question label (already check_plain'd).
 * - $value_html: Pre-rendered answer value HTML (empty notice, file link,
 *                comma list, rich text, or plain text — determined in PHP).
 */
?>
<div class="dfr-answer-card">
  <div class="dfr-answer-question"><?php print $label; ?></div>
  <?php print $value_html; ?>
</div>
