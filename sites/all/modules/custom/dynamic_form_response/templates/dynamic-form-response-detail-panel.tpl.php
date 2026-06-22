<?php
/**
 *
 * Variables:
 * - $answered_q:        (int)  Number of questions with an answer.
 * - $total_q:           (int)  Total number of questions.
 * - $pct:               (int)  Completion percentage (0–100).
 * - $prev_url:          URL for the previous response, or FALSE if none.
 * - $next_url:          URL for the next response, or FALSE if none.
 * - $current_pos:       (int)  1-based position of this response (0 if unknown).
 * - $total_responses:   (int)  Total submitted responses for this form.
 * - $all_responses_url: URL to the full responses list.
 * - $form_builder_url:  URL to the form builder.
 */
?>
<aside class="dfr-detail-panel">

  <div class="dfr-panel-card">
    <h4 class="dfr-panel-card-title"><?php print t('Completion'); ?></h4>
    <div class="dfr-panel-stat">
      <span class="dfr-panel-stat-num"><?php print $answered_q; ?> / <?php print $total_q; ?></span>
      <span class="dfr-panel-stat-label"><?php print t('questions answered'); ?></span>
    </div>
    <div class="dfr-completion-bar-wrap">
      <div class="dfr-completion-bar" style="width:<?php print $pct; ?>%"></div>
    </div>
    <div class="dfr-completion-pct"><?php print $pct; ?>%</div>
  </div>

  <?php if ($prev_url || $next_url): ?>
    <div class="dfr-panel-card dfr-panel-nav">
      <h4 class="dfr-panel-card-title"><?php print t('Navigate'); ?></h4>
      <div class="dfr-panel-nav-btns">
        <?php if ($prev_url): ?>
          <a href="<?php print check_url($prev_url); ?>" class="dfr-nav-btn">&larr; <?php print t('Previous'); ?></a>
        <?php endif; ?>
        <?php if ($next_url): ?>
          <a href="<?php print check_url($next_url); ?>" class="dfr-nav-btn"><?php print t('Next'); ?> &rarr;</a>
        <?php endif; ?>
      </div>
      <?php if ($current_pos): ?>
        <p class="dfr-nav-position">
          <?php print t('@num of @total', array('@num' => $current_pos, '@total' => $total_responses)); ?>
        </p>
      <?php endif; ?>
    </div>
  <?php endif; ?>

  <div class="dfr-panel-card dfr-panel-actions">
    <h4 class="dfr-panel-card-title"><?php print t('Actions'); ?></h4>
    <a href="<?php print check_url($all_responses_url); ?>" class="dfr-action-btn dfr-action-btn--secondary">
      <?php print t('All Responses'); ?>
    </a>
    <a href="<?php print check_url($form_builder_url); ?>" class="dfr-action-btn dfr-action-btn--secondary">
      <?php print t('Form Builder'); ?>
    </a>
  </div>

</aside>
