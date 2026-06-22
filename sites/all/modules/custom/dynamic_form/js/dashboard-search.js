/**
 * @file
 * Dynamic Form — live text filter for dashboard listing tables.
 *
 * Injects a search input above .dfb-dashboard-table and hides rows whose
 * first column does not contain the typed string. Works with jQuery 1.4.4+.
 */
(function ($, Drupal) {

  Drupal.behaviors.dynamicFormDashboardSearch = {
    attach: function (context, settings) {
      var s       = settings.dynamicFormSearch || {};
      var section = s.section || '';

      var placeholders = {
        forms:     Drupal.t('Search forms…'),
        questions: Drupal.t('Search questions…'),
        sections:  Drupal.t('Search sections…')
      };
      var placeholder = placeholders[section] || Drupal.t('Search…');

      $('.dfb-dashboard-table', context).once('dfb-search-init', function () {
        var $tableEl = $(this);
        var $rows    = $tableEl.find('tbody tr');

        var $wrap  = $('<div class="dfb-search-bar-wrap"></div>');
        var $input = $(
          '<input type="text" class="dfb-search-input"' +
          ' placeholder="' + placeholder + '">'
        );
        var $clear = $('<button type="button" class="dfb-search-clear" title="' + Drupal.t('Clear') + '">&times;</button>');
        var $count = $('<span class="dfb-search-count"></span>');

        $wrap.append($input).append($clear).append($count);
        $tableEl.before($wrap);

        function _updateCount(visible, total) {
          if ($input.val() === '') {
            $count.text('').hide();
          } else {
            $count.text(Drupal.t('@v of @t', { '@v': visible, '@t': total })).show();
          }
        }

        function _filter() {
          var q     = $.trim($input.val()).toLowerCase();
          var total = $rows.length;
          var shown = 0;

          $rows.each(function () {
            var text = $(this).find('td').eq(0).text().toLowerCase();
            if (!q || text.indexOf(q) !== -1) {
              $(this).show();
              shown++;
            } else {
              $(this).hide();
            }
          });

          $clear.toggle(q.length > 0);
          _updateCount(shown, total);
        }

        // jQuery 1.4.4 has no .on() — use .bind().
        $input.bind('keyup', function () { _filter(); });

        $clear.bind('click', function () {
          $input.val('').focus();
          _filter();
        });

        // Init state.
        $clear.hide();
        $count.hide();
      });
    }
  };

})(jQuery, Drupal);
