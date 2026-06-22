<?php
/**
 *
 * Variables:
 * - $form_title:     Form title (already check_plain'd).
 * - $form_id:        (int) Form ID.
 * - $responses_url:  URL to the responses list for this form.
 * - $analytics_url:  URL to the global analytics overview.
 * - $kpi_cards:      Array of stat-card variable arrays (label, value, icon, modifier).
 * - $has_trend:      (bool) Whether daily submission trend data exists.
 * - $questions_html: Pre-rendered question-breakdown HTML, or empty string.
 */
?>
<div class="dfb-analytics-page">

  <div class="dfb-analytics-header">
    <div class="dfb-analytics-header-top">
      <div>
        <h2><?php print t('Analytics: @title', array('@title' => $form_title)); ?></h2>
        <p class="dfb-dashboard-subtitle"><?php print t('Submission data and question breakdowns'); ?></p>
      </div>
      <div class="dfb-analytics-header-actions">
        <a href="<?php print $responses_url; ?>" class="dfb-btn dfb-btn-secondary">
          <?php print t('View Responses'); ?>
        </a>
        <a href="<?php print $analytics_url; ?>" class="dfb-btn dfb-btn-ghost">
          <i class="fa-solid fa-arrow-left"></i> <?php print t('All Analytics'); ?>
        </a>
      </div>
    </div>
  </div>

  <div class="dfb-stats-grid">
    <?php foreach ($kpi_cards as $card): ?>
      <?php print theme('dynamic_form_analytics_stat_card', $card); ?>
    <?php endforeach; ?>
  </div>

  <div class="dfb-analytics-grid">

    <div class="dfb-chart-card dfb-chart-card--wide">
      <h3 class="dfb-chart-title"><?php print t('Daily Submissions'); ?></h3>
      <?php if ($has_trend): ?>
        <div id="dfa-chart-form-trend" class="dfb-chart-container"></div>
      <?php else: ?>
        <div class="dfb-analytics-empty"><?php print t('No submissions recorded yet.'); ?></div>
      <?php endif; ?>
    </div>

    <div class="dfb-chart-card">
      <h3 class="dfb-chart-title"><?php print t('Completion Funnel'); ?></h3>
      <div id="dfa-chart-funnel" class="dfb-chart-container dfb-chart-container--donut"></div>
    </div>

    <div class="dfb-chart-card">
      <h3 class="dfb-chart-title"><?php print t('Respondent Type'); ?></h3>
      <div id="dfa-chart-respondent" class="dfb-chart-container dfb-chart-container--donut"></div>
    </div>

  </div>

  <div class="dfb-chart-card dfb-chart-card--wide dfb-wc-card">
    <div class="dfb-wc-header">
      <h3 class="dfb-chart-title"><?php print t('Tag Word Cloud'); ?></h3>
      <div class="dfb-wc-filters">
        <select id="dfa-wc-question-form" class="dfb-wc-select"
                aria-label="<?php print t('Filter by question'); ?>">
          <option value=""><?php print t('All Tag Questions'); ?></option>
        </select>
      </div>
    </div>
    <div id="dfa-chart-wordcloud-form" class="dfb-chart-container dfb-chart-container--wordcloud"></div>
  </div>

  <?php if ($questions_html): ?>
    <div class="dfb-analytics-questions">
      <h3 class="dfb-analytics-section-title"><?php print t('Question Breakdown'); ?></h3>
      <?php print $questions_html; ?>
    </div>
  <?php endif; ?>

</div>
