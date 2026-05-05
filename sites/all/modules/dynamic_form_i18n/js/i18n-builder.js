(function ($, Drupal) {

  /* -------------------------------------------------------------------
     Boot: runs once Drupal settings are available.
     Drupal.settings.dfbi18n is injected by dynamic_form_i18n_init().
     ------------------------------------------------------------------- */
  Drupal.behaviors.dfbi18nBuilder = {
    attach: function (context, settings) {
      var s = settings.dfbi18n;
      if (!s) { return; }

      // Inject the language switcher bar above the builder header card.
      $('#dfb-builder-container', context).once('dfbi18n-switcher', function () {
        $(this).prepend(_dfbi18nBuildSwitcher(s));
      });

      if (!s.lang) { return; }

      // In translation mode — run once on initial page attach only.
      $('body').once('dfbi18n-mode-init', function () {
        // 1. Set a custom AJAX header on every request so PHP can detect
        //    the active language during Drupal form AJAX rebuilds and
        //    system/ajax form submissions — $.ajaxPrefilter needs jQuery 1.5+
        //    so we use the jQuery 1.4-compatible $.ajaxSetup instead.
        $.ajaxSetup({
          beforeSend: function (jqXHR) {
            jqXHR.setRequestHeader('X-DFB-I18N-Lang', s.lang);
          }
        });

        // 2. Structural restrictions: disable add / delete / reorder.
        _dfbi18nApplyRestrictions();

        // 3. Load all translations and update the builder page.
        var formId = settings.dynamicFormBuilder ? settings.dynamicFormBuilder.formId : null;
        if (formId) {
          _dfbi18nLoadAndApply(s, formId);
        }
      });
    }
  };

  /* -------------------------------------------------------------------
     Custom Drupal AJAX command handler.
     Called by the PHP _dfbi18n_question_translate_ajax_callback() to
     update a question card's translation-status badge after a save.
     ------------------------------------------------------------------- */
  Drupal.ajax.prototype.commands.dfbi18nCardUpdate = function (ajax, response) {
    var $card = $('.dfb-question-card[data-question-id="' + response.questionId + '"]');
    if (!$card.length) { return; }

    // Remove any existing status badge.
    $card.find('.dfbi18n-status-badge').remove();

    var $right = $card.find('.dfb-question-header-right');
    if (response.translated) {
      $right.prepend(
        '<span class="dfbi18n-status-badge dfbi18n-status-done">'
        + '&#10003; ' + response.lang
        + '</span>'
      );
    }
    else {
      $right.prepend(
        '<span class="dfbi18n-status-badge dfbi18n-status-needed">'
        + '&#9888; ' + Drupal.t('Needs translation')
        + '</span>'
      );
    }
  };

  /* -------------------------------------------------------------------
     Language switcher HTML builder.
     ------------------------------------------------------------------- */
  function _dfbi18nBuildSwitcher(s) {
    var html = '<div class="dfbi18n-switcher" id="dfbi18n-switcher">';
    html += '<span class="dfbi18n-switcher-label">' + Drupal.t('Language') + '</span>';
    html += '<div class="dfbi18n-lang-tabs">';

    // EN tab — strips ?lang= from the current URL.
    var enActive = !s.lang ? ' dfbi18n-tab-active' : '';
    html += '<a class="dfbi18n-lang-tab' + enActive + '" href="'
      + _dfbi18nUrlWithoutLang(s.currentPath) + '">EN</a>';

    // One tab per supported language.
    $.each(s.supportedLangs, function (code, label) {
      var active = (s.lang === code) ? ' dfbi18n-tab-active' : '';
      html += '<a class="dfbi18n-lang-tab' + active + '" href="'
        + _dfbi18nUrlWithLang(s.currentPath, code) + '" title="' + label + '">'
        + code.toUpperCase() + '</a>';
    });

    html += '</div>';

    if (s.lang) {
      html += '<span class="dfbi18n-mode-pill">'
        + '&#128274; ' + Drupal.t('Translation mode — structure is locked')
        + '</span>';
    }

    html += '</div>';
    return html;
  }

  /* -------------------------------------------------------------------
     Structural restrictions in translation mode.
     ------------------------------------------------------------------- */
  function _dfbi18nApplyRestrictions() {
    // Hide "Add Question" buttons.
    $('.dfb-btn-add-question').hide();

    // Hide section and question delete icons.
    $('.dfb-section-delete-icon, .dfb-question-delete-icon').hide();

    // Hide the inline section rename icon.
    $('.dfb-section-edit-icon').hide();

    // Disable drag handles visually and functionally by removing the
    // handle element's role — SortableJS will no longer find a valid handle.
    $('.dfb-drag-handle').addClass('dfbi18n-handle-disabled');

    // Also watch for new sections added via AJAX (though Add Section is hidden,
    // be defensive) by observing mutations — not available in all IE8 contexts,
    // so wrap in a check.
    if (window.MutationObserver) {
      var observer = new MutationObserver(function () {
        $('.dfb-btn-add-question').hide();
        $('.dfb-section-delete-icon, .dfb-question-delete-icon').hide();
        $('.dfb-section-edit-icon').hide();
        $('.dfb-drag-handle').addClass('dfbi18n-handle-disabled');
      });
      var container = document.getElementById('dfb-sections-wrapper');
      if (container) {
        observer.observe(container, { childList: true, subtree: true });
      }
    }

    // Hide the "Add Section" submit button.
    $('.dfb-btn-add-section').hide();
  }

  /* -------------------------------------------------------------------
     Load all translations for the form and apply to the builder.
     ------------------------------------------------------------------- */
  function _dfbi18nLoadAndApply(s, formId) {
    $.ajax({
      url:      s.basePath + '/form/' + formId + '/translations',
      type:     'GET',
      dataType: 'json',
      success: function (data) {
        // Update question cards.
        if (data.questions) {
          $.each(data.questions, function (qid, trans) {
            _dfbi18nUpdateCard(qid, trans, s.lang.toUpperCase());
          });
        }

        // Set up form-level (title + description) inline translation.
        _dfbi18nInitFormTranslation(s, formId, data.form || {});
      }
    });
  }

  /* -------------------------------------------------------------------
     Update a single question card with translation status.
     ------------------------------------------------------------------- */
  function _dfbi18nUpdateCard(qid, trans, langCode) {
    var $card = $('.dfb-question-card[data-question-id="' + qid + '"]');
    if (!$card.length) { return; }

    // Remove any previous badge.
    $card.find('.dfbi18n-status-badge').remove();
    var $right = $card.find('.dfb-question-header-right');

    if (trans.translated) {
      // Show German label; mark card as translated.
      var $label  = $card.find('.dfb-question-label');
      var reqStar = $label.find('.dfb-req').length
        ? ' <span class="dfb-req">*</span>'
        : '';
      $label.html(trans.label + reqStar);
      $right.prepend(
        '<span class="dfbi18n-status-badge dfbi18n-status-done">'
        + '&#10003; ' + langCode + '</span>'
      );
    }
    else {
      // Show English label in muted/italic style + warning badge.
      $card.find('.dfb-question-label').addClass('dfbi18n-label-untranslated');
      $right.prepend(
        '<span class="dfbi18n-status-badge dfbi18n-status-needed">'
        + '&#9888; ' + Drupal.t('Needs translation') + '</span>'
      );
    }
  }

  /* -------------------------------------------------------------------
     Form-level (title + description) inline translation panel.
     Renders an editable panel below the builder header card in DE mode.
     ------------------------------------------------------------------- */
  function _dfbi18nInitFormTranslation(s, formId, existing) {
    var langLabel = s.langLabel || s.lang.toUpperCase();
    var title     = existing.translated ? (existing.title       || '') : '';
    var desc      = existing.translated ? (existing.description || '') : '';

    // Original EN values from the page DOM.
    var enTitle = $.trim($('.dfb-builder-title').text());
    var enDesc  = $.trim($('.dfb-builder-desc').text());

    var panel =
      '<div class="dfbi18n-form-panel" id="dfbi18n-form-panel">'
      + '<div class="dfbi18n-form-panel-header">'
      + '<span class="dfbi18n-form-panel-icon">&#127760;</span>'
      + '<strong>' + Drupal.t('Form Translation (@lang)', { '@lang': langLabel }) + '</strong>'
      + '</div>'

      // Title field.
      + '<div class="dfbi18n-form-field">'
      + '<label class="dfbi18n-form-label">' + Drupal.t('Title') + '</label>'
      + '<input type="text" class="dfbi18n-title-input" value="' + _dfbi18nEscAttr(title)
      + '" placeholder="' + _dfbi18nEscAttr(Drupal.t('@lang title…', { '@lang': langLabel })) + '">'
      + '<div class="dfbi18n-original-hint"><strong>' + Drupal.t('English:') + '</strong> ' + _dfbi18nEscHtml(enTitle) + '</div>'
      + '</div>'

      // Description field.
      + '<div class="dfbi18n-form-field">'
      + '<label class="dfbi18n-form-label">' + Drupal.t('Description') + '</label>'
      + '<textarea class="dfbi18n-desc-input" placeholder="' + _dfbi18nEscAttr(Drupal.t('@lang description…', { '@lang': langLabel })) + '">'
      + _dfbi18nEscHtml(desc) + '</textarea>'
      + (enDesc ? '<div class="dfbi18n-original-hint"><strong>' + Drupal.t('English:') + '</strong> ' + _dfbi18nEscHtml(enDesc) + '</div>' : '')
      + '</div>'

      // Save button.
      + '<div class="dfbi18n-form-panel-footer">'
      + '<button type="button" class="dfbi18n-save-form-btn" id="dfbi18n-save-form-btn">'
      + Drupal.t('Save @lang Translation', { '@lang': langLabel })
      + '</button>'
      + '<span class="dfbi18n-save-status" id="dfbi18n-save-status"></span>'
      + '</div>'
      + '</div>';

    // Insert the panel after the builder header card.
    $('.dfb-builder-header-card').after(panel);

    // Save button handler.
    $('body').once('dfbi18n-form-save', function () {
      $(document).delegate('#dfbi18n-save-form-btn', 'click', function () {
        var $btn    = $(this);
        var $status = $('#dfbi18n-save-status');
        var titleVal = $.trim($('.dfbi18n-title-input').val());
        var descVal  = $.trim($('.dfbi18n-desc-input').val());

        if (!titleVal) {
          $status.text(Drupal.t('Title cannot be empty.')).addClass('dfbi18n-save-error').removeClass('dfbi18n-save-ok');
          return;
        }

        $btn.attr('disabled', 'disabled').text(Drupal.t('Saving…'));
        $status.text('').removeClass('dfbi18n-save-error dfbi18n-save-ok');

        $.ajax({
          url:         s.basePath + '/form/' + formId + '/save',
          type:        'POST',
          contentType: 'application/json',
          dataType:    'json',
          data:        JSON.stringify({ title: titleVal, description: descVal }),
          success: function (resp) {
            $btn.removeAttr('disabled').text(Drupal.t('Save @lang Translation', { '@lang': langLabel }));
            if (resp.status === 'success') {
              $status.text(Drupal.t('Saved.')).addClass('dfbi18n-save-ok').removeClass('dfbi18n-save-error');
            }
            else {
              $status.text(resp.message || Drupal.t('Error.')).addClass('dfbi18n-save-error').removeClass('dfbi18n-save-ok');
            }
          },
          error: function () {
            $btn.removeAttr('disabled').text(Drupal.t('Save @lang Translation', { '@lang': langLabel }));
            $status.text(Drupal.t('Network error.')).addClass('dfbi18n-save-error').removeClass('dfbi18n-save-ok');
          }
        });
      });
    });
  }

  /* -------------------------------------------------------------------
     URL helpers — jQuery 1.4.4 compatible (no URL API).
     ------------------------------------------------------------------- */
  function _dfbi18nUrlWithLang(url, lang) {
    // Strip existing lang param then append the new one.
    url = url.replace(/([?&])lang=[^&]*/g, '$1').replace(/[?&]$/, '');
    var sep = (url.indexOf('?') !== -1) ? '&' : '?';
    return url + sep + 'lang=' + lang;
  }

  function _dfbi18nUrlWithoutLang(url) {
    return url
      .replace(/([?&])lang=[^&]*/g, '$1')
      .replace(/[?&]$/, '')
      .replace(/\?&/, '?');
  }

  /* -------------------------------------------------------------------
     Minimal HTML/attribute escaping helpers.
     ------------------------------------------------------------------- */
  function _dfbi18nEscHtml(str) {
    return String(str)
      .replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;');
  }

  function _dfbi18nEscAttr(str) {
    return _dfbi18nEscHtml(str);
  }

})(jQuery, Drupal);
