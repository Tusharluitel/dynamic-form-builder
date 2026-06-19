<?php
/**
 *
 * Variables:
 * - $title:    The form's title (already check_plain'd).
 * - $back_url: URL to the public forms listing page.
 */
?>
<div class="dfr-thankyou">
  <div class="dfr-check-icon">&#10003;</div>
  <h2><?php print t('Thank you for your response!'); ?></h2>
  <p><?php print t('Your response to @title has been submitted successfully.',
      array('@title' => $title)); ?></p>
  <a href="<?php print $back_url; ?>" class="dfr-btn-back">
    &larr; <?php print t('Back to forms'); ?>
  </a>
</div>
