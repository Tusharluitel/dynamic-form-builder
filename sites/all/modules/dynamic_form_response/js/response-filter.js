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
          $(this).select2({ width: '100%' });
          $(this).closest('.dfr-qfilter-field').addClass('dfr-qfilter-field--multi');
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

      // ── Clear button: reset field value and show/hide self ─────────────
      $form.on('click', '.dfr-qfilter-clear', function () {
        var $btn   = $(this);
        var $field = $btn.closest('.dfr-qfilter-field');
        var $val   = $field.find('.dfr-qfilter-val');

        if ($field.hasClass('dfr-qfilter-field--multi') && typeof $.fn.select2 !== 'undefined') {
          $val.select2('val', '').trigger('change');
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
        var current = getFilterVal($val);
        var hasVal  = current && current.length > 0;
        if (hasVal) { $btn.show(); } else { $btn.hide(); }
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
          var $grp   = $(this);
          var $valEl = $grp.find('.dfr-qfilter-val');
          var current = getFilterVal($valEl);
          var isEmpty = !current || current.length === 0;

          if (isEmpty) {
            // Disable qid and op so they don't appear in the URL.
            // For Select2 multi: do NOT disable the native <select> itself —
            // disabling it can confuse Select2. The empty multi-select already
            // submits no val[] params on its own.
            $grp.find('.dfr-qfilter-qid, .dfr-qfilter-op').prop('disabled', true);
            if (!$grp.hasClass('dfr-qfilter-field--multi')) {
              $valEl.prop('disabled', true);
            }
          }
        });
      });
    }
  };
}(jQuery));
