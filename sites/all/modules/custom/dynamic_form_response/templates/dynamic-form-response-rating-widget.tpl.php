<?php
/**
 * Variables:
 * - $stars:     Array of star items, each with keys:
 *                 'value'  — integer star number.
 *                 'active' — TRUE if this star should be highlighted.
 * - $current:   Currently selected rating value (int, 0 = none).
 * - $min_label: Optional label for the low end of the scale.
 * - $max_label: Optional label for the high end of the scale.
 */
?>
<div class="dfp-rating-wrap">
  <?php if ($min_label): ?>
    <span class="dfp-scale-label dfp-scale-label-min"><?php print $min_label; ?></span>
  <?php endif; ?>
  <div class="dfp-stars">
    <?php foreach ($stars as $star): ?>
      <span class="dfp-star<?php print $star['active'] ? ' dfp-star-active' : ''; ?>"
            data-value="<?php print $star['value']; ?>">&#9733;</span>
    <?php endforeach; ?>
  </div>
  <?php if ($max_label): ?>
    <span class="dfp-scale-label dfp-scale-label-max"><?php print $max_label; ?></span>
  <?php endif; ?>
  <input type="hidden" class="dfr-rating-value" value="<?php print $current; ?>">
</div>
