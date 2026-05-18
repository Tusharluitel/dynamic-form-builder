/**
 * @file
 * Response filter bar — static question filters + clear buttons.
 *
 * Option-type question filters use Select2 v3 (multiple). All other types
 * use plain text/date/number inputs. This script handles:
 *   1. Initializing Select2 on multi-select question filters.
 *   2. Per-field clear (×) buttons that reset the value and re-submit.
 *   3. Disabling empty question fields on submit so they don't pollute the URL.
 */
(function ($) {
  'use strict';

  Drupal.behaviors.dfrResponseFilter = {
    attach: function (context, settings) {
      var $form = $('#dfr-filter-form', context);
      if (!$form.length || $form.data('dfr-filter-attached')) {
        return;
      }
      $form.data('dfr-filter-attached', true);

      // ── Initialize Select2 on multi-select question filters ────────────
      if (typeof $.fn.select2 !== 'undefined') {
        $form.find('.dfr-qfilter-select').each(function () {
          $(this).select2({ width: '100%' });
        });
      }

      // ── Clear button: reset field value and show/hide self ─────────────
      $form.on('click', '.dfr-qfilter-clear', function () {
        var $btn   = $(this);
        var $field = $btn.closest('.dfr-qfilter-field');
        var $val   = $field.find('.dfr-qfilter-val');

        if ($val.prop('multiple')) {
          $val.val([]).trigger('change');
        } else {
          $val.val('').trigger('change');
        }
        $btn.hide();
        $form.submit();
      });

      // Toggle clear button visibility whenever a value changes.
      $form.on('change input', '.dfr-qfilter-val', function () {
        var $val    = $(this);
        var $btn    = $val.closest('.dfr-qfilter-field').find('.dfr-qfilter-clear');
        var hasVal;

        if ($val.prop('multiple')) {
          var selected = $val.val();
          hasVal = selected && selected.length > 0;
        } else {
          hasVal = $val.val() !== '';
        }

        if (hasVal) { $btn.show(); } else { $btn.hide(); }
      });

      // ── On submit: disable hidden qid/op for fields with empty value ────
      // Prevents empty f[i][qid]=5&f[i][val]= from appearing in the URL,
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

        // For question filters, disable hidden fields when val is empty.
        $form.find('.dfr-qfilter-field').each(function () {
          var $grp   = $(this);
          var $valEl = $grp.find('.dfr-qfilter-val');
          var isEmpty;

          if ($valEl.prop('multiple')) {
            var selected = $valEl.val();
            isEmpty = !selected || selected.length === 0;
          } else {
            isEmpty = !$valEl.val();
          }

          if (isEmpty) {
            $grp.find('.dfr-qfilter-qid, .dfr-qfilter-op, .dfr-qfilter-val')
              .prop('disabled', true);
          }
        });
      });
    }
  };
}(jQuery));
