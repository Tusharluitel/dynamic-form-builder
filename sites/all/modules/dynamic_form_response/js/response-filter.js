/**
 * @file
 * Advanced response filter: multi-row AND/OR question filters + full-text search.
 *
 * Each filter row has an operator (AND/OR), a question picker, and a value
 * widget loaded via AJAX. Option-type questions get a Select2 dropdown of
 * predefined options; all other types get a plain input.
 */
(function ($) {
  Drupal.behaviors.dfrResponseFilter = {
    attach: function (context, settings) {
      if (!settings.dfrResponseFilter) {
        return;
      }
      var $form = $('#dfr-filter-form', context);
      if (!$form.length || $form.data('dfr-attached')) {
        return;
      }
      $form.data('dfr-attached', true);

      var cfg     = settings.dfrResponseFilter;
      var $rows   = $('#dfr-filter-rows', context);
      var nextIdx = 0;

      // ── Select2 initializer ─────────────────────────────────────────
      function s2init($el, placeholder) {
        if ($.fn.select2) {
          $el.select2({
            placeholder:       placeholder || '',
            allowClear:        true,
            width:             '100%',
            dropdownAutoWidth: true
          });
        }
      }

      // ── Load value widget for one row via AJAX ──────────────────────
      function loadValue($row, qid, currentVal) {
        var $wrap = $row.find('.dfr-adv-value');
        $wrap.empty().hide();
        if (!qid) {
          return;
        }
        var idx = $row.data('idx');

        $.getJSON(cfg.filterOptionsUrl, { question_id: qid, form_id: cfg.formId },
          function (data) {
            $wrap.empty();

            if (data.is_option_type && data.options && data.options.length) {
              var $sel = $('<select>')
                .attr('name', 'f[' + idx + '][val]')
                .addClass('dfr-adv-value-select')
                .attr('data-placeholder', Drupal.t('Any answer'));
              $sel.append($('<option>').val('').text(''));
              $.each(data.options, function (i, opt) {
                var $opt = $('<option>').val(opt.id).text(opt.text);
                if (String(opt.id) === String(currentVal)) {
                  $opt.attr('selected', 'selected');
                }
                $sel.append($opt);
              });
              $wrap.append($sel);
              s2init($sel, Drupal.t('Any answer'));
              if (currentVal) {
                $sel.select2('val', String(currentVal));
              }

            } else {
              var inputType = 'text';
              var ph = Drupal.t('Enter value…');
              if (data.type === 'number' || data.type === 'rating' || data.type === 'linear_scale') {
                inputType = 'number';
                ph = '';
              } else if (data.type === 'date') {
                inputType = 'date';
                ph = '';
              } else if (data.type === 'time') {
                inputType = 'time';
                ph = '';
              }
              $wrap.append($('<input>').attr({
                type:    inputType,
                name:    'f[' + idx + '][val]',
                placeholder: ph,
                value:   currentVal || ''
              }).addClass('dfr-adv-value-input'));
            }

            $wrap.show();
          }
        );
      }

      // ── Build a filter row ──────────────────────────────────────────
      function buildRow(idx, data) {
        data = data || {};
        var isFirst = ($rows.find('.dfr-adv-row').length === 0);
        var $row    = $('<div>').addClass('dfr-adv-row').attr('data-idx', idx);

        // Operator cell.
        var $opCell = $('<div>').addClass('dfr-adv-cell dfr-adv-cell--op');
        if (isFirst) {
          $opCell.append($('<span>').addClass('dfr-adv-where').text(Drupal.t('Where')));
        } else {
          var $op = $('<select>').attr('name', 'f[' + idx + '][op]').addClass('dfr-adv-op');
          $op.append($('<option>').val('AND').text('AND'));
          $op.append($('<option>').val('OR').text('OR'));
          if (data.op === 'OR') {
            $op.val('OR');
          }
          $opCell.append($op);
        }
        $row.append($opCell);

        // Question picker cell.
        var $qCell = $('<div>').addClass('dfr-adv-cell dfr-adv-cell--question');
        var $qSel  = $('<select>')
          .attr('name', 'f[' + idx + '][qid]')
          .addClass('dfr-adv-qsel')
          .attr('data-placeholder', Drupal.t('Choose question…'));
        $qSel.append($('<option>').val('').text(''));
        $.each(cfg.questions || [], function (i, q) {
          var $opt = $('<option>').val(q.id).text(q.label);
          if (String(q.id) === String(data.qid)) {
            $opt.attr('selected', 'selected');
          }
          $qSel.append($opt);
        });
        $qCell.append($qSel);
        $row.append($qCell);

        // Value widget cell.
        $row.append(
          $('<div>').addClass('dfr-adv-cell dfr-adv-cell--value').append(
            $('<div>').addClass('dfr-adv-value')
          )
        );

        // Remove button cell.
        var $rmCell = $('<div>').addClass('dfr-adv-cell dfr-adv-cell--remove');
        if (!isFirst) {
          $rmCell.append(
            $('<button>').attr('type', 'button').addClass('dfr-adv-remove').html('&times;')
          );
        }
        $row.append($rmCell);

        // When question changes reload value widget.
        $qSel.on('change', function () {
          $row.find('.dfr-adv-value').empty().hide();
          loadValue($row, $(this).val(), '');
        });

        s2init($qSel, Drupal.t('Choose question…'));

        if (data.qid) {
          loadValue($row, data.qid, data.val || '');
        }

        return $row;
      }

      // ── Add row ─────────────────────────────────────────────────────
      $('#dfr-filter-add-row', context).on('click', function () {
        $rows.append(buildRow(nextIdx++, {}));
      });

      // ── Remove row (delegated) ───────────────────────────────────────
      $rows.on('click', '.dfr-adv-remove', function () {
        $(this).closest('.dfr-adv-row').remove();
        renumber();
      });

      // ── Renumber all rows after structural change ────────────────────
      function renumber() {
        $rows.find('.dfr-adv-row').each(function (i) {
          var $row = $(this);
          $row.attr('data-idx', i);
          $row.find('[name]').each(function () {
            $(this).attr('name',
              $(this).attr('name').replace(/^f\[\d+\]/, 'f[' + i + ']')
            );
          });
          if (i === 0) {
            var $opCell = $row.find('.dfr-adv-cell--op');
            if (!$opCell.find('.dfr-adv-where').length) {
              $opCell.empty().append(
                $('<span>').addClass('dfr-adv-where').text(Drupal.t('Where'))
              );
            }
            $row.find('.dfr-adv-cell--remove').empty();
          }
        });
        nextIdx = $rows.find('.dfr-adv-row').length;
      }

      // ── Pre-populate rows from server state ─────────────────────────
      if (cfg.filterRows && cfg.filterRows.length) {
        $.each(cfg.filterRows, function (i, rowData) {
          $rows.append(buildRow(nextIdx++, rowData));
        });
      }

      // ── On submit: renumber and disable empty fields ─────────────────
      $form.on('submit', function () {
        renumber();

        $.each(['#dfr-filter-search', '#dfr-filter-date-from',
                '#dfr-filter-date-to', '#dfr-filter-respondent'], function (i, sel) {
          var $el = $(sel, context);
          if ($el.length && !$el.val()) {
            $el.prop('disabled', true);
          }
        });

        $rows.find('.dfr-adv-row').each(function () {
          if (!$(this).find('.dfr-adv-qsel').val()) {
            $(this).find('[name]').prop('disabled', true);
          }
        });
      });
    }
  };
})(jQuery);
