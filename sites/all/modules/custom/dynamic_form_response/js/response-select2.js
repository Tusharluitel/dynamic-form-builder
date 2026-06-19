(function ($) {
  Drupal.behaviors.dfrSelect2Init = {
    attach: function (context, settings) {
      // Regular select and multi-select fields.
      $('.dfp-select2', context).not('.dfp-tags-select2').once('dfp-s2', function () {
        if (typeof $.fn.select2 === 'undefined') { return; }
        var isMulti = $(this).hasClass('dfp-select-multi');
        $(this).select2({
          placeholder: isMulti ? 'Select options…' : '— Select —',
          allowClear: !isMulti,
          width: '100%',
          dropdownCssClass: 'dfp-s2-preview-drop'
        });
      });

      // Tags fields — autocomplete URL is stored in data-autocomplete-url.
      $('.dfp-tags-select2', context).once('dfp-tags-s2', function () {
        if (typeof $.fn.select2 === 'undefined') { return; }
        var acUrl = $(this).data('autocomplete-url');
        $(this).select2({
          tags: [],
          tokenSeparators: [','],
          width: '100%',
          placeholder: 'Type to search or add tags…',
          minimumInputLength: 1,
          dropdownCssClass: 'dfp-s2-preview-drop',
          ajax: {
            url: acUrl,
            dataType: 'json',
            quietMillis: 250,
            data: function (term) { return { term: term }; },
            results: function (data) {
              // Legacy: plain array (no spellcheck).
              if ($.isArray(data)) { return { results: data }; }
              // Exact/prefix results — return directly.
              if (data.results && data.results.length > 0) {
                return { results: data.results };
              }
              // Spellcheck suggestions — show as a grouped "Did you mean?" set.
              if (data.spellcheck && data.spellcheck.found && data.spellcheck.suggestions.length) {
                return { results: [{ text: Drupal.t('Did you mean?'), children: data.spellcheck.suggestions }] };
              }
              return { results: [] };
            }
          },
          createSearchChoice: function (term, data) {
            var termLC = term.toLowerCase();
            for (var i = 0; i < data.length; i++) {
              // Handle both flat items and grouped children.
              var items = data[i].children ? data[i].children : [data[i]];
              for (var j = 0; j < items.length; j++) {
                if (items[j].text && items[j].text.toLowerCase() === termLC) { return; }
              }
            }
            return { id: term, text: term };
          }
        });
      });
    }
  };
})(jQuery);
