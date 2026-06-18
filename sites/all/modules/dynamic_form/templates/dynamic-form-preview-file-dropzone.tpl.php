<?php
/**
 * @file
 * Template for the file-upload dropzone shown on the form preview page.
 *
 * Variables:
 *   $svg        - The upload cloud SVG markup string.
 *   $meta_html  - Rendered type badges + size/count pills (may be empty).
 */
?>
<div class="dfp-file-dropzone">
  <div class="dfp-file-dropzone-inner">
    <div class="dfp-file-upload-icon">
      <?php print $svg; ?>
    </div>
    <div class="dfp-file-upload-prompt">
      <span class="dfp-file-upload-btn"><?php print t('Click to upload'); ?></span>
      <span class="dfp-file-upload-or"> <?php print t('or drag and drop'); ?></span>
    </div>
    <div class="dfp-file-name"></div>
    <?php print $meta_html; ?>
  </div><?php /* closes dfp-file-dropzone-inner; dfp-file-dropzone closed by #suffix */ ?>
