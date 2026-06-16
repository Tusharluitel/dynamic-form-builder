<?php
/**
 *
 * Variables:
 * - $qid:       (int) Question ID.
 * - $label:     Question label (already check_plain'd).
 * - $type:      Question type slug (already check_plain'd).
 * - $body_html: Pre-rendered answer body HTML (chart container, text samples,
 *               or empty-state notice — determined in PHP).
 */
?>
<div class="dfb-question-analytics-card" data-question-id="<?php print $qid; ?>">
  <div class="dfb-question-analytics-header">
    <span class="dfb-question-analytics-label"><?php print $label; ?></span>
    <span class="dfb-question-type-badge"><?php print $type; ?></span>
  </div>
  <?php print $body_html; ?>
</div>
