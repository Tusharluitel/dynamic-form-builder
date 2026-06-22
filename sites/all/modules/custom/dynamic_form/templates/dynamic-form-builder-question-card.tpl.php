<?php
/**
 * @file
 * Template for a single question card in the form builder.
 */
$width_labels = array('half' => '&#189;', 'third' => '&#8531;', 'quarter' => '&#188;');
$q_width = (!empty($question->width) && isset($width_labels[$question->width])) ? $question->width : '';
?>
<div class="dfb-question-card" data-question-id="<?php print (int) $question->id; ?>">

  <div class="dfb-drag-handle dfb-question-drag-handle" title="<?php print t('Drag to reorder'); ?>">
    <i class="fa-solid fa-grip-vertical"></i>
  </div>

  <div class="dfb-question-content dfb-question-editable" title="<?php print t('Click to edit'); ?>">
    <div class="dfb-question-header">
      <h3 class="dfb-question-label">
        <?php print check_plain($question->label); ?>
        <?php if ($question->is_required): ?>
          <span class="dfb-req">*</span>
        <?php endif; ?>
      </h3>
      <div class="dfb-question-header-right">
        <?php if ($q_width): ?>
          <span class="dfb-question-width-badge"><?php print $width_labels[$q_width]; ?></span>
        <?php endif; ?>
        <span class="dfb-question-type-badge"><?php print check_plain($question->type); ?></span>
        <span class="dfb-edit-hint"><i class="fa-solid fa-pen"></i></span>
        <button class="dfb-question-delete-icon" data-action="open-delete-modal"
          data-entity-type="question"
          data-entity-id="<?php print (int) $question->id; ?>"
          data-entity-name="<?php print check_plain($question->label); ?>"
          title="<?php print t('Delete question'); ?>">
          <svg xmlns="http://www.w3.org/2000/svg" width="13" height="13" viewBox="0 0 24 24"
            fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
            <polyline points="3 6 5 6 21 6"/>
            <path d="M19 6l-1 14H6L5 6"/>
            <path d="M10 11v6"/>
            <path d="M14 11v6"/>
            <path d="M9 6V4h6v2"/>
          </svg>
        </button>
      </div>
    </div>
    <?php if (!empty($question->description)): ?>
      <div class="dfb-question-desc"><?php print check_plain($question->description); ?></div>
    <?php endif; ?>
  </div>

</div>
