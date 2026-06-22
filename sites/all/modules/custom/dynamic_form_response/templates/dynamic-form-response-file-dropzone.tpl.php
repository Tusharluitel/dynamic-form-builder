<?php
/**
 *
 * Rendered as the #markup value of the dropzone_inner child inside the
 * FAPI container for file questions. The outer container div (dfp-file-dropzone)
 * is built in PHP because it carries a conditional CSS class (dfp-file-done).
 *
 * Variables:
 * - $svg:       SVG upload-cloud icon markup.
 * - $has_file:  TRUE when a file is already selected (show $file_name, hide prompt).
 * - $file_name: Sanitised filename of an existing file selection (already check_plain'd).
 * - $meta_html: Pre-rendered file constraints HTML (type badges, size/count pills).
 */
?>
<div class="dfp-file-dropzone-inner">
  <div class="dfp-file-upload-icon"><?php print $svg; ?></div>
  <div class="dfp-file-upload-prompt">
    <span class="dfp-file-upload-btn"><?php print t('Click to upload'); ?></span>
    <span class="dfp-file-upload-or"> <?php print t('or drag and drop'); ?></span>
  </div>
  <div class="dfp-file-name"<?php print $has_file ? '' : ' style="display:none"'; ?>>
    <?php print $file_name; ?>
  </div>
  <?php print $meta_html; ?>
</div>
