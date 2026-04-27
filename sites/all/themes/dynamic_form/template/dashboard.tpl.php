<?php
/**
 * @file
 * Dashboard layout template for the Dynamic Form Builder.
 *
 * Variables:
 *   - $sidebar: Rendered sidebar navigation HTML.
 *   - $content: Rendered main content panel HTML.
 */
?>
<div class="dfb-dashboard" id="dfb-dashboard">
  <aside class="dfb-dashboard-sidebar" id="dfb-dashboard-sidebar">
    <?php print $sidebar; ?>
  </aside>
  <main class="dfb-dashboard-main" id="dfb-dashboard-main">
    <?php print $content; ?>
  </main>
</div>
