<!DOCTYPE html>
<html lang="<?php print $language->language; ?>">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title><?php print $head_title; ?></title>
  <?php print $head; ?>
  <?php print $styles; ?>
  <style>
    *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }

    body {
      font-family: 'Inter', -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
      background: #0f172a;
      color: #f8fafc;
      min-height: 100vh;
      display: flex;
      flex-direction: column;
    }

    .dfb-err-nav {
      padding: 1.25rem 2rem;
      display: flex;
      align-items: center;
    }

    .dfb-err-logo {
      font-size: 1.1rem;
      font-weight: 700;
      color: #f8fafc;
      text-decoration: none;
      background: linear-gradient(135deg, #6366f1, #06b6d4);
      -webkit-background-clip: text;
      -webkit-text-fill-color: transparent;
      background-clip: text;
    }

    .dfb-err-main {
      flex: 1;
      display: flex;
      align-items: center;
      justify-content: center;
      padding: 3rem 1.5rem;
    }

    .dfb-err-card {
      background: #1e293b;
      border: 1px solid #334155;
      border-radius: 20px;
      padding: 3rem 2.5rem;
      max-width: 520px;
      width: 100%;
      text-align: center;
      box-shadow: 0 12px 40px rgba(0, 0, 0, .4);
    }

    .dfb-err-icon {
      width: 72px;
      height: 72px;
      background: linear-gradient(135deg, rgba(99,102,241,.15), rgba(6,182,212,.15));
      border: 1px solid rgba(99,102,241,.3);
      border-radius: 50%;
      display: flex;
      align-items: center;
      justify-content: center;
      margin: 0 auto 1.75rem;
    }

    .dfb-err-icon svg {
      width: 36px;
      height: 36px;
      stroke: #818cf8;
      fill: none;
      stroke-width: 1.75;
      stroke-linecap: round;
      stroke-linejoin: round;
    }

    .dfb-err-code {
      font-size: 0.75rem;
      font-weight: 600;
      letter-spacing: .12em;
      text-transform: uppercase;
      color: #818cf8;
      margin-bottom: .6rem;
    }

    .dfb-err-title {
      font-size: 1.6rem;
      font-weight: 700;
      color: #f8fafc;
      margin-bottom: .85rem;
      line-height: 1.3;
    }

    .dfb-err-desc {
      font-size: 0.95rem;
      color: #94a3b8;
      line-height: 1.65;
      margin-bottom: 2rem;
    }

    .dfb-err-actions {
      display: flex;
      gap: .75rem;
      justify-content: center;
      flex-wrap: wrap;
    }

    .dfb-err-btn-primary {
      display: inline-flex;
      align-items: center;
      gap: .4rem;
      padding: .65rem 1.4rem;
      background: linear-gradient(135deg, #6366f1, #4f46e5);
      color: #fff;
      font-size: 0.9rem;
      font-weight: 600;
      border-radius: 9999px;
      text-decoration: none;
      transition: opacity .2s;
    }
    .dfb-err-btn-primary:hover { opacity: .88; }

    .dfb-err-btn-ghost {
      display: inline-flex;
      align-items: center;
      gap: .4rem;
      padding: .65rem 1.4rem;
      background: transparent;
      border: 1px solid #475569;
      color: #cbd5e1;
      font-size: 0.9rem;
      font-weight: 500;
      border-radius: 9999px;
      text-decoration: none;
      cursor: pointer;
      font-family: inherit;
      transition: border-color .2s, color .2s;
    }
    .dfb-err-btn-ghost:hover { border-color: #818cf8; color: #f8fafc; }

    .dfb-err-messages {
      margin-top: 1.5rem;
      text-align: left;
    }
    .dfb-err-messages .messages {
      padding: .85rem 1rem;
      border-radius: 10px;
      font-size: .85rem;
      background: rgba(239,68,68,.1);
      border: 1px solid rgba(239,68,68,.3);
      color: #fca5a5;
    }

    .dfb-err-footer {
      padding: 1.25rem;
      text-align: center;
      font-size: .8rem;
      color: #475569;
    }
  </style>
</head>
<body>

  <nav class="dfb-err-nav">
    <a href="<?php print $base_path; ?>" class="dfb-err-logo">DFB</a>
  </nav>

  <main class="dfb-err-main">
    <div class="dfb-err-card">

      <div class="dfb-err-icon">
        <svg viewBox="0 0 24 24">
          <circle cx="12" cy="12" r="10"/>
          <line x1="12" y1="8" x2="12" y2="12"/>
          <line x1="12" y1="16" x2="12.01" y2="16"/>
        </svg>
      </div>

      <p class="dfb-err-code">500 &mdash; Server Error</p>
      <h1 class="dfb-err-title">Something went wrong</h1>
      <p class="dfb-err-desc">
        We ran into an unexpected problem on our end.<br>
        This has been logged and our team is on it. Please try again in a moment.
      </p>

      <div class="dfb-err-actions">
        <a href="<?php print $base_path; ?>" class="dfb-err-btn-primary">
          &larr; Go Home
        </a>
        <button class="dfb-err-btn-ghost" onclick="window.location.reload()">
          Try Again
        </button>
      </div>

      <?php if (!empty($messages)): ?>
        <div class="dfb-err-messages">
          <?php print $messages; ?>
        </div>
      <?php endif; ?>

    </div>
  </main>

  <footer class="dfb-err-footer">
    &copy; <?php print date('Y'); ?> Dynamic Form Builder. All rights reserved.
  </footer>

</body>
</html>
