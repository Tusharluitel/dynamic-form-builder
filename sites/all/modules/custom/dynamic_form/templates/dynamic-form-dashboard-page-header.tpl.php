<?php
/**
 *
 * Variables:
 * - $title:        Page heading text (pre-translated).
 * - $subtitle:     Subtitle text (pre-translated), or empty string.
 * - $with_row:     TRUE to wrap content in dfb-dashboard-header-row (for
 *                  pages with an action button or the two-column edit layout).
 * - $action_url:   URL for the optional header action button, or NULL.
 * - $action_label: Label for the action button (pre-translated), or NULL.
 */
?>
<div class="dfb-dashboard-header">
  <?php if ($with_row): ?>
  <div class="dfb-dashboard-header-row">
    <div>
      <h2><?php print $title; ?></h2>
      <?php if ($subtitle): ?>
        <p class="dfb-dashboard-subtitle"><?php print $subtitle; ?></p>
      <?php endif; ?>
    </div>
    <?php if ($action_url): ?>
      <a href="<?php print $action_url; ?>" class="dfb-header-action-btn"><?php print $action_label; ?></a>
    <?php endif; ?>
  </div>
  <?php else: ?>
  <h2><?php print $title; ?></h2>
  <?php if ($subtitle): ?>
    <p class="dfb-dashboard-subtitle"><?php print $subtitle; ?></p>
  <?php endif; ?>
  <?php endif; ?>
</div>
