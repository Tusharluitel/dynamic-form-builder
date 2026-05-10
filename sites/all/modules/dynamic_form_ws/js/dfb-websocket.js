/**
 * @file
 * Client-side collaborative presence manager for the Dynamic Form Builder.
 *
 * Connects to the Ratchet WebSocket server, tracks which section/question
 * each collaborator is interacting with, and renders presence avatars +
 * colored element highlights in real time (Figma-style).
 */

(function ($) {
  'use strict';

  // -------------------------------------------------------------------------
  // Drupal behavior entry point
  // -------------------------------------------------------------------------

  Drupal.behaviors.dfbWebSocket = {
    attach: function (context, settings) {
      if (!settings.dfbWebSocket) { return; }
      $('#dfb-builder-container', context).once('dfb-ws', function () {
        DfbPresence.init(settings.dfbWebSocket);
      });
    }
  };

  // -------------------------------------------------------------------------
  // DfbPresence — all state and logic is private to this object
  // -------------------------------------------------------------------------

  var DfbPresence = {

    ws:        null,
    config:    null,
    users:     {},        // uid → { uid, name, color, cursor }
    reconnectDelay: 1000, // ms; doubles on each failure up to maxDelay
    maxDelay:       30000,
    reconnectTimer: null,
    cursorThrottle: null,

    // -----------------------------------------------------------------------
    // Init
    // -----------------------------------------------------------------------

    init: function (config) {
      this.config = config;
      this._buildPresenceBar();
      this.connect();
      this._bindCursorTracking();
    },

    // -----------------------------------------------------------------------
    // WebSocket lifecycle
    // -----------------------------------------------------------------------

    connect: function () {
      var self = this;

      if (this.ws) {
        try { this.ws.close(); } catch (e) {}
      }

      this.ws = new WebSocket(this.config.wsUrl);

      this.ws.onopen = function () {
        self.reconnectDelay = 1000; // reset backoff on successful connect
        self.ws.send(JSON.stringify({ type: 'auth', token: self.config.token }));
      };

      this.ws.onmessage = function (evt) {
        var msg;
        try { msg = JSON.parse(evt.data); } catch (e) { return; }
        self._handleMessage(msg);
      };

      this.ws.onclose = function () {
        self._showDisconnected();
        self.reconnect();
      };

      this.ws.onerror = function () {
        // onclose fires after onerror; reconnect is handled there.
      };
    },

    reconnect: function () {
      var self = this;
      if (this.reconnectTimer) { return; }
      this.reconnectTimer = setTimeout(function () {
        self.reconnectTimer = null;
        self.connect();
      }, this.reconnectDelay);
      // Exponential backoff capped at maxDelay.
      this.reconnectDelay = Math.min(this.reconnectDelay * 2, this.maxDelay);
    },

    // -----------------------------------------------------------------------
    // Incoming message routing
    // -----------------------------------------------------------------------

    _handleMessage: function (msg) {
      switch (msg.type) {
        case 'auth_ok':
          this._showConnected();
          break;

        case 'auth_error':
          console.warn('[DFB WS] Auth error:', msg.message);
          break;

        case 'presence_state':
          var self = this;
          $.each(msg.users, function (i, u) {
            self.users[u.uid] = u;
          });
          this._renderPresenceBar();
          break;

        case 'user_joined':
          this.users[msg.user.uid] = msg.user;
          this._renderPresenceBar();
          this._showToast(msg.user.name + ' joined');
          break;

        case 'user_left':
          if (this.users[msg.uid]) {
            this._clearHighlight(msg.uid);
            delete this.users[msg.uid];
          }
          this._renderPresenceBar();
          break;

        case 'cursor_update':
          if (this.users[msg.uid]) {
            this.users[msg.uid].cursor = msg.cursor;
          }
          this._applyHighlight(msg.uid, msg.cursor);
          break;
      }
    },

    // -----------------------------------------------------------------------
    // Cursor tracking — delegated + throttled
    // -----------------------------------------------------------------------

    _bindCursorTracking: function () {
      var self = this;

      // Hovering a section card.
      $(document).on('mouseenter.dfbWs', '.dfb-section-card', function () {
        var sectionId = parseInt($(this).data('section-id'), 10) || null;
        self._sendCursor({ section_id: sectionId, question_id: null, action: 'hovering' });
      });

      // Clicking (focusing) a question card triggers "editing".
      $(document).on('click.dfbWs', '.dfb-question-editable', function () {
        var $card      = $(this).closest('[data-question-id]');
        var $section   = $(this).closest('[data-section-id]');
        var questionId = parseInt($card.data('question-id'), 10) || null;
        var sectionId  = parseInt($section.data('section-id'), 10) || null;
        self._sendCursor({ section_id: sectionId, question_id: questionId, action: 'editing' });
      });

      // Opening the add/edit question modal — report editing state.
      $(document).on('click.dfbWs', '[data-action="open-question-modal"], [data-action="edit-question"]', function () {
        var $section  = $(this).closest('[data-section-id]');
        var sectionId = parseInt($section.data('section-id'), 10) || null;
        var qId       = parseInt($(this).data('question-id'), 10) || null;
        self._sendCursor({ section_id: sectionId, question_id: qId, action: 'editing' });
      });

      // Mouse leaving the builder → go idle.
      $(document).on('mouseleave.dfbWs', '#dfb-builder-container', function () {
        self._sendIdle();
      });
    },

    _sendCursor: function (data) {
      var self = this;
      if (this.cursorThrottle) { return; }
      this.cursorThrottle = setTimeout(function () {
        self.cursorThrottle = null;
      }, 100);
      this._wsSend($.extend({ type: 'cursor' }, data));
    },

    _sendIdle: function () {
      this._wsSend({ type: 'idle' });
    },

    _wsSend: function (payload) {
      if (this.ws && this.ws.readyState === WebSocket.OPEN) {
        this.ws.send(JSON.stringify(payload));
      }
    },

    // -----------------------------------------------------------------------
    // Presence bar
    // -----------------------------------------------------------------------

    _buildPresenceBar: function () {
      var $bar = $('#dfb-presence-bar');
      if ($bar.length === 0) {
        $bar = $('<div id="dfb-presence-bar" class="dfb-presence-bar"></div>');
        $('#dfb-builder-container').prepend($bar);
      }
      $bar.show();
    },

    _renderPresenceBar: function () {
      var $bar = $('#dfb-presence-bar');
      $bar.empty();

      var label = $('<span class="dfb-presence-label">Online:</span>');
      $bar.append(label);

      var self = this;
      $.each(this.users, function (uid, u) {
        var initials = self._initials(u.name);
        var $avatar  = $(
          '<div class="dfb-presence-avatar" ' +
          'data-dfb-uid="' + parseInt(uid, 10) + '" ' +
          'title="' + self._escAttr(u.name) + '" ' +
          'style="background-color:' + self._escAttr(u.color) + '">' +
          initials +
          '</div>'
        );
        $bar.append($avatar);
      });

      // Hide the label when no collaborators are present.
      if ($.isEmptyObject(this.users)) {
        label.hide();
      }
    },

    _showDisconnected: function () {
      $('#dfb-presence-bar').addClass('dfb-presence-disconnected');
    },

    _showConnected: function () {
      $('#dfb-presence-bar').removeClass('dfb-presence-disconnected');
    },

    // -----------------------------------------------------------------------
    // Element highlights
    // -----------------------------------------------------------------------

    _applyHighlight: function (uid, cursor) {
      // Remove any existing highlight for this user.
      this._clearHighlight(uid);

      if (!cursor || cursor.action === 'idle' || cursor.action === 'viewing') {
        return;
      }

      var user  = this.users[uid];
      if (!user) { return; }

      // Determine the target element: question card first, then section card.
      var $target = null;
      if (cursor.question_id) {
        $target = $('[data-question-id="' + cursor.question_id + '"]');
      }
      if ((!$target || !$target.length) && cursor.section_id) {
        $target = $('[data-section-id="' + cursor.section_id + '"]').first();
      }
      if (!$target || !$target.length) { return; }

      var self       = this;
      var initials   = this._initials(user.name);
      var label      = this._escHtml(user.name) + ' is ' + this._escHtml(cursor.action);

      $target
        .addClass('dfb-presence-highlight')
        .attr('data-dfb-presence-uid', uid)
        .css('--dfb-presence-color', user.color);

      // Small avatar badge on the card.
      var $badge = $(
        '<div class="dfb-presence-badge" data-dfb-badge-uid="' + uid + '" ' +
        'title="' + this._escAttr(label) + '" ' +
        'style="background-color:' + this._escAttr(user.color) + '">' +
        initials +
        '</div>'
      );
      $target.append($badge);

      // Auto-clear after 5 s of no updates from this user on this element.
      var timer = setTimeout(function () {
        self._clearHighlight(uid);
      }, 5000);
      $target.data('dfb-presence-timer-' + uid, timer);
    },

    _clearHighlight: function (uid) {
      var $el = $('[data-dfb-presence-uid="' + uid + '"]');
      if (!$el.length) { return; }

      var timer = $el.data('dfb-presence-timer-' + uid);
      if (timer) { clearTimeout(timer); }

      $el
        .removeClass('dfb-presence-highlight')
        .removeAttr('data-dfb-presence-uid')
        .css('--dfb-presence-color', '');

      $('[data-dfb-badge-uid="' + uid + '"]').remove();
    },

    // -----------------------------------------------------------------------
    // Toast (reuse the existing dfbToast if available, else console.log)
    // -----------------------------------------------------------------------

    _showToast: function (message) {
      if (window.dfbToast) {
        dfbToast.show('info', message);
      }
    },

    // -----------------------------------------------------------------------
    // Utilities
    // -----------------------------------------------------------------------

    _initials: function (name) {
      if (!name) { return '?'; }
      var parts = name.trim().split(/\s+/);
      if (parts.length >= 2) {
        return (parts[0][0] + parts[parts.length - 1][0]).toUpperCase();
      }
      return name.substring(0, 2).toUpperCase();
    },

    _escAttr: function (str) {
      return String(str)
        .replace(/&/g, '&amp;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#39;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;');
    },

    _escHtml: function (str) {
      return String(str)
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;');
    }
  };

})(jQuery);
