<?php
/**
 * @file
 * Template for the file-upload dropzone inner content on the preview page.
 *
 * The outer <div class="dfp-file-dropzone"> wrapper is provided by the FAPI
 * container element in _dynamic_form_preview_build_fapi_element().
 *
 * Variables:
 *   $svg        - The upload cloud SVG markup string.
 *   $meta_html  - Rendered type badges + size/count pills (may be empty).
 */
?>
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
</div>
