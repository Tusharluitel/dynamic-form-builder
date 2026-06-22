<?php
/**
 *
 * Variables:
 * - $form_title:   Form title (already check_plain'd).
 * - $resp_name:    Respondent display name (already check_plain'd).
 * - $resp_mail:    Respondent email or '—' (already check_plain'd).
 * - $submitted_at: Formatted submission date string, or t('Pending').
 */
?>
<div class="dfr-detail-meta">
  <dl>
    <dt><?php print t('Form'); ?></dt>
    <dd><?php print $form_title; ?></dd>
    <dt><?php print t('Respondent'); ?></dt>
    <dd><?php print $resp_name; ?></dd>
    <dt><?php print t('Email'); ?></dt>
    <dd><?php print $resp_mail; ?></dd>
    <dt><?php print t('Submitted'); ?></dt>
    <dd><?php print $submitted_at; ?></dd>
  </dl>
</div>
