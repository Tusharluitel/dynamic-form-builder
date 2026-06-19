<?php
/**
 *
 * Variables:
 * - $section: Section DB row object (id, name, description).
 *
 * Note: the wrapping <div class="dfp-section"> is opened and closed by the
 * page callback's loop to manage section boundaries correctly.
 */
?>
<div class="dfp-section-header">
  <h2 class="dfp-section-title"><?php print check_plain($section->name); ?></h2>
  <?php if (!empty($section->description)): ?>
    <p class="dfp-section-desc"><?php print check_plain($section->description); ?></p>
  <?php endif; ?>
</div>
