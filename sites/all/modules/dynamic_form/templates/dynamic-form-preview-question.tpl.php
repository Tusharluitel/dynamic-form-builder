<?php
/**
 *
 * Variables:
 * - $question: Question DB row object (id, label, description, type, is_required).
 * - $input_html: Pre-rendered input markup from theme_dynamic_form_preview_input().
 */
?>
<div class="dfp-question" data-type="<?php print check_plain($question->type); ?>">
  <div class="dfp-question-label">
    <?php print check_plain($question->label); ?>
    <?php if ($question->is_required): ?>
      <span class="dfp-req" title="<?php print t('Required'); ?>">*</span>
    <?php endif; ?>
  </div>
  <?php if (!empty($question->description)): ?>
    <p class="dfp-help-text"><?php print check_plain($question->description); ?></p>
  <?php endif; ?>
  <div class="dfp-question-input"><?php print $input_html; ?></div>
</div>
