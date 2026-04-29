/**
 * @file
 * Dynamic Form — member picker behaviour.
 *
 * Provides an AJAX user-search input that lets the form owner add members
 * with a role (Editor / Form Applicant). Selected members are shown as
 * chip tags and serialised into a hidden field submitted with the form.
 */
(function ($, Drupal) {

  Drupal.behaviors.dynamicFormMembers = {
    attach: function (context, settings) {
      // Guard: only run when the member picker is on the page.
      var $search = $('#dfb-members-search', context);
      if (!$search.length) { return; }

      // .once() prevents double-binding when attach() is called again (e.g.
      // after any AJAX rebuild of surrounding form elements).
      $search.once('dfb-members-init', function () {
        var s         = settings.dynamicFormMembers || {};
        var searchUrl = s.searchUrl || '';

        // Hidden field that carries the member list to the server.
        var $hidden = $('input[name="members_selected"]');

        // In-memory list of selected members: [{uid, name, role}, …]
        var members = [];
        try { members = JSON.parse($hidden.val() || '[]'); }
        catch (e) { members = []; }

        // Render any pre-loaded members (edit-form case).
        _renderChips();

        var searchTimer = null;

        // ---- Search input ------------------------------------------------
        // jQuery 1.4.4 (Drupal 7 default) does not have .on() — use .bind().

        $search.bind('keyup', function () {
          var q = $.trim($(this).val());
          clearTimeout(searchTimer);

          if (q.length < 2) {
            $('#dfb-members-results').hide().empty();
            return;
          }

          searchTimer = setTimeout(function () {
            $.ajax({
              url:      searchUrl,
              type:     'GET',
              dataType: 'json',
              data:     { s: q },
              success:  _renderResults,
              error:    function () {
                $('#dfb-members-results').hide().empty();
              }
            });
          }, 300);
        });

        // Close the dropdown when clicking anywhere outside the picker.
        $(document).bind('click.dfbMembers', function (e) {
          if (!$(e.target).closest('#dfb-members-search, #dfb-members-results').length) {
            $('#dfb-members-results').hide();
          }
        });

        // ---- Rendering ---------------------------------------------------

        function _renderResults(users) {
          var $r = $('#dfb-members-results').empty();

          if (!users || !users.length) {
            $r.append(
              '<div class="dfb-member-no-results">' + Drupal.t('No users found.') + '</div>'
            );
          }
          else {
            $.each(users, function (i, u) {
              // Mark users already in the list as non-clickable.
              var already = false;
              $.each(members, function (j, m) {
                if (m.uid === u.uid) { already = true; return false; }
              });

              var label =
                '<span class="dfb-member-result-name">' + _esc(u.name) + '</span>' +
                ' <span class="dfb-member-result-mail">(' + _esc(u.mail) + ')</span>';

              var $item = $(
                '<div class="dfb-member-result-item' +
                (already ? ' dfb-member-already-added' : '') +
                '"></div>'
              ).html(label);

              if (!already) {
                $item.bind('click', function () {
                  _addMember(u.uid, u.name, 'editor');
                  $r.hide().empty();
                  $search.val('').focus();
                });
              }

              $r.append($item);
            });
          }

          $r.show();
        }

        function _renderChips() {
          var $c = $('#dfb-members-chips').empty();

          if (!members.length) {
            $c.append(
              '<span class="dfb-members-empty">' +
              Drupal.t('No members added yet.') +
              '</span>'
            );
            return;
          }

          $.each(members, function (i, m) {
            var roleOpts =
              '<option value="editor"'         + (m.role === 'editor'         ? ' selected' : '') + '>' + Drupal.t('Editor')         + '</option>' +
              '<option value="form_applicant"' + (m.role === 'form_applicant' ? ' selected' : '') + '>' + Drupal.t('Form Applicant') + '</option>';

            var $chip = $(
              '<div class="dfb-member-chip" data-uid="' + m.uid + '">' +
                '<span class="dfb-member-chip-name">' + _esc(m.name) + '</span>' +
                '<select class="dfb-member-chip-role">' + roleOpts + '</select>' +
                '<button type="button" class="dfb-member-chip-remove" title="' + Drupal.t('Remove') + '">' +
                  '&times;' +
                '</button>' +
              '</div>'
            );

            $chip.find('.dfb-member-chip-role').bind('change', function () {
              _changeRole(m.uid, $(this).val());
            });

            $chip.find('.dfb-member-chip-remove').bind('click', function () {
              _removeMember(m.uid);
            });

            $c.append($chip);
          });
        }

        // ---- Member list mutations ----------------------------------------

        function _addMember(uid, name, role) {
          members.push({ uid: uid, name: name, role: role });
          _renderChips();
          _sync();
        }

        function _removeMember(uid) {
          members = $.grep(members, function (m) { return m.uid !== uid; });
          _renderChips();
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

        // ---- Utility -------------------------------------------------------

        function _esc(str) {
          return $('<div>').text(str).html();
        }
      });
    }
  };

})(jQuery, Drupal);
