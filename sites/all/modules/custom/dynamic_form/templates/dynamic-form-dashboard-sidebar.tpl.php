<?php
/**
 *
 * Variables:
 * - $items:      Array of prepared nav items, each with keys:
 *                'classes', 'url', 'icon', 'label'.
 * - $create_url: URL for the "New Form" action button.
 */
?>
<nav class="dfb-sidebar-nav">
  <div class="dfb-sidebar-header">
    <span class="dfb-sidebar-icon"><i class="fa-solid fa-gear"></i></span>
    <span class="dfb-sidebar-title"><?php print t('Manage'); ?></span>
  </div>
  <ul class="dfb-sidebar-menu">
    <?php foreach ($items as $item): ?>
    <li class="<?php print $item['classes']; ?>">
      <a href="<?php print $item['url']; ?>">
        <span class="dfb-sidebar-item-icon"><?php print $item['icon']; ?></span>
        <span class="dfb-sidebar-item-label"><?php print $item['label']; ?></span>
      </a>
    </li>
    <?php endforeach; ?>
  </ul>
  <div class="dfb-sidebar-action">
    <a href="<?php print $create_url; ?>" class="dfb-sidebar-create-btn">
      <span>+</span> <?php print t('New Form'); ?>
    </a>
  </div>
</nav>
