/**
 * @file
 * Dynamic Form — unified member picker behaviour.
 *
 * Handles both registered-user search (Add) and unregistered-email invite.
 * Selected members are displayed in a shared table alongside pending
 * invitations rendered by invitations.js.
 *
 * jQuery 1.4.4 compatible (.bind(), .delegate(), .attr()).
 */
(function ($, Drupal) {

  // 8-color palette for avatar initials
  var AVATAR_COLORS = [
    '#6366f1', '#0ea5e9', '#10b981', '#f59e0b',
    '#ef4444', '#8b5cf6', '#ec4899', '#14b8a6'
  ];

  Drupal.behaviors.dynamicFormMembers = {
    attach: function (context, settings) {
      var $search = $('#dfb-members-search', context);
      if (!$search.length) { return; }

      $search.once('dfb-members-init', function () {
        var s         = settings.dynamicFormMembers || {};
        var searchUrl = s.searchUrl || '';
        var canInvite = !!s.canInvite;

        var $hidden  = $('input[name="members_selected"]');
        var $results = $('#dfb-members-results');

        var members = [];
        try { members = JSON.parse($hidden.val() || '[]'); }
        catch (e) { members = []; }

        _ensureTable();
        _renderMemberRows();

        var searchTimer = null;

        // ---- Search input -----------------------------------------------

        $search.bind('keyup', function () {
          var q = $.trim($(this).val());
          clearTimeout(searchTimer);

          if (q.length < 2) {
            $results.hide().empty();
            return;
          }

          searchTimer = setTimeout(function () { _doSearch(q); }, 300);
        });

        function _doSearch(q) {
          $.ajax({
            url:      searchUrl,
            type:     'GET',
            dataType: 'json',
            data:     { s: q },
            success:  function (users) { _renderDropdown(users, q); },
            error:    function () { $results.hide().empty(); }
          });
        }

        $(document).bind('click.dfbMembers', function (e) {
          if (!$(e.target).closest('#dfb-members-search, #dfb-members-results, #dfb-member-add-btn').length) {
            $results.hide();
          }
        });

        // ---- Add button -------------------------------------------------

        var $addBtn = $('#dfb-member-add-btn');

        $search.bind('keyup change input', function () {
          $addBtn.attr('disabled', $.trim($(this).val()).length < 2 ? 'disabled' : null);
        });
        if ($.trim($search.val()).length < 2) { $addBtn.attr('disabled', 'disabled'); }

        $addBtn.bind('click', function () {
          var q = $.trim($search.val());
          if (q.length < 2) { return; }

          var role = $('#dfb-member-role-select').val() || 'editor';
          var $items = $results.find('.dfb-member-result-item');

          if ($results.is(':visible') && $items.length) {
            $items.first().trigger('click');
            return;
          }

          if (canInvite && _isEmail(q)) {
            _sendInvite(q.toLowerCase(), role);
            $search.val('').focus();
            $addBtn.attr('disabled', 'disabled');
            return;
          }

          _doSearch(q);
        });

        // ---- Dropdown ---------------------------------------------------

        function _renderDropdown(users, query) {
          $results.empty();

          var available = [];
          $.each(users, function (i, u) {
            var already = false;
            $.each(members, function (j, m) {
              if (m.uid === u.uid) { already = true; return false; }
            });
            if (!already) { available.push(u); }
          });

          $.each(available, function (i, u) {
            var initials = _avatarInitial(u.name || u.mail);
            var color    = _avatarColor(u.name || u.mail);
            var $item = $(
              '<div class="dfb-member-result-item">' +
                '<div class="dfb-result-identity">' +
                  '<span class="dfb-result-name">' + _esc(u.name) + '</span>' +
                  '<span class="dfb-result-mail">' + _esc(u.mail) + '</span>' +
                '</div>' +
                '<span class="dfb-result-badge dfb-badge-add">' + Drupal.t('Add') + '</span>' +
              '</div>'
            );
            $item.bind('click', function () {
              var role = $('#dfb-member-role-select').val() || 'editor';
              _addMember(u.uid, u.name, u.mail, role);
              $results.hide().empty();
              $search.val('').focus();
              $addBtn.attr('disabled', 'disabled');
            });
            $results.append($item);
          });

          if (canInvite && _isEmail(query)) {
            var emailNorm  = query.toLowerCase();
            var exactMatch = false;
            $.each(users, function (i, u) {
              if (u.mail && u.mail.toLowerCase() === emailNorm) {
                exactMatch = true; return false;
              }
            });

            if (!exactMatch) {
              var $invite = $(
                '<div class="dfb-member-result-item dfb-result-item-invite">' +
                  '<div class="dfb-result-identity">' +
                    '<span class="dfb-result-mail">' + _esc(query) + '</span>' +
                    '<span class="dfb-result-hint">' + Drupal.t('No account — send invite link') + '</span>' +
                  '</div>' +
                  '<span class="dfb-result-badge dfb-badge-invite">' + Drupal.t('Invite') + '</span>' +
                '</div>'
              );
              $invite.bind('click', function () {
                var role = $('#dfb-member-role-select').val() || 'editor';
                _sendInvite(emailNorm, role);
                $results.hide().empty();
                $search.val('').focus();
                $addBtn.attr('disabled', 'disabled');
              });
              $results.append($invite);
            }
          }

          if ($results.children().length) {
            $results.show();
          } else {
            $results.append(
              '<div class="dfb-member-no-results">' + Drupal.t('No users found.') + '</div>'
            ).show();
          }
        }

        // ---- Table ------------------------------------------------------

        function _ensureTable() {
          var $wrap = $('#dfb-members-table-wrap');
          if ($wrap.find('#dfb-members-table').length) { return; }
          $wrap.html(
            '<table id="dfb-members-table" class="dfb-members-table">' +
              '<thead>' +
                '<tr>' +
                  '<th class="dfb-th-member">' + Drupal.t('Member') + '</th>' +
                  '<th class="dfb-th-role">'   + Drupal.t('Role')   + '</th>' +
                  '<th class="dfb-th-status">' + Drupal.t('Status') + '</th>' +
                  '<th class="dfb-th-actions"></th>' +
                '</tr>' +
              '</thead>' +
              '<tbody id="dfb-members-tbody"></tbody>' +
              '<tbody id="dfb-invites-tbody"></tbody>' +
            '</table>'
          );
        }

        function _renderMemberRows() {
          _ensureTable();
          var $tbody = $('#dfb-members-tbody').empty();

          if (!members.length) {
            $tbody.append(
              '<tr class="dfb-row-empty">' +
                '<td colspan="4" class="dfb-empty-cell">' +
                  '<div class="dfb-empty-state">' +
                    '<div class="dfb-empty-icon">&#128100;</div>' +
                    '<div class="dfb-empty-text">' + Drupal.t('No members added yet') + '</div>' +
                    '<div class="dfb-empty-hint">' + Drupal.t('Search above to add collaborators') + '</div>' +
                  '</div>' +
                '</td>' +
              '</tr>'
            );
            return;
          }

          $.each(members, function (i, m) {
            var initials = _avatarInitial(m.name || m.mail);
            var color    = _avatarColor(m.name || m.mail);
            var $row = $(
              '<tr class="dfb-member-row" data-uid="' + m.uid + '">' +
                '<td class="dfb-col-member">' +
                  '<div class="dfb-member-identity">' +
                    '<div class="dfb-member-info">' +
                      '<span class="dfb-member-name">' + _esc(m.name || '') + '</span>' +
                      (m.mail ? '<span class="dfb-member-mail">' + _esc(m.mail) + '</span>' : '') +
                    '</div>' +
                  '</div>' +
                '</td>' +
                '<td class="dfb-col-role">' +
                  '<select class="dfb-row-role-select">' + _buildRoleOpts(m.role) + '</select>' +
                '</td>' +
                '<td class="dfb-col-status">' +
                  '<span class="dfb-status-pill dfb-status-member">' + Drupal.t('Member') + '</span>' +
                '</td>' +
                '<td class="dfb-col-actions">' +
                  '<button type="button" class="dfb-row-remove-btn" data-uid="' + m.uid + '">' +
                    Drupal.t('Remove') +
                  '</button>' +
                '</td>' +
              '</tr>'
            );

            $row.find('.dfb-row-role-select').bind('change', (function (uid) {
              return function () { _changeRole(uid, $(this).val()); };
            }(m.uid)));

            $row.find('.dfb-row-remove-btn').bind('click', (function (uid) {
              return function () { _removeMember(uid); };
            }(m.uid)));

            $tbody.append($row);
          });
        }

        function _buildRoleOpts(selected) {
          var roles = [
            ['editor',         Drupal.t('Editor')],
            ['form_applicant', Drupal.t('Form Applicant')],
            ['viewer',         Drupal.t('Viewer')],
            ['reviewer',       Drupal.t('Reviewer')]
          ];
          var html = '';
          $.each(roles, function (i, r) {
            html += '<option value="' + r[0] + '"' + (r[0] === selected ? ' selected' : '') + '>' + r[1] + '</option>';
          });
          return html;
        }

        // ---- Member list mutations --------------------------------------

        function _addMember(uid, name, mail, role) {
          members.push({ uid: uid, name: name, mail: mail || '', role: role });
          _renderMemberRows();
          _sync();
        }

        function _removeMember(uid) {
          members = $.grep(members, function (m) { return m.uid !== uid; });
          _renderMemberRows();
          _sync();
        }

        function _changeRole(uid, newRole) {
          $.each(members, function (i, m) {
            if (m.uid === uid) { m.role = newRole; return false; }
          });
          _sync();
        }

        function _sync() {
          $hidden.val(JSON.stringify(members));
        }

        // ---- Invite action ----------------------------------------------

        function _sendInvite(email, role) {
          var inv = Drupal.settings.dynamicFormInvitations || {};
          if (!inv.sendUrl || !inv.formId) { return; }

          $.ajax({
            url:      inv.sendUrl,
            type:     'POST',
            dataType: 'json',
            data:     { form_id: inv.formId, email: email, role: role },
            success: function (resp) {
              if (resp.status === 'error') { _toast('error', resp.message); return; }
              _toast('success', resp.message);
              if (typeof Drupal.dfbReloadInvitations === 'function') {
                Drupal.dfbReloadInvitations();
              }
            },
            error: function () {
              _toast('error', Drupal.t('Request failed. Please try again.'));
            }
          });
        }

        // ---- Utilities --------------------------------------------------

        function _avatarInitial(str) {
          if (!str) { return '?'; }
          return str.charAt(0).toUpperCase();
        }

        function _avatarColor(str) {
          if (!str) { return AVATAR_COLORS[0]; }
          var code = str.charCodeAt(0) || 0;
          return AVATAR_COLORS[code % AVATAR_COLORS.length];
        }

        function _toast(type, msg) {
          if (window.DFBToast && DFBToast[type]) { DFBToast[type](msg); }
        }

        function _isEmail(str) {
          return /^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(str);
        }

        function _esc(str) {
          return $('<div>').text(str || '').html();
        }
      });
    }
  };

})(jQuery, Drupal);
