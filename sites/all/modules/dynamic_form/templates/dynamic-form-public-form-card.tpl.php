<?php
/**
 *
 * Variables:
 * - $title:          Form title (already check_plain'd).
 * - $url:            URL to the form view page (already check_url'd).
 * - $desc:           Truncated description (already check_plain'd), or empty string.
 * - $response_label: Pre-formatted response count string (via format_plural).
 * - $closes_at:      Formatted closing date string, or NULL if no expiry.
 */
?>
<div class="dfb-public-form-card">
  <div class="dfb-public-form-card-body">
    <h3><a href="<?php print $url; ?>"><?php print $title; ?></a></h3>
    <?php if ($desc): ?>
      <p class="dfb-public-form-card-desc"><?php print $desc; ?></p>
    <?php endif; ?>
    <div class="dfb-public-form-card-meta">
      <span><?php print $response_label; ?></span>
      <?php if ($closes_at): ?>
        <span class="dfb-dot">&middot;</span>
        <span class="dfb-closes-at"><?php print t('Closes at: @date', array('@date' => $closes_at)); ?></span>
      <?php endif; ?>
    </div>
  </div>
  <div class="dfb-public-form-card-footer">
    <a href="<?php print $url; ?>" class="dfb-public-form-btn"><?php print t('View Form'); ?></a>
  </div>
</div>
