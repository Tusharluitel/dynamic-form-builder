<?php
/**
 *
 * Variables:
 * - $radar_url:     URL to the Form Radar page.
 * - $kpi_cards:     Array of stat-card variable arrays (label, value, icon, modifier).
 * - $has_trend:     (bool) Whether submission trend data for the last 30 days exists.
 * - $has_top_forms: (bool) Whether top-forms-by-responses data exists.
 */
?>
<div class="dfb-analytics-page">

  <div class="dfb-analytics-header dfb-analytics-header--split">
    <div class="dfb-analytics-header-text">
      <h2><?php print t('Analytics Overview'); ?></h2>
      <p class="dfb-dashboard-subtitle"><?php print t('Aggregated data across all forms'); ?></p>
    </div>
    <a href="<?php print $radar_url; ?>" class="dfb-radar-entry-btn"
       title="<?php print t('Open Form Radar'); ?>">
      <span class="dfb-radar-entry-icon"><i class="fa-solid fa-satellite-dish"></i></span>
      <span class="dfb-radar-entry-label">Radar</span>
    </a>
  </div>

  <div class="dfb-stats-grid">
    <?php foreach ($kpi_cards as $card): ?>
      <?php print theme('dynamic_form_analytics_stat_card', $card); ?>
    <?php endforeach; ?>
  </div>

  <div class="dfb-analytics-grid">

    <div class="dfb-chart-card dfb-chart-card--wide">
      <h3 class="dfb-chart-title"><?php print t('Submissions — Last 30 Days'); ?></h3>
      <?php if ($has_trend): ?>
        <div id="dfa-chart-global-trend" class="dfb-chart-container"></div>
      <?php else: ?>
        <div class="dfb-analytics-empty"><?php print t('No submissions in the last 30 days.'); ?></div>
      <?php endif; ?>
    </div>

    <div class="dfb-chart-card">
      <h3 class="dfb-chart-title"><?php print t('Response Status'); ?></h3>
      <div id="dfa-chart-status-donut" class="dfb-chart-container dfb-chart-container--donut"></div>
    </div>

    <div class="dfb-chart-card dfb-chart-card--wide">
      <h3 class="dfb-chart-title"><?php print t('Top Forms by Responses'); ?></h3>
      <?php if ($has_top_forms): ?>
        <div id="dfa-chart-top-forms" class="dfb-chart-container dfb-chart-container--hbar"></div>
      <?php else: ?>
        <div class="dfb-analytics-empty"><?php print t('No submitted responses yet.'); ?></div>
      <?php endif; ?>
    </div>

  </div>

  <div class="dfb-chart-card dfb-chart-card--wide dfb-wc-card">
    <div class="dfb-wc-header">
      <h3 class="dfb-chart-title"><?php print t('Tag Word Cloud'); ?></h3>
      <div class="dfb-wc-filters">
        <select id="dfa-wc-form" class="dfb-wc-select"
                aria-label="<?php print t('Filter by form'); ?>">
          <option value=""><?php print t('All Forms'); ?></option>
        </select>
        <select id="dfa-wc-question" class="dfb-wc-select" disabled
                aria-label="<?php print t('Filter by question'); ?>">
          <option value=""><?php print t('All Tag Questions'); ?></option>
        </select>
      </div>
    </div>
    <div id="dfa-chart-wordcloud" class="dfb-chart-container dfb-chart-container--wordcloud"></div>
  </div>

</div>
