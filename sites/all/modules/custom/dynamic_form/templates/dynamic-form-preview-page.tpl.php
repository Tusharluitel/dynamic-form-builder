<?php
/**
 *
 * Variables:
 * - $form_record: Form DB row object (id, title, description).
 * - $back_url: Rendered HTML anchor returned by l() pointing to the builder.
 * - $body_html: Paginated question/section content HTML (or empty-state message).
 */
?>
<div class="dfp-preview-container">

  <div class="dfp-preview-banner">
    <span class="dfp-preview-badge"><?php print t('Preview Mode'); ?></span>
    <span class="dfp-preview-note"><?php print t('This is how respondents will see your form.'); ?></span>
    <?php print $back_url; ?>
  </div>

  <div class="dfp-form-card">
    <div class="dfp-form-header">
      <h1 class="dfp-form-title"><?php print check_plain($form_record->title); ?></h1>
      <?php if (!empty($form_record->description)): ?>
        <p class="dfp-form-desc"><?php print check_markup($form_record->description); ?></p>
      <?php endif; ?>
    </div>
    <?php print $body_html; ?>
  </div>

</div>
