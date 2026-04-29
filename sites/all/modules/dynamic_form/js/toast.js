/**
 * @file
 * Dynamic Form — DFBToast notification utility.
 *
 * Zero external dependencies. Uses only jQuery 1.4.4-compatible methods
 * (.bind, .animate, .slideDown, .slideUp) so it works on Drupal 7 out of
 * the box without the jQuery Update module.
 *
 * Public API (window.DFBToast):
 *   DFBToast.success(msg [, durationMs])
 *   DFBToast.error(msg   [, durationMs])
 *   DFBToast.warning(msg [, durationMs])
 *   DFBToast.info(msg    [, durationMs])
 *
 * PHP helper (in AJAX callbacks):
 *   $commands[] = ajax_command_dfb_toast('success', t('Saved.'));
 */
(function ($, Drupal, window) {

  'use strict';

  /* ------------------------------------------------------------------ */
  /* Constants                                                            */
  /* ------------------------------------------------------------------ */

  var CONTAINER_ID = 'dfb-toast-container';

  var ICONS = {
    success: '&#10003;',
    error:   '&#10007;',
    warning: '&#9888;',
    info:    '&#8505;',
  };

  // Filled with translated strings once Drupal is ready.
  var HEADINGS = {
    success: 'Success',
    error:   'Error',
    warning: 'Warning',
    info:    'Info',
  };

  var DEFAULT_DURATION = {
    success: 4500,
    error:   7000,
    warning: 5000,
    info:    4500,
  };

  /* ------------------------------------------------------------------ */
  /* Internal helpers                                                     */
  /* ------------------------------------------------------------------ */

  function _getContainer() {
    var $c = $('#' + CONTAINER_ID);
    if (!$c.length) {
      $c = $('<div></div>').attr('id', CONTAINER_ID).appendTo('body');
    }
    return $c;
  }

  function _esc(str) {
    return $('<div>').text(str).html();
  }

  function _dismiss($toast) {
    $toast.slideUp(200, function () { $(this).remove(); });
  }

  function _show(type, msg, duration) {
    var heading = _esc(HEADINGS[type] || type);
    var icon    = ICONS[type] || 'i';
    var ms      = duration || DEFAULT_DURATION[type] || 4500;

    var $toast = $(
      '<div class="dfb-toast dfb-toast-' + type + '">' +
        '<span class="dfb-toast-icon">' + icon + '</span>' +
        '<div class="dfb-toast-body">' +
          '<div class="dfb-toast-heading">' + heading + '</div>' +
          '<div class="dfb-toast-message">' + msg + '</div>' +
        '</div>' +
        '<button class="dfb-toast-close" type="button">&times;</button>' +
        '<div class="dfb-toast-progress"></div>' +
      '</div>'
    );

    $toast.hide();
    _getContainer().append($toast);
    $toast.slideDown(200);

    // Shrink progress bar over the toast lifetime.
    $toast.find('.dfb-toast-progress').animate(
      { width: '0%' },
      { duration: ms, easing: 'linear' }
    );

    // Auto-dismiss.
    var timer = setTimeout(function () { _dismiss($toast); }, ms);

    // Manual close — .bind() for jQuery 1.4.4 compatibility.
    $toast.find('.dfb-toast-close').bind('click', function () {
      clearTimeout(timer);
      _dismiss($toast);
    });
  }

  /* ------------------------------------------------------------------ */
  /* Public API                                                           */
  /* ------------------------------------------------------------------ */

  var DFBToast = {
    success: function (msg, duration) { _show('success', msg, duration); },
    error:   function (msg, duration) { _show('error',   msg, duration); },
    warning: function (msg, duration) { _show('warning', msg, duration); },
    info:    function (msg, duration) { _show('info',    msg, duration); },
  };

  window.DFBToast = DFBToast;

  /* ------------------------------------------------------------------ */
  /* Bootstrap on DOM ready                                               */
  /* ------------------------------------------------------------------ */

  $(document).ready(function () {

    // Translate headings now that Drupal.t is available.
    HEADINGS = {
      success: Drupal.t('Success'),
      error:   Drupal.t('Error'),
      warning: Drupal.t('Warning'),
      info:    Drupal.t('Info'),
    };

    // Register custom Drupal AJAX command so PHP callbacks can fire toasts.
    // Usage: $commands[] = ajax_command_dfb_toast('success', t('Saved.'));
    if (Drupal.ajax && Drupal.ajax.prototype && Drupal.ajax.prototype.commands) {
      Drupal.ajax.prototype.commands.dfbToast = function (ajax, response) {
        var type = response.toast_type || 'info';
        if (typeof DFBToast[type] === 'function') {
          DFBToast[type](response.message || '');
        }
      };
    }
  });

  /* ------------------------------------------------------------------ */
  /* Drupal behavior: convert server-side .messages divs to toasts       */
  /* ------------------------------------------------------------------ */

  Drupal.behaviors.dfbToast = {
    attach: function (context) {
      $('div.messages', context).once('dfb-toast-convert', function () {
        var $el   = $(this);
        var items = [];
        $el.find('ul li').each(function () {
          var t = $.trim($(this).text());
          if (t) { items.push(t); }
        });
        var text = items.length ? items.join(' ') : $.trim($el.text());
        if (!text) { return; }

        if ($el.hasClass('status'))       { DFBToast.success(text); }
        else if ($el.hasClass('warning')) { DFBToast.warning(text); }
        else if ($el.hasClass('error'))   { DFBToast.error(text);   }
        else                              { DFBToast.info(text);    }

        $el.remove();
      });
    }
  };

})(jQuery, Drupal, window);
