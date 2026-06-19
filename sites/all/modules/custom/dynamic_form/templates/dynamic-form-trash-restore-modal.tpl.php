<?php
/**
 *
 * No variables — modal content is populated dynamically by dashboard-delete.js
 * using data attributes from the triggering element.
 */
?>
<div id="dfb-restore-modal" role="alertdialog" aria-modal="true"
  aria-labelledby="dfb-restore-modal-title" style="display:none;">
  <div class="dfb-delete-modal-dialog">
    <span class="dfb-delete-modal-icon"><i class="fa-solid fa-rotate-left"></i></span>
    <h3 class="dfb-delete-modal-title" id="dfb-restore-modal-title">
      <?php print t('Restore this'); ?> <span class="dfb-restore-modal-type"></span>?
    </h3>
    <p class="dfb-delete-modal-body">
      <?php print t('You are about to restore'); ?>
      <strong class="dfb-restore-modal-name"></strong>.
      <?php print t('The item will become active again.'); ?>
    </p>
    <div class="dfb-delete-modal-actions">
      <button class="dfb-restore-confirm-btn" type="button"><?php print t('Restore'); ?></button>
      <button class="dfb-restore-cancel-btn"  type="button"><?php print t('Cancel'); ?></button>
    </div>
  </div>
</div>
