<?php
/**
 *
 * Variables:
 * - $label:    Card label string (e.g. "Submitted").
 * - $value:    Card value string (e.g. "42" or "78%").
 * - $icon:     Pre-rendered icon HTML (Font Awesome <i> tag).
 * - $modifier: BEM modifier for color variant: primary | success | accent | warning.
 */
?>
<div class="dfb-stat-card dfb-stat-<?php print $modifier; ?>">
  <div class="dfb-stat-icon"><?php print $icon; ?></div>
  <div class="dfb-stat-body">
    <div class="dfb-stat-value"><?php print $value; ?></div>
    <div class="dfb-stat-label"><?php print $label; ?></div>
  </div>
</div>
