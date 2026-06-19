
<div class="dfb-builder-container" id="dfb-builder-container"
  data-form-id="<?php print (int) $form_record->id; ?>">

  <div class="dfb-builder-header-card">
    <div class="dfb-builder-header-top">
      <div class="dfb-builder-header-text">
        <h1 class="dfb-builder-title"><?php print check_plain($form_record->title); ?></h1>
        <?php if (!empty($form_record->description)): ?>
          <p class="dfb-builder-desc"><?php print check_markup($form_record->description); ?></p>
        <?php endif; ?>
      </div>
      <a href="<?php print check_url($preview_url); ?>" target="_blank"
        class="dfb-btn-preview"
        title="<?php print t('Open preview in a new tab'); ?>">
        <?php print t('Preview'); ?>
      </a>
    </div>
  </div>

  <?php print $sections_form; ?>

  <div id="dfb-question-modal" class="dfb-modal-overlay" style="display:none;">
    <div class="dfb-modal-content">
      <div class="dfb-modal-header">
        <div class="dfb-modal-header-text">
          <h2><?php print t('Add Question'); ?></h2>
          <p class="dfb-modal-header-sub"><?php print t('Choose a question type to add to this section'); ?></p>
        </div>
        <button type="button" class="dfb-modal-close" data-action="close-modal">&times;</button>
      </div>
      <?php print $question_modal_form; ?>
    </div>
  </div>

  <div id="dfb-delete-confirm-modal" class="dfb-modal-overlay" style="display:none;">
    <div class="dfb-delete-confirm-box">
      <div class="dfb-delete-confirm-icon"><i class="fa-regular fa-trash-can"></i></div>
      <p class="dfb-delete-confirm-msg">
        <?php print t('Delete'); ?> "<strong id="dfb-delete-confirm-name"></strong>"?
      </p>
      <p class="dfb-delete-confirm-sub">
        <?php print t('This will move it to Trash. You can restore it later.'); ?>
      </p>
      <div class="dfb-delete-confirm-actions">
        <button type="button" class="dfb-btn-cancel"
          data-action="close-delete-modal"><?php print t('Cancel'); ?></button>
        <button type="button" class="dfb-btn-delete-confirm"
          id="dfb-delete-confirm-btn"><?php print t('Delete'); ?></button>
      </div>
    </div>
  </div>

  <div id="dfb-edit-question-modal" class="dfb-modal-overlay" style="display:none;">
    <div class="dfb-modal-content">
      <div id="dfb-edit-form-placeholder">
        <div class="dfb-modal-header">
          <div class="dfb-modal-header-text">
            <h2><?php print t('Edit Question'); ?></h2>
          </div>
          <button type="button" class="dfb-modal-close"
            data-action="close-edit-modal">&times;</button>
        </div>
        <div class="dfb-edit-form-loading">
          <p><?php print t('Loading...'); ?></p>
        </div>
      </div>
    </div>
  </div>

</div>
