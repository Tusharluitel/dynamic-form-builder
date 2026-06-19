<?php
/**
 * Template for a single section card in the form builder.
 */
?>
<div class="dfb-section-card" data-section-id="<?php print (int) $section->id; ?>">

  <div class="dfb-section-header">
    <div class="dfb-drag-handle dfb-section-drag-handle" title="<?php print t('Drag to reorder'); ?>">
      <i class="fa-solid fa-grip-vertical"></i>
    </div>
    <div class="dfb-section-title-wrap">
      <h2 class="dfb-section-title"><?php print check_plain($section->name); ?></h2>
      <button class="dfb-section-edit-icon" data-action="inline-rename-section"
        title="<?php print t('Rename section'); ?>">
        <i class="fa-solid fa-pen"></i>
      </button>
      <button class="dfb-section-delete-icon" data-action="open-delete-modal"
        data-entity-type="section"
        data-entity-id="<?php print (int) $section->id; ?>"
        data-entity-name="<?php print check_plain($section->name); ?>"
        title="<?php print t('Delete section'); ?>">
        <svg xmlns="http://www.w3.org/2000/svg" width="14" height="14" viewBox="0 0 24 24"
          fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
          <polyline points="3 6 5 6 21 6"/>
          <path d="M19 6l-1 14H6L5 6"/>
          <path d="M10 11v6"/>
          <path d="M14 11v6"/>
          <path d="M9 6V4h6v2"/>
        </svg>
      </button>
      <?php if (!empty($section->description)): ?>
        <p class="dfb-section-desc"><?php print check_plain($section->description); ?></p>
      <?php endif; ?>
    </div>
  </div>

  <div class="dfb-questions-wrapper" data-section-id="<?php print (int) $section->id; ?>">
    <?php print $questions_html; ?>
  </div>

  <div class="dfb-section-footer">
    <button class="dfb-btn-add-question"
      data-action="open-question-modal"
      data-section-id="<?php print (int) $section->id; ?>"
      data-section-name="<?php print check_plain($section->name); ?>">
      <span class="icon">+</span> <?php print t('Add Question'); ?>
    </button>
  </div>

</div>
