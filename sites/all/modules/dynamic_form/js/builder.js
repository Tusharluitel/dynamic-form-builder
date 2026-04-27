(function ($, Drupal) {

  Drupal.behaviors.dynamicFormBuilder = {
    attach: function (context, settings) {
      // Guard: only run if we're on the builder page OR the standalone edit page.
      if (!document.getElementById('dfb-builder-container') && !document.getElementById('dfb-question-form-wrapper')) {
        return;
      }

      var s = settings.dynamicFormBuilder;
      // settings might be undefined on the edit page depending on how the
      // array was merged, so we default it if missing.
      if (!s) { s = { basePath: '' }; }

      var basePath = s.basePath;

      // -----------------------------------------------------------------
      // SortableJS: sections wrapper.
      //
      // .once(id, fn) marks elements so the callback never fires twice on
      // the same DOM element.  After Drupal's ajax_command_replace swaps
      // #dfb-sections-wrapper, the new element has no marker, so Sortable
      // is re-initialized correctly on the replacement element.
      // -----------------------------------------------------------------
      $('#dfb-sections-wrapper', context).once('dfb-sections-sortable', function () {
        _dfbInitSortable(this, '.dfb-section-drag-handle', 'section', basePath + '/reorder');
      });

      // -----------------------------------------------------------------
      // SortableJS: question wrappers inside sections.
      // Same pattern — works for sections already on the page and for
      // sections injected after a Drupal AJAX rebuild.
      // -----------------------------------------------------------------
      $('.dfb-questions-wrapper', context).once('dfb-questions-sortable', function () {
        _dfbInitSortable(this, '.dfb-question-drag-handle', 'question', basePath + '/reorder', 'dfb-questions-wrapper');
      });

      // Open modal: inject section context into the Drupal form, show modal.
      // Delegated so dynamically-added "Add Question" buttons work after sections AJAX.
      $('#dfb-builder-container', context).once('dfb-question-modal-open', function () {
        $(this).delegate('[data-action="open-question-modal"]', 'click', function (e) {
          e.preventDefault();
          var $btn        = $(this);
          var sectionId   = $btn.data('section-id');
          var sectionName = $btn.data('section-name') || '';

          // Store active section on the modal itself so we can re-inject after AJAX rebuilds.
          $('#dfb-question-modal').data('active-section-id', sectionId).data('active-section-name', sectionName);

          // Inject into the hidden field — use name selector, NOT id, because Drupal
          // appends a numeric suffix (--N) to all form element IDs.
          $('input[name="section_id"]').val(sectionId);
          $('#dfb-section-name-display').text(sectionName);

          // Reset type picker to the first option (text) on every open.
          var $radios = $('.dfb-type-section input[type="radio"]');
          $radios.each(function () {
            this.checked = (this.value === 'text');
          });
          // Sync the selected-card highlight.
          $('.dfb-type-section .form-type-radio').removeClass('dfb-type-selected');
          $('.dfb-type-section input[value="text"]').closest('.form-type-radio').addClass('dfb-type-selected');
          // Re-trigger Drupal states so sub-sections show/hide correctly.
          $radios.filter(':checked').trigger('change');

          // Show modal — use CSS animate to preserve display:flex.
          $('#dfb-question-modal').css({ opacity: 0, display: 'flex' }).animate({ opacity: 1 }, 150);
        });
      });

      // Re-inject section_id after every AJAX rebuild of the question form wrapper.
      // (Options/validations AJAX replaces sub-sections; the save AJAX replaces the whole
      // wrapper — in both cases section_id resets to 0 in the fresh markup.)
      $('body').once('dfb-section-id-reinjector', function () {
        $(document).ajaxComplete(function () {
          var $modal = $('#dfb-question-modal');
          if (!$modal.is(':visible')) { return; }
          var sid = $modal.data('active-section-id');
          if (sid) {
            $('input[name="section_id"]').val(sid);
          }
          var sname = $modal.data('active-section-name');
          if (sname !== undefined) {
            $('#dfb-section-name-display').text(sname);
          }
        });
      });

      // Close modal — delegated on document so it always works, even after
      // AJAX rebuilds that replace the Cancel button's DOM node.
      // Runs once per page load (guarded by the body once marker).
      $('body').once('dfb-modal-close-delegated', function () {
        // Any element with data-action="close-modal" closes the modal.
        $(document).delegate('[data-action="close-modal"]', 'click', function (e) {
          e.preventDefault();
          e.stopImmediatePropagation(); // prevent Drupal's AJAX submit handler
          $('#dfb-question-modal').hide();
        });

        // Clicking the dark backdrop (overlay itself, not its children) also closes.
        $('#dfb-question-modal').on('click', function (e) {
          if ($(e.target).is('#dfb-question-modal')) {
            $(this).hide();
          }
        });
      });

      // Type picker: highlight the selected card with a CSS class.
      // The radio input is visually hidden; the parent .form-type-radio gets 'dfb-type-selected'.
      // This supplements the CSS :checked selector for browsers where sibling CSS isn't enough.
      // Uses delegated binding on the wrapper so it survives AJAX rebuilds of the form.
      $('#dfb-question-form-wrapper', context).once('dfb-type-picker', function () {
        $(this).delegate('.dfb-type-section input[type="radio"]', 'change', function () {
          var $input = $(this);
          if ($input.is(':checked')) {
            $('.dfb-type-section .form-type-radio').removeClass('dfb-type-selected');
            $input.closest('.form-type-radio').addClass('dfb-type-selected');
          }
        });
        // Highlight the default on page load.
        $('.dfb-type-section input[type="radio"]:checked').closest('.form-type-radio').addClass('dfb-type-selected');
      });
    }
  };

  // -------------------------------------------------------------------
  // Shared SortableJS initializer.
  //
  // @param {Element} container    The sortable container element.
  // @param {string}  handleSel   CSS selector for the drag handle.
  // @param {string}  itemType    'section' or 'question'.
  // @param {string}  endpoint    URL of the reorder AJAX endpoint.
  // @param {string}  [groupName] SortableJS group name (enables cross-list drag).
  // -------------------------------------------------------------------
  function _dfbInitSortable(container, handleSel, itemType, endpoint, groupName) {
    if (typeof Sortable === 'undefined') {
      if (window.console) { console.error('SortableJS is not loaded.'); }
      return;
    }

    var options = {
      animation:  150,
      handle:     handleSel,
      ghostClass: 'dfb-sortable-ghost',
      dragClass:  'dfb-sortable-drag',
      onEnd: function (evt) {
        var order    = [];
        var parentId = null;

        if (itemType === 'question') {
          parentId = $(evt.to).data('section-id');
          $(evt.to).children('.dfb-question-card').each(function (index) {
            order.push({
              id:        $(this).data('question-id'),
              position:  index,
              parent_id: parentId
            });
          });
        }
        else {
          $(container).children('.dfb-section-card').each(function (index) {
            order.push({
              id:       $(this).data('section-id'),
              position: index
            });
          });
        }

        $.ajax({
          url:         endpoint,
          type:        'POST',
          contentType: 'application/json',
          dataType:    'json',
          data:        JSON.stringify({ type: itemType, order: order }),
          success: function (response) {
            if (response.status !== 'success') {
              alert(Drupal.t('Failed to save order: ') + response.message);
            }
          },
          error: function () {
            alert(Drupal.t('A network error occurred while saving the new order.'));
          }
        });
      }
    };

    if (groupName) {
      options.group = groupName;
    }

    new Sortable(container, options);
  }

})(jQuery, Drupal);
