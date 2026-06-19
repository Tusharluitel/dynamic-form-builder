<?php
/**
 *
 * Variables:
 * - $back_url:         URL for the "← Forms" back link.
 * - $form_id:          (int) Form primary key.
 * - $form_title:       Form title (already check_plain'd).
 * - $form_builder_url: URL for the form builder link in the breadcrumb.
 * - $has_filters:      TRUE when active filter criteria are present.
 * - $filtered_total:   (int) Number of responses matching current filters.
 * - $total_all:        (int) Total submitted responses regardless of filters.
 * - $solr_error:       TRUE when the search service returned a 503.
 */
?>
<div class="dfr-breadcrumb">
  <a href="<?php print check_url($back_url); ?>" class="dfr-back-link">&larr; <?php print t('Forms'); ?></a>
  <span class="dfr-breadcrumb-sep">/</span>
  <a href="<?php print check_url($form_builder_url); ?>" class="dfr-breadcrumb-link">
    <?php print $form_title; ?>
  </a>
</div>

<div class="dfr-responses-header">
  <h2><?php print t('Responses &mdash; @title', array('@title' => $form_title)); ?></h2>
  <?php if ($has_filters): ?>
    <span class="dfr-response-count dfr-response-count--filtered">
      <?php print format_plural($filtered_total, '1 match', '@count matches'); ?>
      <span class="dfr-response-count-of">
        <?php print t('of'); ?> <?php print (int) $total_all; ?>
      </span>
    </span>
  <?php else: ?>
    <span class="dfr-response-count">
      <?php print format_plural($total_all, '1 submitted response', '@count submitted responses'); ?>
    </span>
  <?php endif; ?>
</div>

<?php if ($solr_error): ?>
  <div class="dfr-solr-error">
    <?php print t('Search service unavailable (503). Filters cannot be applied and results are hidden until the service is restored.'); ?>
  </div>
<?php endif; ?>
