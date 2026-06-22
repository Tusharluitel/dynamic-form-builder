<?php
/**
 *
 * No variables — modal content is populated dynamically by dashboard-delete.js
 * using data attributes from the triggering element.
 */
?>
<div id="dfb-delete-modal" role="alertdialog" aria-modal="true"
  aria-labelledby="dfb-delete-modal-title" style="display:none;">
  <div class="dfb-delete-modal-dialog">
    <span class="dfb-delete-modal-icon"><i class="fa-regular fa-trash-can"></i></span>
    <h3 class="dfb-delete-modal-title" id="dfb-delete-modal-title">
      <?php print t('Delete this'); ?> <span class="dfb-delete-modal-type"></span>?
    </h3>
    <p class="dfb-delete-modal-body dfb-msg-soft">
      <?php print t('You are about to delete'); ?>
      <strong class="dfb-delete-modal-name"></strong>.
      <?php print t('The item will be moved to Trash and can be restored later.'); ?>
    </p>
    <p class="dfb-delete-modal-body dfb-msg-perma" style="display:none;">
      <?php print t('You are about to permanently delete'); ?>
      <strong class="dfb-delete-modal-name"></strong>.
      <?php print t('This cannot be undone. All associated data will be permanently removed.'); ?>
    </p>
    <div class="dfb-delete-modal-actions">
      <button class="dfb-delete-confirm-btn" type="button"><?php print t('Delete'); ?></button>
      <button class="dfb-delete-cancel-btn" type="button"><?php print t('Cancel'); ?></button>
    </div>
  </div>
</div>
