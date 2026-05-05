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
      if (!s) { s = { basePath: '', deletePath: '' }; }

      var basePath   = s.basePath;
      var deletePath = s.deletePath;

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

      // Apply initial type visibility for the add form on page load.
      $('#dfb-question-form-wrapper', context).once('dfb-type-visibility-init', function () {
        _dfbApplyTypeVisibility($(this));
      });

      // Width picker — clicking a button updates the hidden width input and
      // toggles the active highlight. Delegated on body so it works in both
      // the add modal and the edit modal after AJAX injection.
      $('body').once('dfb-width-picker', function () {
        $(document).delegate('.dfb-width-btn', 'click', function () {
          var $btn    = $(this);
          var width   = $btn.attr('data-width');
          var $picker = $btn.closest('.dfb-width-picker');
          $picker.find('.dfb-width-btn').removeClass('dfb-width-active');
          $btn.addClass('dfb-width-active');
          // Traverse up to the <form> so we update only the hidden input that
          // belongs to the same form as the clicked button.
          $btn.closest('form').find('input.dfb-width-value').val(width);
        });
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
          var $radios = $('.dfb-type-section input[type="radio"]', '#dfb-question-modal');
          $radios.each(function () {
            this.checked = (this.value === 'text');
          });
          // Sync the selected-card highlight.
          $('.dfb-type-section .form-type-radio', '#dfb-question-modal').removeClass('dfb-type-selected');
          $('.dfb-type-section input[value="text"]', '#dfb-question-modal').closest('.form-type-radio').addClass('dfb-type-selected');

          // Reset width picker to Full on every open.
          var $addWrapper = $('#dfb-question-form-wrapper');
          $addWrapper.find('.dfb-width-btn').removeClass('dfb-width-active');
          $addWrapper.find('.dfb-width-btn[data-width="full"]').addClass('dfb-width-active');
          $addWrapper.find('input.dfb-width-value').val('full');

          // Apply type-based visibility (type is 'text', so options/scale/file hidden,
          // validation shown).
          _dfbApplyTypeVisibility($('#dfb-question-form-wrapper'));

          // Show modal — use CSS animate to preserve display:flex.
          $('#dfb-question-modal').css({ opacity: 0, display: 'flex' }).animate({ opacity: 1 }, 150);
        });
      });

      // Re-inject section_id and re-apply type visibility after every AJAX rebuild.
      // (Options/validations AJAX replaces sub-sections; the save AJAX replaces the whole
      // wrapper — in both cases section_id resets to 0 in the fresh markup.)
      // Re-applying visibility is necessary because the fresh HTML from the server carries
      // the data-dfb-show-for attribute, which CSS hides by default, but Drupal only calls
      // attachBehaviors on the newly replaced child element — not the parent form wrapper —
      // so _dfbApplyTypeVisibility never fires automatically after a partial AJAX replace.
      $('body').once('dfb-section-id-reinjector', function () {
        $(document).ajaxComplete(function () {
          var $addModal  = $('#dfb-question-modal');
          var $editModal = $('#dfb-edit-question-modal');

          if ($addModal.is(':visible')) {
            var sid = $addModal.data('active-section-id');
            if (sid) { $('input[name="section_id"]').val(sid); }
            var sname = $addModal.data('active-section-name');
            if (sname !== undefined) { $('#dfb-section-name-display').text(sname); }
            _dfbApplyTypeVisibility($('#dfb-question-form-wrapper'));
          }

          if ($editModal.is(':visible')) {
            _dfbApplyTypeVisibility($('#dfb-edit-question-form-wrapper'));
          }
        });
      });

      // Close add-question modal — delegated so it survives AJAX rebuilds.
      $('body').once('dfb-modal-close-delegated', function () {
        $(document).delegate('[data-action="close-modal"]', 'click', function (e) {
          e.preventDefault();
          e.stopImmediatePropagation();
          $('#dfb-question-modal').hide();
        });
        $('#dfb-question-modal').bind('click', function (e) {
          if ($(e.target).is('#dfb-question-modal')) { $(this).hide(); }
        });
      });

      // Close edit-question modal.
      $('body').once('dfb-edit-modal-close', function () {
        $(document).delegate('[data-action="close-edit-modal"]', 'click', function (e) {
          e.preventDefault();
          e.stopImmediatePropagation();
          $('#dfb-edit-question-modal').hide();
        });
        $('#dfb-edit-question-modal').bind('click', function (e) {
          if ($(e.target).is('#dfb-edit-question-modal')) { $(this).hide(); }
        });
      });

      // Click on a question's content area to open the edit modal.
      // Drag-handle clicks are excluded by targeting .dfb-question-editable.
      $('#dfb-builder-container', context).once('dfb-question-edit', function () {
        $(this).delegate('.dfb-question-editable', 'click', function (e) {
          e.stopPropagation();
          // Let the delete-icon handler take over when the trash button is clicked.
          if ($(e.target).closest('[data-action="open-delete-modal"]').length) { return; }
          var questionId = $(this).closest('.dfb-question-card').data('question-id');
          if (!questionId) { return; }

          // Show the modal immediately with the loading state.
          $('#dfb-edit-question-modal').css({ opacity: 0, display: 'flex' }).animate({ opacity: 1 }, 150);

          $.ajax({
            url:      basePath + '/question/' + questionId + '/edit-form',
            type:     'GET',
            dataType: 'json',
            success: function (commands) {
              // Drupal's `insert` command calls ajax.getEffect() — provide a
              // minimal shim so the handler doesn't throw.
              var fakeAjax = {
                wrapper: '#dfb-edit-form-placeholder',
                method:  'html',
                effect:  'none',
                speed:   'none',
                getEffect: function (response) {
                  var type = response.effect || this.effect;
                  if (type === 'none') {
                    return { showEffect: 'show', hideEffect: 'hide', showSpeed: '' };
                  }
                  var speed = response.speed || this.speed;
                  if (type === 'fade') {
                    return { showEffect: 'fadeIn', hideEffect: 'fadeOut', showSpeed: speed };
                  }
                  return { showEffect: type + 'Toggle', hideEffect: type + 'Toggle', showSpeed: speed };
                }
              };

              // Process each Drupal AJAX command (settings merge + html injection).
              $.each(commands, function (i, cmd) {
                if (Drupal.ajax && Drupal.ajax.prototype.commands[cmd.command]) {
                  Drupal.ajax.prototype.commands[cmd.command](fakeAjax, cmd, null);
                }
              });

              // -------------------------------------------------------
              // ID-conflict fix: both the Add form and Edit form are
              // rendered in separate PHP requests, so Drupal gives them
              // identical element IDs (e.g. "edit-type-text").
              // When the Edit form is injected, clicking a label whose
              // for="edit-type-text" finds the FIRST matching element —
              // which is the hidden Add form's radio, not the Edit form's.
              // Fix: rebind each label to directly check its sibling input,
              // bypassing the for-attribute ID lookup entirely.
              // -------------------------------------------------------
              $('#dfb-edit-question-modal .dfb-type-section .form-type-radio').each(function () {
                var $item = $(this);
                var radio = $item.find('input[type="radio"]')[0]; // native DOM element
                $item.find('label').unbind('click.dfbfix').bind('click.dfbfix', function (e) {
                  e.preventDefault();
                  if (radio) {
                    radio.checked = true;
                    $(radio).trigger('change');
                  }
                });
              });

              // Apply type visibility now that the edit form is in the DOM.
              _dfbApplyTypeVisibility($('#dfb-edit-question-form-wrapper'));
            },
            error: function () {
              $('#dfb-edit-question-modal').hide();
              if (window.DFBToast) { DFBToast.error(Drupal.t('Failed to load question editor. Please try again.')); }
            }
          });
        });
      });

      // Type picker — label click fix.
      // Both forms render radios with identical IDs (separate PHP requests reset
      // Drupal's ID counter). A label's `for` attribute would match the first
      // element with that ID in the DOM — always the add form's radio, even when
      // clicking inside the edit modal. Intercepting the click and checking the
      // radio by DOM proximity (same .form-type-radio wrapper) fixes this.
      $('body').once('dfb-type-picker-label-fix', function () {
        $(document).delegate('.dfb-type-section label', 'click', function (e) {
          e.preventDefault();
          var radio = $(this).closest('.form-type-radio').find('input[type="radio"]')[0];
          if (radio && !radio.checked) {
            radio.checked = true;
            $(radio).trigger('change');
          }
        });
      });

      // Type picker: highlight the selected card AND update sub-section visibility.
      // Delegated on body so it covers both the add form (rendered on page
      // load) and the edit form (injected dynamically via AJAX).
      $('body').once('dfb-type-picker-global', function () {
        $(document).delegate('.dfb-type-section input[type="radio"]', 'change', function () {
          var $input   = $(this);
          var $section = $input.closest('.dfb-type-section');
          if ($input.is(':checked')) {
            $section.find('.form-type-radio').removeClass('dfb-type-selected');
            $input.closest('.form-type-radio').addClass('dfb-type-selected');

            // Update conditional sub-section visibility for this form.
            var $wrapper = $input.closest('#dfb-question-form-wrapper, #dfb-edit-question-form-wrapper');
            if ($wrapper.length) {
              _dfbApplyTypeVisibility($wrapper);
            }
          }
        });
      });
      // Highlight whichever radio is already checked (add form on load;
      // edit form is handled when Drupal.attachBehaviors fires after inject).
      $('.dfb-type-section input[type="radio"]:checked', context)
        .closest('.form-type-radio').addClass('dfb-type-selected');

      // Inline section rename: click the edit icon to make the h2 editable.
      $('#dfb-builder-container', context).once('dfb-section-rename', function () {
        $(this).delegate('[data-action="inline-rename-section"]', 'click', function (e) {
          e.preventDefault();
          e.stopPropagation();

          var $btn    = $(this);
          var $card   = $btn.closest('.dfb-section-card');
          var $h2     = $card.find('.dfb-section-title');
          var current = $h2.text();

          if ($h2.hasClass('dfb-renaming')) { return; }
          $h2.addClass('dfb-renaming');

          var $input = $('<input>')
            .attr('type', 'text')
            .addClass('dfb-section-title-input')
            .val(current);

          $h2.hide().after($input);
          $input[0].focus();
          $input[0].select();

          function _save() {
            var newName = $.trim($input.val());
            if (!newName) { _cancel(); return; }
            if (newName === current) { _cancel(); return; }

            var sectionId = $card.data('section-id');
            $input.attr('disabled', 'disabled');

            $.ajax({
              url:         basePath + '/section/' + sectionId + '/rename',
              type:        'POST',
              contentType: 'application/json',
              dataType:    'json',
              data:        JSON.stringify({ name: newName }),
              success: function (resp) {
                if (resp.status === 'success') {
                  $h2.text(resp.name).removeClass('dfb-renaming').show();
                  $input.remove();
                  // Keep the Add Question button's data-section-name in sync.
                  $card.find('[data-section-name]').attr('data-section-name', resp.name);
                  if (window.DFBToast) { DFBToast.success(Drupal.t('Section renamed.')); }
                }
                else {
                  if (window.DFBToast) { DFBToast.error(resp.message || Drupal.t('Could not rename section.')); }
                  _cancel();
                }
              },
              error: function () {
                if (window.DFBToast) { DFBToast.error(Drupal.t('A network error occurred.')); }
                _cancel();
              }
            });
          }

          function _cancel() {
            $h2.removeClass('dfb-renaming').show();
            $input.remove();
          }

          $input.bind('keydown', function (e) {
            if (e.which === 13) { e.preventDefault(); $input.unbind('blur'); _save(); }
            if (e.which === 27) { $input.unbind('blur'); _cancel(); }
          });
          $input.bind('blur', function () { _save(); });
        });
      });

      // -------------------------------------------------------------------
      // Delete confirmation modal.
      //
      // Any element with data-action="open-delete-modal" opens the shared
      // #dfb-delete-confirm-modal.  The entity type, id, and display name
      // are read from data attributes and stored on the modal element.
      // -------------------------------------------------------------------

      // Open: show modal and remember what we are about to delete.
      $('#dfb-builder-container', context).once('dfb-delete-modal-open', function () {
        $(this).delegate('[data-action="open-delete-modal"]', 'click', function (e) {
          e.preventDefault();
          e.stopPropagation();
          var $btn  = $(this);
          var modal = $('#dfb-delete-confirm-modal');
          modal.data('entity-type', $btn.attr('data-entity-type'));
          modal.data('entity-id',   $btn.attr('data-entity-id'));
          modal.data('entity-card', $btn.closest('.dfb-section-card, .dfb-question-card'));
          $('#dfb-delete-confirm-name').text($btn.attr('data-entity-name'));
          modal.css({ opacity: 0, display: 'flex' }).animate({ opacity: 1 }, 150);
        });
      });

      // Close: cancel button or backdrop click.
      $('body').once('dfb-delete-modal-close', function () {
        $(document).delegate('[data-action="close-delete-modal"]', 'click', function (e) {
          e.preventDefault();
          $('#dfb-delete-confirm-modal').hide();
        });
        $('#dfb-delete-confirm-modal').bind('click', function (e) {
          if ($(e.target).is('#dfb-delete-confirm-modal')) { $(this).hide(); }
        });
      });

      // Confirm: POST to delete endpoint, remove card from DOM on success.
      $('body').once('dfb-delete-modal-confirm', function () {
        $(document).delegate('#dfb-delete-confirm-btn', 'click', function () {
          var modal      = $('#dfb-delete-confirm-modal');
          var entityType = modal.data('entity-type');
          var entityId   = modal.data('entity-id');
          var $card      = modal.data('entity-card');
          var $btn       = $(this);

          $btn.attr('disabled', 'disabled').text(Drupal.t('Deleting…'));

          $.ajax({
            url:         deletePath + '/' + entityType + '/' + entityId,
            type:        'POST',
            dataType:    'json',
            success: function (resp) {
              modal.hide();
              $btn.removeAttr('disabled').text(Drupal.t('Delete'));
              if (resp.status === 'ok') {
                if ($card && $card.length) { $card.remove(); }
                if (window.DFBToast) { DFBToast.success(resp.message || Drupal.t('Deleted.')); }
              }
              else {
                if (window.DFBToast) { DFBToast.error(resp.message || Drupal.t('Could not delete item.')); }
              }
            },
            error: function () {
              modal.hide();
              $btn.removeAttr('disabled').text(Drupal.t('Delete'));
              if (window.DFBToast) { DFBToast.error(Drupal.t('A network error occurred.')); }
            }
          });
        });
      });
    }
  };

  // -------------------------------------------------------------------
  // Show/hide conditional sub-sections based on the selected question type.
  //
  // Each conditional container carries data-dfb-show-for="<space-separated
  // type list>".  This function reads the checked type radio within the
  // given form wrapper and toggles every [data-dfb-show-for] element.
  // -------------------------------------------------------------------
  function _dfbApplyTypeVisibility($wrapper) {
    var type = $wrapper.find('input[name="type"]:checked').val();
    if (!type) { return; }
    $wrapper.find('[data-dfb-show-for]').each(function () {
      var allowed = $(this).attr('data-dfb-show-for').split(' ');
      $(this).toggle(allowed.indexOf(type) !== -1);
    });
  }

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
              if (window.DFBToast) { DFBToast.warning(Drupal.t('Could not save order: ') + response.message); }
            }
          },
          error: function () {
            if (window.DFBToast) { DFBToast.error(Drupal.t('A network error occurred while saving the new order.')); }
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
