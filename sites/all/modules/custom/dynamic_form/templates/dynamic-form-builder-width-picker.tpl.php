<?php
/**
 * Variables:
 * - $current_width: Active width key ('full', 'half', 'third', 'quarter').
 * - $options: Array of width options, each with keys:
 *   - value: width key string
 *   - label: translated display label
 *   - cols: number of column spans to visualise
 *   - is_active: boolean
 */
?>
<div class="dfb-width-picker">
  <p class="dfb-subsection-label"><?php print t('Column Width'); ?></p>
  <div class="dfb-width-options">
    <?php foreach ($options as $opt): ?>
      <button type="button"
        class="dfb-width-btn<?php print $opt['is_active'] ? ' dfb-width-active' : ''; ?>"
        data-width="<?php print $opt['value']; ?>">
        <span class="dfb-width-icon">
          <?php for ($i = 0; $i < $opt['cols']; $i++): ?>
            <span class="<?php print $i === 0 ? 'dfb-width-col dfb-width-col-filled' : 'dfb-width-col'; ?>"></span>
          <?php endfor; ?>
        </span>
        <span class="dfb-width-label"><?php print check_plain($opt['label']); ?></span>
      </button>
    <?php endforeach; ?>
  </div>
</div>
