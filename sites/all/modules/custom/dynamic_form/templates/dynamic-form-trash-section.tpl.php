<?php
/**
 *
 * Variables:
 * - $title:      Section heading (pre-translated, already check_plain'd).
 * - $icon:       Font Awesome icon markup string.
 * - $count:      Integer count of deleted items in this section.
 * - $table_html: Rendered theme('table') output, or empty string when no items.
 */
?>
<div class="dfb-trash-section">
  <h3 class="dfb-trash-section-title">
    <span class="dfb-trash-section-icon"><?php print $icon; ?></span>
    <?php print $title; ?>
    <span class="dfb-trash-count">(<?php print (int) $count; ?>)</span>
  </h3>
  <?php if ($table_html): ?>
    <?php print $table_html; ?>
  <?php else: ?>
    <div class="dfb-trash-empty"><p><?php print t('No deleted items.'); ?></p></div>
  <?php endif; ?>
</div>
