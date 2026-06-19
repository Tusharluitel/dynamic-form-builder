<?php
/**
 * Variables:
 * - $buttons:   Array of button items, each with keys:
 *                 'value'  — integer scale number.
 *                 'active' — TRUE if this button is the currently selected value.
 * - $current:   Currently selected value (int or NULL if nothing selected yet).
 * - $min_label: Optional label for the low end of the scale.
 * - $max_label: Optional label for the high end of the scale.
 */
?>
<div class="dfp-scale-wrap">
  <div class="dfp-scale-buttons">
    <?php foreach ($buttons as $btn): ?>
      <button type="button"
              class="dfp-scale-btn<?php print $btn['active'] ? ' dfp-scale-active' : ''; ?>">
        <?php print $btn['value']; ?>
      </button>
    <?php endforeach; ?>
  </div>
  <?php if ($min_label || $max_label): ?>
    <div class="dfp-scale-labels">
      <span class="dfp-scale-label dfp-scale-label-min"><?php print $min_label; ?></span>
      <span class="dfp-scale-label dfp-scale-label-max"><?php print $max_label; ?></span>
    </div>
  <?php endif; ?>
  <input type="hidden" class="dfr-scale-value"
         value="<?php print ($current !== NULL ? $current : ''); ?>">
</div>
