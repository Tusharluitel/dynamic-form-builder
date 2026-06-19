<?php
/**
 *
 * Variables:
 * - $is_first:  TRUE when this is the first step (hide Prev button).
 * - $is_last:   TRUE when this is the final step (show Submit instead of Next).
 * - $prev_step: Step number to go back to (current step - 1).
 * - $next_step: Step number to advance to (current step + 1).
 */
?>
<div class="dfr-step-nav">
  <?php if (!$is_first): ?>
    <button type="button" class="dfr-btn-prev" data-step="<?php print $prev_step; ?>">
      <?php print t('&larr; Previous'); ?>
    </button>
  <?php else: ?>
    <span class="dfr-nav-spacer"></span>
  <?php endif; ?>
  <?php if ($is_last): ?>
    <button type="button" class="dfr-btn-submit"><?php print t('Submit'); ?></button>
  <?php else: ?>
    <button type="button" class="dfr-btn-next" data-step="<?php print $next_step; ?>">
      <?php print t('Next &rarr;'); ?>
    </button>
  <?php endif; ?>
</div>
