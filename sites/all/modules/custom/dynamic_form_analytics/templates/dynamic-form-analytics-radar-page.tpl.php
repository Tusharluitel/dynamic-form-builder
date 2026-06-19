<?php
/**
 *
 * The filter controls (visibility toggles, ring toggles, completion slider,
 * date range) are entirely client-side: JS reads them via event listeners and
 * re-renders the D3 radar chart without a page reload.
 *
 * Variables:
 * - $back_url: URL to the global analytics overview page.
 */
?>
<div class="dfb-analytics-page">

  <div class="dfb-analytics-header dfb-analytics-header--split">
    <div class="dfb-analytics-header-text">
      <h2><?php print t('Form Radar'); ?></h2>
      <p class="dfb-dashboard-subtitle">
        <?php print t('Dot position: segment = visibility, ring = submission volume. Dot color = completion rate.'); ?>
      </p>
    </div>
    <a href="<?php print $back_url; ?>" class="dfb-btn dfb-btn-ghost">
      &larr; <?php print t('Back to Analytics'); ?>
    </a>
  </div>

  <div id="dfa-radar-filters" class="dfa-radar-filters">

    <div class="dfa-filter-group">
      <span class="dfa-filter-label"><?php print t('Visibility'); ?></span>
      <div class="dfa-filter-toggles">
        <button class="dfa-filter-toggle active" data-filter="visibility" data-key="p"
                style="--toggle-color:#4f6ef7"><?php print t('Public'); ?></button>
        <button class="dfa-filter-toggle active" data-filter="visibility" data-key="r"
                style="--toggle-color:#f59e0b"><?php print t('Restricted'); ?></button>
        <button class="dfa-filter-toggle active" data-filter="visibility" data-key="m"
                style="--toggle-color:#8b5cf6"><?php print t('Members Only'); ?></button>
      </div>
    </div>

    <div class="dfa-filter-group">
      <span class="dfa-filter-label"><?php print t('Volume Ring'); ?></span>
      <div class="dfa-filter-toggles">
        <button class="dfa-filter-toggle active" data-filter="ring" data-key="Emerging"><?php print t('Emerging'); ?></button>
        <button class="dfa-filter-toggle active" data-filter="ring" data-key="Growing"><?php print t('Growing'); ?></button>
        <button class="dfa-filter-toggle active" data-filter="ring" data-key="Active"><?php print t('Active'); ?></button>
        <button class="dfa-filter-toggle active" data-filter="ring" data-key="Popular"><?php print t('Popular'); ?></button>
      </div>
    </div>

    <div class="dfa-filter-group">
      <span class="dfa-filter-label"><?php print t('Min Completion'); ?></span>
      <div class="dfa-filter-range">
        <input type="range" id="dfa-completion-min" min="0" max="100" value="0" step="5">
        <span id="dfa-completion-val">0%</span>
      </div>
    </div>

    <div class="dfa-filter-group">
      <span class="dfa-filter-label"><?php print t('Response Date'); ?></span>
      <div class="dfa-filter-date-group">
        <input type="date" id="dfa-date-from" class="dfa-date-input">
        <span class="dfa-date-sep"><?php print t('to'); ?></span>
        <input type="date" id="dfa-date-to" class="dfa-date-input">
        <button id="dfa-date-apply" class="dfa-filter-apply-btn"><?php print t('Apply'); ?></button>
        <button id="dfa-date-clear" class="dfa-filter-clear-btn"><?php print t('Clear'); ?></button>
      </div>
    </div>

  </div>

  <div class="dfb-chart-card">
    <div id="dfa-chart-radar" class="dfb-chart-container dfb-chart-container--radar"></div>
  </div>

</div>
