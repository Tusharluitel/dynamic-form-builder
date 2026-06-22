<?php
/**
 *
 * Variables:
 * - $questions: Array of question objects with properties:
 *               id, label (truncated), type, allow_filter.
 */
?>
<div class="dfb-filter-questions-panel">
  <h3><?php print t('Response Filters'); ?></h3>
  <p class="dfb-filter-panel-desc">
    <?php print t('Toggle which questions appear as filter options on the responses page.'); ?>
  </p>
  <?php if (empty($questions)): ?>
    <p class="dfb-filter-panel-empty">
      <?php print t('No questions yet. Add questions via the Builder.'); ?>
    </p>
  <?php else: ?>
    <?php foreach ($questions as $q): ?>
    <div class="dfb-filter-question-item">
      <div class="dfb-filter-question-info">
        <div class="dfb-filter-question-type"><?php print check_plain($q->type); ?></div>
        <div class="dfb-filter-question-label" title="<?php print check_plain($q->label); ?>">
          <?php print check_plain(truncate_utf8($q->label, 50, TRUE, TRUE)); ?>
        </div>
      </div>
      <div class="dfb-toggle-wrap">
        <input type="checkbox" class="dfb-filter-toggle-input"
          id="dfb-toggle-<?php print (int) $q->id; ?>"
          data-question-id="<?php print (int) $q->id; ?>"
          <?php if (!empty($q->allow_filter)): ?>checked="checked"<?php endif; ?>>
        <label class="dfb-toggle-label" for="dfb-toggle-<?php print (int) $q->id; ?>"></label>
      </div>
    </div>
    <?php endforeach; ?>
  <?php endif; ?>
</div>
