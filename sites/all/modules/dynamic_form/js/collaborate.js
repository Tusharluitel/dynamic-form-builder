(function ($, Drupal) {

  // -----------------------------------------------------------------------
  // Drupal behavior: attaches once on the builder page.
  // -----------------------------------------------------------------------
  Drupal.behaviors.dfbCollaborate = {
    attach: function (context, settings) {
      if (!document.getElementById('dfb-builder-container')) { return; }

      var s = settings.dynamicFormBuilder;
      if (!s || !s.formId || !s.currentUser) { return; }

      $('body').once('dfb-collaborate', function () {
        dfbCollab.init(s.formId, s.currentUser, s.wsUrl || 'ws://localhost:8080');
      });
    }
  };

  // -----------------------------------------------------------------------
  // Core collaboration object.
  // -----------------------------------------------------------------------
  var dfbCollab = {
    ws:             null,
    formId:         null,
    user:           null,
    wsUrl:          null,
    reconnectTimer: null,

    init: function (formId, user, wsUrl) {
      this.formId = formId;
      this.user   = user;
      this.wsUrl  = wsUrl;
      this._renderPresenceBar();
      this.connect();
      this._bindBroadcastHooks();
    },

    // --- Connection ---

    connect: function () {
      var self = this;
      try {
        this.ws = new WebSocket(this.wsUrl);
      } catch (e) {
        console.warn('[Collab] WebSocket not available:', e);
        return;
      }

      this.ws.onopen = function () {
        clearTimeout(self.reconnectTimer);
        self.ws.send(JSON.stringify({
          type:    'join',
          form_id: self.formId,
          uid:     self.user.uid,
          name:    self.user.name
        }));
      };

      this.ws.onmessage = function (e) {
        var msg;
        try { msg = JSON.parse(e.data); } catch (ex) { return; }
        self._handleMessage(msg);
      };

      this.ws.onclose = function () {
        self.reconnectTimer = setTimeout(function () { self.connect(); }, 3000);
      };

      this.ws.onerror = function () {
        self.ws.close();
      };
    },

    send: function (data) {
      if (this.ws && this.ws.readyState === WebSocket.OPEN) {
        this.ws.send(JSON.stringify(data));
      }
    },

    // --- Inbound message handler ---

    _handleMessage: function (msg) {
      switch (msg.type) {
        case 'presence':
          this._updatePresenceBar(msg.users || []);
          break;
        case 'section_renamed':
          this._applySectionRename(msg.section_id, msg.name, msg.sender_name);
          break;
        case 'deleted':
          this._applyDelete(msg.entity_type, msg.entity_id, msg.sender_name);
          break;
        case 'content_changed':
          this._reloadSections(msg.sender_name);
          break;
      }
    },

    // --- Broadcast hooks (called after local changes succeed) ---

    _bindBroadcastHooks: function () {
      var self = this;

      $('body').on('dfb:section_renamed', function (e, sectionId, name) {
        self.send({ type: 'section_renamed', form_id: self.formId, section_id: sectionId, name: name });
      });

      $('body').on('dfb:deleted', function (e, entityType, entityId) {
        self.send({ type: 'deleted', form_id: self.formId, entity_type: entityType, entity_id: entityId });
      });

      $('body').on('dfb:content_changed', function () {
        self.send({ type: 'content_changed', form_id: self.formId });
      });
    },

    // --- DOM updates from remote events ---

    _applySectionRename: function (sectionId, name, sender) {
      var $card = $('.dfb-section-card[data-section-id="' + sectionId + '"]');
      if ($card.length) {
        $card.find('.dfb-section-title').text(name);
        $card.find('[data-section-name]').attr('data-section-name', name);
      }
      if (window.DFBToast) {
        DFBToast.info((sender || Drupal.t('Someone')) + Drupal.t(' renamed a section.'));
      }
    },

    _applyDelete: function (entityType, entityId, sender) {
      var $card;
      if (entityType === 'section') {
        $card = $('.dfb-section-card[data-section-id="' + entityId + '"]');
      } else {
        $card = $('.dfb-question-card[data-question-id="' + entityId + '"]');
      }
      if ($card && $card.length) { $card.remove(); }
      if (window.DFBToast) {
        DFBToast.info((sender || Drupal.t('Someone')) + Drupal.t(' deleted a ') + entityType + '.');
      }
    },

    _reloadSections: function (sender) {
      var basePath = Drupal.settings.dynamicFormBuilder.basePath;
      var formId   = this.formId;

      $.ajax({
        url:      basePath + '/sections/' + formId,
        type:     'GET',
        dataType: 'json',
        success: function (commands) {
          if (!$.isArray(commands)) { return; }
          var fakeAjax = {
            wrapper: '#dfb-sections-wrapper',
            method:  'html',
            effect:  'none',
            speed:   'none',
            getEffect: function () {
              return { showEffect: 'show', hideEffect: 'hide', showSpeed: '' };
            }
          };
          $.each(commands, function (i, cmd) {
            if (Drupal.ajax && Drupal.ajax.prototype.commands[cmd.command]) {
              Drupal.ajax.prototype.commands[cmd.command](fakeAjax, cmd, null);
            }
          });
          Drupal.attachBehaviors(document, Drupal.settings);
        }
      });

      if (window.DFBToast) {
        DFBToast.info((sender || Drupal.t('Someone')) + Drupal.t(' updated the form.'));
      }
    },

    // --- Presence bar ---

    _renderPresenceBar: function () {
      if ($('#dfb-collab-bar').length) { return; }
      var $bar = $('<div id="dfb-collab-bar" class="dfb-collab-bar">'
        + '<span class="dfb-collab-label">' + Drupal.t('Online') + '</span>'
        + '<span class="dfb-collab-avatars"></span>'
        + '<span class="dfb-collab-status dfb-collab-connecting">' + Drupal.t('Connecting…') + '</span>'
        + '</div>');
      $('#dfb-builder-container').prepend($bar);
    },

    _updatePresenceBar: function (users) {
      var self   = this;
      var $bar   = $('#dfb-collab-bar');
      var html   = '';

      users.forEach(function (u) {
        var initial  = (u.name || '?').charAt(0).toUpperCase();
        var isMe     = (u.uid === self.user.uid);
        var title    = isMe ? u.name + ' (' + Drupal.t('you') + ')' : u.name;
        var extraCls = isMe ? ' dfb-collab-avatar-me' : '';
        html += '<span class="dfb-collab-avatar' + extraCls + '"'
              + ' style="background:' + u.color + '"'
              + ' title="' + title + '">'
              + initial
              + '</span>';
      });

      $bar.find('.dfb-collab-avatars').html(html);
      $bar.find('.dfb-collab-status').remove();
    }
  };

  // Expose so builder.js trigger calls work after the behavior has attached.
  window.dfbCollab = dfbCollab;

})(jQuery, Drupal);
