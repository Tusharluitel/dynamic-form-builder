<?php
/**
 * Variables:
 * - $question:         Question DB row (id, label, description, type, is_required).
 * - $input_html:       Pre-rendered input markup from _dynamic_form_response_render_input().
 * - $validations_json: JSON-encoded validation rules string for data attribute, or NULL.
 */
?>
<div class="dfp-question"
  data-question-id="<?php print (int) $question->id; ?>"
  data-type="<?php print check_plain($question->type); ?>"
  data-required="<?php print (int) $question->is_required; ?>"
  <?php if ($validations_json): ?>data-validations="<?php print check_plain($validations_json); ?>"<?php endif; ?>>
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
  <div class="dfr-field-error"></div>
</div>
