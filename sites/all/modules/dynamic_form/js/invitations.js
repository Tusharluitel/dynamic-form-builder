/**
 * @file
 * Dynamic Form — invitation rows in the shared members table.
 *
 * Loads pending invitations via AJAX and renders them into #dfb-invites-tbody.
 * Exposes Drupal.dfbReloadInvitations() so members.js can trigger a reload
 * immediately after sending an invite from the search dropdown.
 *
 * jQuery 1.4.4 compatible (.bind(), .delegate(), .attr()).
 */
(function ($, Drupal) {

  Drupal.behaviors.dynamicFormInvitations = {
    attach: function (context, settings) {
      var $wrap = $('#dfb-members-table-wrap', context);
      if (!$wrap.length) { return; }

      var s = settings.dynamicFormInvitations || {};
      if (!s.listUrl) { return; }

      $wrap.once('dfb-invitations-init', function () {
        var listUrl    = s.listUrl    || '';
        var resendBase = s.resendBase || '';
        var revokeBase = s.revokeBase || '';

        Drupal.dfbReloadInvitations = function () { _loadInvitations(); };

        // Small delay so members.js finishes building the table first.
        setTimeout(function () { _loadInvitations(); }, 50);

        // ---- Resend / Revoke via event delegation -----------------------

        $wrap.delegate('.dfb-invite-resend-btn', 'click', function () {
          var id = $(this).attr('data-id');
          var $btn = $(this);
          $btn.attr('disabled', 'disabled').text(Drupal.t('Sending…'));
          $.ajax({
            url:      resendBase + '/' + id + '/resend',
            type:     'POST',
            dataType: 'json',
            success: function (resp) {
              _toast(resp.status === 'ok' ? 'success' : 'error', resp.message);
              if (resp.status === 'ok') { _loadInvitations(); }
              else { $btn.attr('disabled', null).text(Drupal.t('Resend')); }
            },
            error: function () {
              _toast('error', Drupal.t('Resend failed. Please try again.'));
              $btn.attr('disabled', null).text(Drupal.t('Resend'));
            }
          });
        });

        $wrap.delegate('.dfb-invite-revoke-btn', 'click', function () {
          if (!confirm(Drupal.t('Revoke this invitation? The link will stop working immediately.'))) { return; }
          var id = $(this).attr('data-id');
          var $btn = $(this);
          $btn.attr('disabled', 'disabled').text(Drupal.t('Revoking…'));
          $.ajax({
            url:      revokeBase + '/' + id + '/revoke',
            type:     'POST',
            dataType: 'json',
            success: function (resp) {
              _toast(resp.status === 'ok' ? 'success' : 'error', resp.message);
              if (resp.status === 'ok') { _loadInvitations(); }
              else { $btn.attr('disabled', null).text(Drupal.t('Revoke')); }
            },
            error: function () {
              _toast('error', Drupal.t('Revoke failed. Please try again.'));
              $btn.attr('disabled', null).text(Drupal.t('Revoke'));
            }
          });
        });

        // ---- Load & render ----------------------------------------------

        function _loadInvitations() {
          $.ajax({
            url:      listUrl,
            type:     'GET',
            dataType: 'json',
            success: function (resp) {
              if (resp.status === 'ok') { _renderInviteRows(resp.invitations || []); }
            }
          });
        }

        function _renderInviteRows(invitations) {
          var $tbody = $('#dfb-invites-tbody');
          if (!$tbody.length) { return; }
          $tbody.empty();

          var pending = [];
          $.each(invitations, function (i, inv) {
            if (inv.status === 'pending') { pending.push(inv); }
          });

          if (!pending.length) { return; }

          $.each(pending, function (i, inv) {
            var expiresIn  = _expiresIn(inv.expires_at);
            var nearExpiry = (inv.expires_at - _now()) < 86400;

            var $row = $(
              '<tr class="dfb-member-row dfb-invite-row" data-invite-id="' + inv.id + '">' +
                '<td class="dfb-col-member">' +
                  '<div class="dfb-member-identity">' +
                    '<div class="dfb-avatar dfb-avatar-invite">&#9993;</div>' +
                    '<div class="dfb-member-info">' +
                      '<span class="dfb-member-mail dfb-invite-email">' + _esc(inv.email) + '</span>' +
                      '<span class="dfb-invite-meta">' +
                        Drupal.t('Invited by @name · @ago', {
                          '@name': inv.invited_by_name,
                          '@ago':  _timeAgo(inv.created_at)
                        }) +
                      '</span>' +
                    '</div>' +
                  '</div>' +
                '</td>' +
                '<td class="dfb-col-role">' +
                  '<span class="dfb-role-text">' + _esc(inv.role_label) + '</span>' +
                '</td>' +
                '<td class="dfb-col-status">' +
                  '<span class="dfb-status-pill dfb-status-pending">' + Drupal.t('Pending') + '</span>' +
                  '<span class="dfb-expires-note' + (nearExpiry ? ' dfb-expires-soon' : '') + '">' +
                    Drupal.t('Expires @when', { '@when': expiresIn }) +
                  '</span>' +
                '</td>' +
                '<td class="dfb-col-actions">' +
                  '<button type="button" class="dfb-invite-resend-btn" data-id="' + inv.id + '">' +
                    Drupal.t('Resend') +
                  '</button>' +
                  '<button type="button" class="dfb-invite-revoke-btn" data-id="' + inv.id + '">' +
                    Drupal.t('Revoke') +
                  '</button>' +
                '</td>' +
              '</tr>'
            );

            $tbody.append($row);
          });
        }

        // ---- Utilities --------------------------------------------------

        function _toast(type, msg) {
          if (window.DFBToast && DFBToast[type]) { DFBToast[type](msg); }
        }

        function _esc(str) {
          return $('<div>').text(str || '').html();
        }

        function _now() {
          return Math.floor(new Date().getTime() / 1000);
        }

        function _timeAgo(ts) {
          var diff = _now() - ts;
          if (diff < 60)    { return Drupal.t('just now'); }
          if (diff < 3600)  { return Math.floor(diff / 60)   + ' ' + Drupal.t('min ago'); }
          if (diff < 86400) { return Math.floor(diff / 3600) + ' ' + Drupal.t('hr ago'); }
          return Math.floor(diff / 86400) + ' ' + Drupal.t('d ago');
        }

        function _expiresIn(ts) {
          var diff = ts - _now();
          if (diff <= 0)    { return Drupal.t('expired'); }
          if (diff < 3600)  { return Drupal.t('< 1 hr'); }
          if (diff < 86400) { return Math.floor(diff / 3600) + ' ' + Drupal.t('hr'); }
          return Math.floor(diff / 86400) + ' ' + Drupal.t('d'); }
      });
    }
  };

})(jQuery, Drupal);
