(function ($) {

  // Star rating, linear scale, and file upload interactions.
  Drupal.behaviors.dfpPreviewInteractive = {
    attach: function (context, settings) {
      // Star rating — highlight stars on hover/click.
      $('.dfp-stars', context).once('dfp-stars-init', function () {
        var stars = $(this).find('.dfp-star');
        stars.bind('mouseover', function () {
          var idx = stars.index(this);
          stars.each(function (i) {
            $(this)[i <= idx ? 'addClass' : 'removeClass']('dfp-star-active');
          });
        }).bind('mouseout', function () {
          stars.each(function () {
            if (!$(this).data('selected')) {
              $(this).removeClass('dfp-star-active');
            }
          });
        }).bind('click', function () {
          var idx = stars.index(this);
          stars.each(function (i) {
            $(this).data('selected', i <= idx);
            $(this)[i <= idx ? 'addClass' : 'removeClass']('dfp-star-active');
          });
        });
      });

      // Linear scale — toggle active button.
      $('.dfp-scale-buttons', context).once('dfp-scale-init', function () {
        var btns = $(this).find('.dfp-scale-btn');
        btns.bind('click', function () {
          btns.removeClass('dfp-scale-active');
          $(this).addClass('dfp-scale-active');
        });
      });

      // File upload — show selected filename(s) below the dropzone prompt.
      $('.dfp-file-hidden', context).once('dfp-file-name-init', function () {
        var fileInput   = $(this);
        var fileNameDiv = fileInput.closest('.dfp-file-dropzone').find('.dfp-file-name');
        fileInput.bind('change', function () {
          var files = this.files;
          if (!files || files.length === 0) {
            fileNameDiv.text('').hide();
            return;
          }
          var names = [];
          for (var i = 0; i < files.length; i++) {
            names.push(files[i].name);
          }
          fileNameDiv.text(names.join(', ')).show();
        });
      });
    }
  };

  // TinyMCE initialisation — only activates when the TinyMCE library is loaded.
  Drupal.behaviors.dfpTinymcePreview = {
    attach: function (context, settings) {
      $('.dfp-text-editor-area', context).once('dfp-tinymce-init', function () {
        if (typeof tinymce === 'undefined') { return; }
        tinymce.init({
          selector  : '#' + this.id,
          height    : 200,
          menubar   : false,
          statusbar : false,
          plugins   : 'lists link',
          toolbar   : 'bold italic underline strikethrough | bullist numlist blockquote | link'
        });
      });
    }
  };

  // Select2 dropdown (select type).
  Drupal.behaviors.dfpSelect2Preview = {
    attach: function (context, settings) {
      $('.dfp-select2', context).once('dfp-s2-init', function () {
        try {
          var isMulti = $(this).hasClass('dfp-select-multi');
          $(this).select2({
            placeholder:      isMulti ? 'Select one or more options…' : '— Select an option —',
            allowClear:       !isMulti,
            width:            '100%',
            dropdownCssClass: 'dfp-s2-preview-drop'
          });
        } catch (e) {}
      });
    }
  };

  // Select2 tags input — URL comes from Drupal.settings to avoid PHP in JS files.
  Drupal.behaviors.dfpTagsPreview = {
    attach: function (context, settings) {
      var autocompleteUrl = settings.dfPreview && settings.dfPreview.autocompleteUrl
        ? settings.dfPreview.autocompleteUrl
        : '';
      if (!autocompleteUrl) { return; }

      $('.dfp-tags-select2', context).once('dfp-tags-s2-init', function () {
        try {
          $(this).select2({
            tags:               [],
            tokenSeparators:    [','],
            width:              '100%',
            placeholder:        'Type to search or add tags…',
            minimumInputLength: 1,
            ajax: {
              url:         autocompleteUrl,
              dataType:    'json',
              quietMillis: 250,
              data: function (term, page) {
                return { term: term };
              },
              results: function (data) {
                // Legacy: plain array (no spellcheck).
                if ($.isArray(data)) { return { results: data }; }
                // Exact/prefix results — return directly.
                if (data.results && data.results.length > 0) {
                  return { results: data.results };
                }
                // Spellcheck suggestions — show as "Did you mean?" group.
                if (data.spellcheck && data.spellcheck.found && data.spellcheck.suggestions.length) {
                  return { results: [{ text: Drupal.t('Did you mean?'), children: data.spellcheck.suggestions }] };
                }
                return { results: [] };
              }
            },
            createSearchChoice: function (term, data) {
              var termLC = term.toLowerCase();
              for (var i = 0; i < data.length; i++) {
                var items = data[i].children ? data[i].children : [data[i]];
                for (var j = 0; j < items.length; j++) {
                  if (items[j].text && items[j].text.toLowerCase() === termLC) { return; }
                }
              }
              return { id: term, text: term };
            }
          });
        } catch (e) {}
      });
    }
  };

})(jQuery);
