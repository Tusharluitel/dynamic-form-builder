<!DOCTYPE html>
<html lang="<?php print $language->language; ?>">
<head>
  <head-placeholder token="<?php print $head_placeholder_token; ?>">
  <title><?php print $head_title; ?></title>

  <css-placeholder token="<?php print $css_placeholder_token; ?>">
  <js-placeholder token="<?php print $js_placeholder_token; ?>">
</head>

<body class="<?php print $classes; ?>" <?php print $attributes; ?>>

<?php print $page_top; ?>

<div id="page-wrapper">

  <!-- Header -->
  <header style="padding:20px; background:#2c3e50; color:white;">
    <h1>Dynamic Form Builder</h1>

    <div style="float:right;">
      <?php if (!$logged_in): ?>
        <a href="<?php print url('user/login'); ?>" style="margin-right:10px; color:white;">Login</a>
        <a href="<?php print url('user/register'); ?>" style="color:white;">Register</a>
      <?php else: ?>
        <a href="<?php print url('user'); ?>" style="color:white;">Dashboard</a>
      <?php endif; ?>
    </div>
  </header>

  <!-- Hero Section -->
  <section style="padding:60px; text-align:center; background:#ecf0f1;">
    <h2>Create Powerful Forms in Minutes</h2>
    <p>Build dynamic, flexible, and scalable forms like Google Forms — all in one place.</p>

    <?php if (!$logged_in): ?>
      <a href="<?php print url('user/register'); ?>" style="padding:10px 20px; background:#3498db; color:white; text-decoration:none; margin-right:10px;">
        Get Started
      </a>

      <a href="<?php print url('user/login'); ?>" style="padding:10px 20px; background:#2ecc71; color:white; text-decoration:none;">
        Login
      </a>
    <?php endif; ?>
  </section>

  <!-- Features -->
  <section style="padding:40px;">
    <h2 style="text-align:center;">Features</h2>

    <div style="display:flex; justify-content:space-around; margin-top:30px;">

      <div>
        <h3>🧩 Dynamic Forms</h3>
        <p>Create fully customizable forms with multiple field types.</p>
      </div>

      <div>
        <h3>📊 Response Management</h3>
        <p>Collect and analyze responses in real-time.</p>
      </div>

      <div>
        <h3>🔒 Secure File Uploads</h3>
        <p>Upload files securely using private storage.</p>
      </div>

    </div>
  </section>

  <!-- Content Region (optional Drupal blocks) -->
  <section style="padding:20px;">
    <?php print $messages; ?>
    <?php print render($page['content']); ?>
  </section>

  <!-- Footer -->
  <footer style="padding:20px; background:#2c3e50; color:white; text-align:center;">
    <p>&copy; <?php print date('Y'); ?> Dynamic Form Builder. All rights reserved.</p>
  </footer>

</div>

<?php print $page_bottom; ?>

</body>
</html>