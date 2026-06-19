<?php
/**
 *
 * Variables:
 * - $icon:         Icon markup (e.g. '<i class="fa-regular fa-file-lines"></i>').
 * - $message:      Already-translated message string.
 * - $action_url:   (optional) URL for the call-to-action link.
 * - $action_label: (optional) Label for the call-to-action link.
 */
?>
<div class="dfb-empty-state">
  <div class="dfb-empty-icon"><?php print $icon; ?></div>
  <p><?php print $message; ?></p>
  <?php if ($action_url && $action_label): ?>
    <a href="<?php print check_url($action_url); ?>" class="dfb-empty-action">
      <?php print $action_label; ?>
    </a>
  <?php endif; ?>
</div>
