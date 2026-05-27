/**
 * @file
 * Response filter bar — static question filters + clear buttons.
 *
 * Option-type question filters use Select2 v3 (multiple). All other types
 * use plain text/date/number inputs. This script handles:
 *   1. Initializing Select2 on multi-select question filters.
 *   2. Per-field clear (×) buttons that reset the value and re-submit.
 *   3. Disabling empty question fields on submit so they don't pollute the URL.
 *   4. Date/time range filters (two inputs: val_from / val_to).
 *   5. "Select all" / "Deselect all" button for choice-type question filters.
 */
(function ($) {
  'use strict';

  /**
   * Returns the current value(s) for a filter input.
   *
   * For Select2-backed multi-selects, reads from Select2's internal state
   * rather than the hidden native <select> (which Select2 v3 may not have
   * synced to the DOM yet at submit time).
   */
  function getFilterVal($el) {
    if ($el.closest('.dfr-qfilter-field--multi').length && typeof $.fn.select2 !== 'undefined') {
      return $el.select2('val'); // returns [] or array of strings
    }
    return $el.val();
  }

  /**
   * Returns true when at least one .dfr-qfilter-val inside $field has a value.
   */
  function fieldHasValue($field) {
    var hasVal = false;
    $field.find('.dfr-qfilter-val').each(function () {
      var v = getFilterVal($(this));
      if (v && v.length > 0) {
        hasVal = true;
        return false; // break each
      }
    });
    return hasVal;
  }

  Drupal.behaviors.dfrResponseFilter = {
    attach: function (context, settings) {
      var $form = $('#dfr-filter-form', context);
      if (!$form.length || $form.data('dfr-filter-attached')) {
        return;
      }
      $form.data('dfr-filter-attached', true);

      // ── Initialize Select2 on multi-select question filters ────────────
      if (typeof $.fn.select2 !== 'undefined') {
        // Static multi-select (radio / checkbox / select type questions).
        $form.find('.dfr-qfilter-select').each(function () {
          var $sel = $(this);
          $sel.select2({ width: '100%', closeOnSelect: false });
          $sel.closest('.dfr-qfilter-field').addClass('dfr-qfilter-field--multi');

          // Inject collapsed-summary overlay inside the Select2 container.
          // When not active and items are selected, the summary shows truncated
          // text over the (overflow-hidden) chips. Clicking the summary opens
          // Select2 directly (no pointer-events pass-through needed).
          var $s2container = $sel.data('select2').container;
          var $summary = $('<div class="dfr-s2-summary" aria-hidden="true"></div>')
            .appendTo($s2container)
            .on('click', function () { $sel.select2('open'); });
          var s2IsOpen = false;

          function refreshS2Summary() {
            var vals = $sel.select2('val') || [];
            if (!vals.length) { $summary.hide(); return; }
            var texts = vals.map(function (v) {
              return $sel.find('option').filter(function () {
                return $(this).val() === v;
              }).first().text() || v;
            });
            var label = texts.length <= 2
              ? texts.join(', ')
              : texts[0] + ', ' + texts[1] + ' +' + (texts.length - 2) + ' more';
            $summary.text(label).attr('title', texts.join(', ')).show();
          }

          $sel
            .on('select2-open',  function () { s2IsOpen = true;  $summary.hide(); })
            .on('select2-close', function () { s2IsOpen = false; refreshS2Summary(); })
            .on('change',        function () { if (!s2IsOpen) { refreshS2Summary(); } });

          refreshS2Summary(); // set initial state for pre-filled filters
        });

        // AJAX autocomplete for tags questions — mirrors the response form's
        // tag input, backed by dynamic-form/ajax/tags/autocomplete (Solr).
        $form.find('.dfr-qfilter-tags').each(function () {
          var $el   = $(this);
          var acUrl = $el.data('autocomplete-url');

          $el.select2({
            tags:               [],
            tokenSeparators:    [','],
            width:              '100%',
            placeholder:        $el.data('placeholder') || Drupal.t('Type to search tags…'),
            minimumInputLength: 1,
            ajax: {
              url:          acUrl,
              dataType:     'json',
              quietMillis:  250,
              data: function (term) { return { term: term }; },
              results: function (data) {
                if ($.isArray(data)) { return { results: data }; }
                if (data.results && data.results.length) {
                  return { results: data.results };
                }
                if (data.spellcheck && data.spellcheck.found && data.spellcheck.suggestions.length) {
                  return { results: [{ text: Drupal.t('Did you mean?'), children: data.spellcheck.suggestions }] };
                }
                return { results: [] };
              }
            },
            // Allow free-form tags not in the autocomplete list.
            createSearchChoice: function (term, data) {
              var lc = term.toLowerCase();
              for (var i = 0; i < data.length; i++) {
                var items = data[i].children ? data[i].children : [data[i]];
                for (var j = 0; j < items.length; j++) {
                  if (items[j].text && items[j].text.toLowerCase() === lc) { return; }
                }
              }
              return { id: term, text: term };
            },
            // Restore pre-filled tags from the URL (comma-separated value).
            initSelection: function (element, callback) {
              var val = element.val();
              if (!val) { return; }
              var data = [];
              $.each(val.split(','), function (i, t) {
                t = $.trim(t);
                if (t) { data.push({ id: t, text: t }); }
              });
              callback(data);
            }
          });

          $el.closest('.dfr-qfilter-field').addClass('dfr-qfilter-field--multi');
        });
      }

      // ── "Select all" / "Deselect all" for choice-type filters ─────────
      $form.on('click', '.dfr-qfilter-select-all', function () {
        var $btn    = $(this);
        var $field  = $btn.closest('.dfr-qfilter-field');
        var $select = $field.find('.dfr-qfilter-select');

        if (typeof $.fn.select2 === 'undefined') { return; }

        var allVals = $select.find('option').map(function () { return $(this).val(); }).get();
        var curVals = $select.select2('val') || [];

        if (curVals.length === allVals.length && allVals.length > 0) {
          // Deselect all.
          $select.select2('val', []).trigger('change');
          $btn.text(Drupal.t('Select all'));
        } else {
          // Select all.
          $select.select2('val', allVals).trigger('change');
          $btn.text(Drupal.t('Deselect all'));
        }
      });

      // Sync "Select all" button label when Select2 changes via the dropdown.
      $form.on('change', '.dfr-qfilter-select', function () {
        var $select  = $(this);
        var $field   = $select.closest('.dfr-qfilter-field');
        var $btn     = $field.find('.dfr-qfilter-select-all');
        if (!$btn.length || typeof $.fn.select2 === 'undefined') { return; }
        var totalOpts = $select.find('option').length;
        var selCount  = ($select.select2('val') || []).length;
        $btn.text(selCount === totalOpts && totalOpts > 0
          ? Drupal.t('Deselect all') : Drupal.t('Select all'));
      });

      // ── Clear button: reset field value and show/hide self ─────────────
      $form.on('click', '.dfr-qfilter-clear', function () {
        var $btn   = $(this);
        var $field = $btn.closest('.dfr-qfilter-field');
        var $val   = $field.find('.dfr-qfilter-val');

        if ($field.hasClass('dfr-qfilter-field--multi') && typeof $.fn.select2 !== 'undefined') {
          $val.select2('val', []).trigger('change');
        } else {
          $val.val('').trigger('change');
        }
        $btn.hide();
        $form.submit();
      });

      // Toggle clear button visibility whenever any value in a field changes.
      $form.on('change input', '.dfr-qfilter-val', function () {
        var $field  = $(this).closest('.dfr-qfilter-field');
        var $btn    = $field.find('.dfr-qfilter-clear');
        if (fieldHasValue($field)) { $btn.show(); } else { $btn.hide(); }
      });

      // ── On submit: disable hidden qid/op for fields with empty value ────
      // Prevents empty f[i][qid]=5 params from appearing in the URL,
      // which keeps $has_filters accurate on the server.
      $form.on('submit', function () {
        // Disable empty meta controls so they don't appear in the URL.
        $.each(['#dfr-filter-date-from', '#dfr-filter-date-to',
                '#dfr-filter-respondent'], function (i, sel) {
          var $el = $(sel, context);
          if ($el.length && !$el.val()) {
            $el.prop('disabled', true);
          }
        });

        // For sort, only disable if it's the default value to keep URLs clean.
        var $sort = $('#dfr-filter-sort', context);
        if ($sort.length && $sort.val() === 'date_desc') {
          $sort.prop('disabled', true);
        }

        // For question filters, disable the group when val is empty.
        $form.find('.dfr-qfilter-field').each(function () {
          var $grp    = $(this);
          var $valEl  = $grp.find('.dfr-qfilter-val');
          var isEmpty = !fieldHasValue($grp);

          if (isEmpty) {
            $grp.find('.dfr-qfilter-qid, .dfr-qfilter-op').prop('disabled', true);

            if ($grp.hasClass('dfr-qfilter-field--range')) {
              // Disable both range inputs so they don't appear in the URL.
              $grp.find('.dfr-qfilter-range-from, .dfr-qfilter-range-to').prop('disabled', true);
            } else if (!$grp.hasClass('dfr-qfilter-field--multi')) {
              $valEl.prop('disabled', true);
            }
            // For multi: the empty <select> already submits no val[] params.
          }
        });
      });
    }
  };
}(jQuery));
