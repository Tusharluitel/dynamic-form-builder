<?php
/**
 * @file
 * Template for the linear-scale widget on the form preview page.
 *
 * Variables:
 *   $buttons    - Array of integers from scale_min to scale_max.
 *   $min_label  - Optional label for the low end (already check_plain'd).
 *   $max_label  - Optional label for the high end (already check_plain'd).
 */
?>
<div class="dfp-scale-wrap">
  <div class="dfp-scale-buttons">
    <?php foreach ($buttons as $n): ?>
      <button type="button" class="dfp-scale-btn"><?php print $n; ?></button>
    <?php endforeach; ?>
  </div>
  <?php if ($min_label || $max_label): ?>
    <div class="dfp-scale-labels">
      <span class="dfp-scale-label dfp-scale-label-min"><?php print $min_label; ?></span>
      <span class="dfp-scale-label dfp-scale-label-max"><?php print $max_label; ?></span>
    </div>
  <?php endif; ?>
</div>
