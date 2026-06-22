<?php
/**
 * @file
 * Template for the star-rating widget on the form preview page.
 *
 * Variables:
 *   $stars      - Array of integers from scale_min to scale_max.
 *   $min_label  - Optional label for the low end (already check_plain'd).
 *   $max_label  - Optional label for the high end (already check_plain'd).
 */
?>
<div class="dfp-rating-wrap">
  <?php if ($min_label): ?>
    <span class="dfp-scale-label dfp-scale-label-min"><?php print $min_label; ?></span>
  <?php endif; ?>
  <div class="dfp-stars">
    <?php foreach ($stars as $n): ?>
      <span class="dfp-star" title="<?php print $n; ?>">
        <i class="fa-solid fa-star"></i>
      </span>
    <?php endforeach; ?>
  </div>
  <?php if ($max_label): ?>
    <span class="dfp-scale-label dfp-scale-label-max"><?php print $max_label; ?></span>
  <?php endif; ?>
</div>
