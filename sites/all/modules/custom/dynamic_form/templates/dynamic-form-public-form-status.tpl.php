<?php
/**
 *
 * Variables:
 * - $message:  Status message string (already translated).
 * - $back_url: URL to the public forms listing page.
 */
?>
<div class="dfb-form-view">
  <div class="dfb-form-view-body">
    <p class="dfb-coming-soon-msg"><?php print $message; ?></p>
    <a href="<?php print $back_url; ?>" class="dfb-btn-back">&larr; <?php print t('Back to public forms'); ?></a>
  </div>
</div>
