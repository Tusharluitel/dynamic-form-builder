/**
 * @file
 * Dashboard delete-confirmation modal.
 *
 * Intercepts .dfb-delete-trigger clicks, shows a modal, and on confirm
 * sends a POST AJAX request to soft-delete the entity without a page redirect.
 * Requires DFBToast (toast.js) to be loaded on the same page.
 */
(function ($, Drupal) {

  'use strict';

  Drupal.behaviors.dfbDashboardDelete = {
    attach: function (context) {
      var $modal = $('#dfb-delete-modal');
      if (!$modal.length) { return; }

      // -----------------------------------------------------------------
      // Open: intercept .dfb-delete-trigger clicks
      // -----------------------------------------------------------------
      $(context).find('.dfb-delete-trigger').once('dfb-delete-open', function () {
        $(this).bind('click', function (e) {
          e.preventDefault();
          e.stopPropagation();

          var $trigger   = $(this);
          var entityType = $trigger.data('entity-type');
          var entityId   = $trigger.data('entity-id');
          var entityName = $trigger.data('entity-name');

          // Populate text inside the modal.
          $modal.find('.dfb-delete-modal-type').text(entityType);
          $modal.find('.dfb-delete-modal-name').text(entityName);

          // Store context so the confirm handler knows what to delete.
          $modal
            .data('entity-type', entityType)
            .data('entity-id',   entityId)
            .data('target-row',  $trigger.closest('tr'));

          // Reset button to its default state before opening.
          $modal.find('.dfb-delete-confirm-btn')
            .removeAttr('disabled')
            .text(Drupal.t('Delete'));

          // Fade in the modal (display:flex is needed for centering).
          $modal.css({ display: 'flex', opacity: 0 }).animate({ opacity: 1 }, 150);
        });
      });

      // -----------------------------------------------------------------
      // Init: close handlers and confirm button (attached once per modal)
      // -----------------------------------------------------------------
      $modal.once('dfb-delete-modal-init', function () {
        var $m = $(this);

        // Cancel button closes the modal.
        $m.find('.dfb-delete-cancel-btn').bind('click', function () {
          _close($m);
        });

        // Clicking the dark backdrop (but not the dialog) closes the modal.
        $m.bind('click', function (e) {
          if ($(e.target).is($m)) { _close($m); }
        });

        // Confirm: fire AJAX delete, then remove the row on success.
        $m.find('.dfb-delete-confirm-btn').bind('click', function () {
          var $btn = $(this);
          var type = $m.data('entity-type');
          var id   = $m.data('entity-id');
          var $row = $m.data('target-row');
          var url  = Drupal.settings.basePath + 'dynamic-form/ajax/delete/' + type + '/' + id;

          $btn.attr('disabled', 'disabled').text(Drupal.t('Deleting…'));

          $.ajax({
            url:      url,
            type:     'POST',
            dataType: 'json',
            success: function (res) {
              if (res && res.status === 'ok') {
                _close($m);
                // Fade out and remove the table row.
                if ($row && $row.length) {
                  $row.animate({ opacity: 0 }, 250, function () { $row.remove(); });
                }
                if (window.DFBToast) { DFBToast.success(res.message); }
              }
              else {
                $btn.removeAttr('disabled').text(Drupal.t('Delete'));
                if (window.DFBToast) {
                  DFBToast.error((res && res.message) || Drupal.t('Could not delete item.'));
                }
              }
            },
            error: function () {
              $btn.removeAttr('disabled').text(Drupal.t('Delete'));
              if (window.DFBToast) {
                DFBToast.error(Drupal.t('An error occurred. Please try again.'));
              }
            }
          });
        });
      });

      // -----------------------------------------------------------------
      // ESC key: close when modal is visible (attached once to document)
      // -----------------------------------------------------------------
      $(document).once('dfb-delete-esc', function () {
        $(this).bind('keydown', function (e) {
          if ((e.key === 'Escape' || e.keyCode === 27) && $('#dfb-delete-modal').is(':visible')) {
            _close($('#dfb-delete-modal'));
          }
        });
      });
    }
  };

  // -----------------------------------------------------------------
  // Restore modal: intercept .dfb-restore-trigger clicks
  // -----------------------------------------------------------------
  Drupal.behaviors.dfbDashboardRestore = {
    attach: function (context) {
      var $modal = $('#dfb-restore-modal');
      if (!$modal.length) { return; }

      $(context).find('.dfb-restore-trigger').once('dfb-restore-open', function () {
        $(this).bind('click', function (e) {
          e.preventDefault();
          e.stopPropagation();

          var $trigger   = $(this);
          var entityType = $trigger.data('entity-type');
          var entityId   = $trigger.data('entity-id');
          var entityName = $trigger.data('entity-name');

          $modal.find('.dfb-restore-modal-type').text(entityType);
          $modal.find('.dfb-restore-modal-name').text(entityName);

          $modal
            .data('entity-type', entityType)
            .data('entity-id',   entityId)
            .data('target-row',  $trigger.closest('tr'));

          $modal.find('.dfb-restore-confirm-btn')
            .removeAttr('disabled')
            .text(Drupal.t('Restore'));

          $modal.css({ display: 'flex', opacity: 0 }).animate({ opacity: 1 }, 150);
        });
      });

      $modal.once('dfb-restore-modal-init', function () {
        var $m = $(this);

        $m.find('.dfb-restore-cancel-btn').bind('click', function () {
          _close($m);
        });

        $m.bind('click', function (e) {
          if ($(e.target).is($m)) { _close($m); }
        });

        $m.find('.dfb-restore-confirm-btn').bind('click', function () {
          var $btn = $(this);
          var type = $m.data('entity-type');
          var id   = $m.data('entity-id');
          var $row = $m.data('target-row');
          var url  = Drupal.settings.basePath + 'dynamic-form/ajax/trash/' + type + '/' + id + '/restore';

          $btn.attr('disabled', 'disabled').text(Drupal.t('Restoring…'));

          $.ajax({
            url:      url,
            type:     'POST',
            dataType: 'json',
            success: function (res) {
              if (res && res.status === 'ok') {
                _close($m);
                if ($row && $row.length) {
                  $row.animate({ opacity: 0 }, 250, function () { $row.remove(); });
                }
                if (window.DFBToast) { DFBToast.success(res.message); }
              }
              else {
                $btn.removeAttr('disabled').text(Drupal.t('Restore'));
                if (window.DFBToast) {
                  DFBToast.error((res && res.message) || Drupal.t('Could not restore item.'));
                }
              }
            },
            error: function () {
              $btn.removeAttr('disabled').text(Drupal.t('Restore'));
              if (window.DFBToast) {
                DFBToast.error(Drupal.t('An error occurred. Please try again.'));
              }
            }
          });
        });
      });

      $(document).once('dfb-restore-esc', function () {
        $(this).bind('keydown', function (e) {
          if ((e.key === 'Escape' || e.keyCode === 27) && $('#dfb-restore-modal').is(':visible')) {
            _close($('#dfb-restore-modal'));
          }
        });
      });
    }
  };

  function _close($modal) {
    $modal.animate({ opacity: 0 }, 150, function () {
      $modal.css('display', 'none');
    });
  }

})(jQuery, Drupal);
