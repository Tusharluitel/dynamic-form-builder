<?php
/**
 *
 * Variables:
 * - $cards:  Array of pre-rendered form card strings.
 * - $pager:  Rendered pager markup (empty string when no pager needed).
 */
?>
<div class="dfb-public-listing">
  <div class="dfb-public-listing-header">
    <h1><?php print t('Public Forms'); ?></h1>
    <p><?php print t('Browse and respond to publicly available forms.'); ?></p>
  </div>
  <?php if (empty($cards)): ?>
    <p class="dfb-empty-note"><?php print t('No public forms are available at this time.'); ?></p>
  <?php else: ?>
    <div class="dfb-public-forms-grid dfb-public-forms-grid--list">
      <?php foreach ($cards as $card): ?>
        <?php print $card; ?>
      <?php endforeach; ?>
    </div>
    <?php print $pager; ?>
  <?php endif; ?>
</div>
