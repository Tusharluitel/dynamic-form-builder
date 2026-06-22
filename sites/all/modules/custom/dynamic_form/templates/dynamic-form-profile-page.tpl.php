<?php
/**
 *
 * Variables:
 * - $initials:   Single uppercase letter for the avatar (already check_plain'd).
 * - $name:       Display username (already check_plain'd).
 * - $mail:       User email address (already check_plain'd).
 * - $role_label: Human-readable role name (already check_plain'd).
 * - $edit_url:   URL to the Drupal user edit page.
 * - $stats:      Array of stat tiles, each with keys:
 *                  'icon'  — FA icon markup string.
 *                  'value' — Stat value (count or formatted date).
 *                  'label' — Stat label (pre-translated).
 */
?>
<div class="dfb-profile-page">
  <div class="dfb-profile-card">
    <div class="dfb-profile-avatar"><?php print $initials; ?></div>
    <div class="dfb-profile-info">
      <h2 class="dfb-profile-name"><?php print $name; ?></h2>
      <p class="dfb-profile-email"><?php print $mail; ?></p>
      <span class="dfb-badge dfb-badge-shared dfb-profile-role"><?php print $role_label; ?></span>
    </div>
    <div class="dfb-profile-actions">
      <a href="<?php print $edit_url; ?>" class="dfb-btn-edit-profile"><?php print t('Edit Profile'); ?></a>
    </div>
  </div>
  <div class="dfb-profile-stats">
    <?php foreach ($stats as $stat): ?>
      <div class="dfb-profile-stat-tile">
        <span class="dfb-profile-stat-icon"><?php print $stat['icon']; ?></span>
        <span class="dfb-profile-stat-value"><?php print $stat['value']; ?></span>
        <span class="dfb-profile-stat-label"><?php print $stat['label']; ?></span>
      </div>
    <?php endforeach; ?>
  </div>
</div>
