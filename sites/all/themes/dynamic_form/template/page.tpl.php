<!DOCTYPE html>
<html lang="<?php print $language->language; ?>">
<head>
  <?php print $head; ?>
  <title><?php print $head_title; ?></title>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <meta name="description" content="Build dynamic, customizable forms with drag-and-drop ease. Collect responses, analyze data, and collaborate — all in one platform.">
  <link rel="preconnect" href="https://fonts.googleapis.com">
  <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
  <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700;800&display=swap" rel="stylesheet">
  <?php print $styles; ?>
  <?php print $scripts; ?>
</head>

<body class="<?php print $classes; ?>" <?php print $attributes; ?>>

<?php print $page_top; 

global $user;

$roles = $user->roles;

if (in_array('admin', $roles) || in_array('super administrator', $roles)) {
  $dashboard_url = url('admin');
} else {
  $dashboard_url = url('user');
}
?>

<div id="page-wrapper">

  <!-- Navigation -->
  <nav class="dfb-nav" id="dfb-nav">
    <div class="dfb-nav-inner">
      <a href="<?php print url('<front>'); ?>" class="dfb-logo">
        <span>DFB</span>
      </a>
      <button class="dfb-nav-toggle" aria-label="Toggle menu">&#9776;</button>
      <div class="dfb-nav-links" id="dfb-nav-links">
        <?php if (!$logged_in): ?>
          <a href="<?php print url('login'); ?>" class="dfb-btn-ghost">Sign In</a>
          <a href="<?php print url('register'); ?>" class="dfb-btn-primary">Sign Up</a>
        <?php else: ?>

          <a href="<?php print $dashboard_url; ?>">Dashboard</a>
          <a href="<?php print url('user/logout'); ?>" class="dfb-btn-ghost">Log Out</a>
        <?php endif; ?>
      </div>
    </div>
  </nav>

  <?php if ($is_front): ?>
  <!-- Hero Section -->
  <section class="dfb-hero" id="hero">
    <div class="dfb-orb dfb-orb-1"></div>
    <div class="dfb-orb dfb-orb-2"></div>
    <div class="dfb-orb dfb-orb-3"></div>
    <div class="dfb-hero-content">
      <h1>Build Dynamic Forms <span class="highlight">Easily</span></h1>
      <p>Create beautiful, responsive forms in minutes. Collect responses, gain insights, and collaborate with your team — all from one powerful platform.</p>
      <?php if (!$logged_in): ?>
        <a href="<?php print url('register'); ?>" class="dfb-hero-cta">
          Get Started Free <span class="arrow">&rarr;</span>
        </a>
      <?php else: ?>
        <a href="<?php print url('user'); ?>" class="dfb-hero-cta">
          Go to Dashboard <span class="arrow">&rarr;</span>
        </a>
      <?php endif; ?>
    </div>
  </section>

  <!-- Features Section -->
  <section class="dfb-features" id="features">
    <div class="dfb-features-inner">
      <div class="dfb-features-header">
        <span class="dfb-section-label">Features</span>
        <h2>Everything you need to build great forms</h2>
        <p>Powerful tools that make form creation, customization, and analysis effortless.</p>
      </div>
      <div class="dfb-features-grid">
        <div class="dfb-feature-card dfb-animate">
          <div class="dfb-feature-icon">&#127912;</div>
          <h3>Drag &amp; Drop Builder</h3>
          <p>Create forms visually with an intuitive drag-and-drop interface. Add text fields, dropdowns, checkboxes, file uploads, and more.</p>
        </div>
        <div class="dfb-feature-card dfb-animate">
          <div class="dfb-feature-icon">&#128202;</div>
          <h3>Real-Time Analytics</h3>
          <p>Track form responses as they come in. Visualize trends, export data, and make informed decisions with built-in analytics.</p>
        </div>
        <div class="dfb-feature-card dfb-animate">
          <div class="dfb-feature-icon">&#128274;</div>
          <h3>Secure &amp; Private</h3>
          <p>Enterprise-grade security with encrypted file uploads, access controls, and GDPR-compliant data handling.</p>
        </div>
      </div>
    </div>
  </section>
  <?php endif; ?>

  <!-- Messages -->
  <?php if ($messages): ?>
    <div class="dfb-messages">
      <?php print $messages; ?>
    </div>
  <?php endif; ?>

  <!-- Main Content -->
  <div class="dfb-content-region">
    <?php print render($page['content']); ?>
  </div>

  <!-- Footer -->
  <footer class="dfb-footer">
    <p>&copy; <?php print date('Y'); ?> Dynamic Form Builder. All rights reserved.</p>
  </footer>

</div>

<?php print $page_bottom; ?>

</body>
</html>