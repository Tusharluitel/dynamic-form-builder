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
  <link rel="stylesheet" href="<?php print $base_path; ?>sites/all/libraries/fontawesome/css/all.min.css">
  <link rel="stylesheet" href="<?php print $base_path; ?>sites/all/modules/custom/dynamic_form/css/toast.css">
  <?php print $styles; ?>
  <?php print $scripts; ?>
</head>

<body class="<?php print $classes; ?>" <?php print $attributes; ?>>

<?php print $page_top; ?>

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

          <?php if (user_access('view form analytics')): ?>
            <a href="<?php print url('dashboard/analytics'); ?>">Dashboard</a>
          <?php else: ?>
            <a href="<?php print url('dashboard/forms'); ?>">Dashboard</a>
          <?php endif; ?>
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
        <a href="<?php print url('dashboard'); ?>" class="dfb-hero-cta">
          Go to Dashboard <span class="arrow">&rarr;</span>
        </a>
      <?php endif; ?>
    </div>
  </section>

  <!-- Public / Accessible Forms Section -->
  <?php if (!empty($front_forms)): ?>
  <section class="dfb-front-forms" id="public-forms">
    <div class="dfb-front-forms-inner">
      <div class="dfb-front-forms-header">
        <span class="dfb-section-label"><?php print $logged_in ? t('Forms') : t('Explore'); ?></span>
        <h2><?php print check_plain($front_forms_title); ?></h2>
        <p><?php print check_plain($front_forms_subtitle); ?></p>
      </div>
      <div class="dfb-public-forms-grid">
        <?php foreach ($front_forms as $front_form): ?>
          <?php print _dynamic_form_render_public_form_card($front_form); ?>
        <?php endforeach; ?>
      </div>
      <?php if ($front_forms_total > 0): ?>
      <div class="dfb-front-forms-footer">
        <a href="<?php print $front_forms_see_more; ?>" class="dfb-see-more-btn">
          <?php print $front_forms_see_label; ?> &rarr;
        </a>
      </div>
      <?php endif; ?>
    </div>
  </section>
  <?php endif; ?>
  <?php endif; ?>

  <!-- Messages -->
  <?php if ($messages): ?>
    <div class="dfb-messages">
      <?php print $messages; ?>
    </div>
  <?php endif; ?>

  <!-- Main Content (skip on front page — content is in custom sections above) -->
  <?php if (!$is_front): ?>
  <div class="dfb-content-region">
    <?php print render($page['content']); ?>
  </div>
  <?php endif; ?>

  <!-- Footer -->
  <footer class="dfb-footer">
    <p>&copy; <?php print date('Y'); ?> Dynamic Form Builder. All rights reserved.</p>
  </footer>

</div>

<?php print $page_bottom; ?>
<script src="<?php print $base_path; ?>sites/all/modules/custom/dynamic_form/js/toast.js"></script>
</body>
</html>