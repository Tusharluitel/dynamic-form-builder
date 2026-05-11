(function ($) {
  'use strict';

  Drupal.behaviors.dfrResponseForm = {
    attach: function (context, settings) {
      var $container = $('.dfr-form-container', context).once('dfr-form-init');
      if (!$container.length) { return; }

      var formId        = parseInt($container.data('form-id'), 10);
      var token         = $container.data('token');
      var saveUrl       = $container.data('save-url');
      var submitUrl     = $container.data('submit-url');
      var uploadUrl     = $container.data('upload-url');
      var thankyouUrl   = $container.data('thankyou-url');
      var totalSteps    = parseInt($container.data('total-steps'), 10);
      var isAnon        = $container.data('is-anonymous') === 1;
      var guestEmailUrl = $container.data('guest-email-url');

      var responseId  = parseInt($('#dfr-response-id').val(), 10) || 0;
      var currentStep = 1;

      var $steps    = $container.find('.dfr-step');
      var $progress = $container.find('.dfr-progress-fill');
      var $progText = $container.find('.dfr-progress-text');

      // ----------------------------------------------------------------
      // Anonymous email capture popup
      // ----------------------------------------------------------------
      var emailCollected = false;
      var guestEmail     = '';
      var pendingAction  = null;
      var $overlay       = null;

      function buildOverlay() {
        var $el = $(
          '<div class="dfr-email-overlay dfr-overlay-hidden">' +
            '<div class="dfr-email-modal">' +
              '<h3>One quick thing before you start</h3>' +
              '<p>Please enter your email so we can identify your submission.</p>' +
              '<input type="email" class="dfr-email-input" placeholder="you@example.com" autocomplete="email" />' +
              '<div class="dfr-email-modal-error"></div>' +
              '<button type="button" class="dfr-email-submit">Continue</button>' +
              '<p class="dfr-email-note">Your email is only used to identify your response.</p>' +
            '</div>' +
          '</div>'
        );
        $el.on('click', '.dfr-email-submit', doSubmitEmail);
        $el.on('keydown', '.dfr-email-input', function (e) {
          if (e.which === 13) { doSubmitEmail(); }
        });
        $el.on('input', '.dfr-email-input', function () {
          $(this).removeClass('dfr-input-error');
          $el.find('.dfr-email-modal-error').hide();
        });
        return $el;
      }

      function showEmailPopup(cb) {
        pendingAction = cb || null;
        // Recreate overlay if user removed it from DOM via DevTools.
        if (!$overlay || !$.contains(document.documentElement, $overlay[0])) {
          $overlay = buildOverlay();
          $('body').append($overlay);
        }
        $overlay.removeClass('dfr-overlay-hidden');
        setTimeout(function () { $overlay.find('.dfr-email-input').focus(); }, 50);
      }

      function doSubmitEmail() {
        var email  = $overlay.find('.dfr-email-input').val().trim();
        var $err   = $overlay.find('.dfr-email-modal-error');
        var $input = $overlay.find('.dfr-email-input');
        var $btn   = $overlay.find('.dfr-email-submit');

        if (!email || !/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(email)) {
          $err.text('Please enter a valid email address.').show();
          $input.addClass('dfr-input-error');
          return;
        }

        $err.hide();
        $input.removeClass('dfr-input-error');
        $btn.prop('disabled', true).text('Saving…');

        $.ajax({
          url:      guestEmailUrl,
          type:     'POST',
          data:     { email: email, token: token },
          dataType: 'json',
          success: function (res) {
            $btn.prop('disabled', false).text('Continue');
            if (res && res.status === 'ok') {
              emailCollected = true;
              guestEmail     = email;
              $overlay.addClass('dfr-overlay-hidden');
              if (typeof pendingAction === 'function') {
                var cb = pendingAction;
                pendingAction = null;
                cb();
              }
            } else {
              $err.text((res && res.message) || 'Something went wrong. Please try again.').show();
            }
          },
          error: function () {
            $btn.prop('disabled', false).text('Continue');
            $err.text('Network error. Please try again.').show();
          }
        });
      }

      if (isAnon) {
        $overlay = buildOverlay();
        $('body').append($overlay);
        showEmailPopup(null);
      }

      // ----------------------------------------------------------------
      // Progress bar
      // ----------------------------------------------------------------
      function updateProgress() {
        if (totalSteps <= 1) { return; }
        var pct = Math.round((currentStep / totalSteps) * 100);
        $progress.css('width', pct + '%');
        $progText.text('Step ' + currentStep + ' of ' + totalSteps);
      }
      updateProgress();

      // ----------------------------------------------------------------
      // Rating stars
      // ----------------------------------------------------------------
      $container.on('mouseenter', '.dfp-star', function () {
        var $stars = $(this).closest('.dfp-stars').find('.dfp-star');
        var idx    = $stars.index(this);
        $stars.each(function (i) {
          $(this)[i <= idx ? 'addClass' : 'removeClass']('dfp-star-hover');
        });
      });

      $container.on('mouseleave', '.dfp-stars', function () {
        $(this).find('.dfp-star').removeClass('dfp-star-hover');
      });

      $container.on('click', '.dfp-star', function () {
        var $stars = $(this).closest('.dfp-stars').find('.dfp-star');
        var idx    = $stars.index(this);
        $stars.each(function (i) {
          $(this)[i <= idx ? 'addClass' : 'removeClass']('dfp-star-active');
        });
        $(this).closest('.dfp-question').find('.dfr-rating-value').val(idx + 1);
      });

      // ----------------------------------------------------------------
      // Linear scale
      // ----------------------------------------------------------------
      $container.on('click', '.dfp-scale-btn', function () {
        var $btns = $(this).closest('.dfp-scale-buttons').find('.dfp-scale-btn');
        $btns.removeClass('dfp-scale-active');
        $(this).addClass('dfp-scale-active');
        $(this).closest('.dfp-question').find('.dfr-scale-value').val($(this).text().trim());
      });

      // ----------------------------------------------------------------
      // File dropzone
      // ----------------------------------------------------------------
      $container.on('click', '.dfp-file-dropzone', function (e) {
        if (!$(e.target).is('input')) {
          $(this).find('.dfp-file-hidden').trigger('click');
        }
      });

      $container.on('change', '.dfp-file-hidden', function () {
        if (!this.files || !this.files.length) { return; }

        var $input    = $(this);
        var $dropzone = $input.closest('.dfp-file-dropzone');
        var $q        = $input.closest('.dfp-question');
        var $valInput = $q.find('.dfp-file-value');
        var $nameEl   = $dropzone.find('.dfp-file-name');

        $nameEl.text('Uploading…').show();
        $dropzone.addClass('dfp-file-uploading').removeClass('dfp-file-done dfp-file-error');
        $valInput.val('');

        var fd = new FormData();
        fd.append('file', this.files[0]);
        fd.append('form_id', formId);
        fd.append('question_id', $input.data('question-id'));
        fd.append('token', token);

        $.ajax({
          url:         uploadUrl,
          type:        'POST',
          data:        fd,
          contentType: false,
          processData: false,
          dataType:    'json',
          success: function (res) {
            $dropzone.removeClass('dfp-file-uploading');
            if (res && res.status === 'ok') {
              $valInput.val(JSON.stringify({ fid: res.fid, filename: res.filename, url: res.url }));
              $nameEl.text(res.filename);
              $dropzone.addClass('dfp-file-done');
              $q.find('.dfr-field-error').text('').hide();
              $q.removeClass('dfp-question-error');
            } else {
              $dropzone.addClass('dfp-file-error');
              $nameEl.text((res && res.message) || 'Upload failed.');
            }
          },
          error: function () {
            $dropzone.removeClass('dfp-file-uploading').addClass('dfp-file-error');
            $nameEl.text('Upload failed. Please try again.');
          }
        });
      });

      // ----------------------------------------------------------------
      // Answer collection — reads every question in a step into a plain object
      // ----------------------------------------------------------------
      function collectStepAnswers($step) {
        var answers = {};
        $step.find('.dfp-question').each(function () {
          var $q   = $(this);
          var qid  = $q.data('question-id');
          var type = $q.data('type');
          var val;

          switch (type) {
            case 'text':
            case 'email':
            case 'number':
            case 'date':
            case 'time':
              val = $q.find('.dfp-input').val() || '';
              break;
            case 'textarea':
              val = $q.find('.dfp-textarea').val() || '';
              break;
            case 'text_editor':
              var $tarea  = $q.find('.dfp-textarea');
              var tinyId  = $tarea.attr('id');
              if (typeof tinymce !== 'undefined' && tinymce.get(tinyId)) {
                val = tinymce.get(tinyId).getContent() || '';
              } else {
                val = $tarea.val() || '';
              }
              break;
            case 'radio':
              val = $q.find('input[type="radio"]:checked').val() || '';
              break;
            case 'checkbox':
              val = [];
              $q.find('input[type="checkbox"]:checked').each(function () {
                val.push($(this).val());
              });
              break;
            case 'select':
              var $sel    = $q.find('.dfp-select2');
              var s2inst  = $sel.data('select2');
              if (typeof $.fn.select2 !== 'undefined' && s2inst) {
                val = $sel.select2('val') || '';
              } else {
                val = $sel.val() || '';
              }
              break;
            case 'rating':
              val = $q.find('.dfr-rating-value').val() || '';
              break;
            case 'linear_scale':
              val = $q.find('.dfr-scale-value').val() || '';
              break;
            case 'tags':
              var $tagsEl  = $q.find('.dfp-tags-select2');
              var s2tags   = $tagsEl.data('select2');
              if (typeof $.fn.select2 !== 'undefined' && s2tags) {
                var rawTags = $tagsEl.select2('val');
                val = Array.isArray(rawTags) ? rawTags
                    : (rawTags ? String(rawTags).split(',') : []);
              } else {
                var rawStr = $tagsEl.val() || '';
                val = rawStr ? rawStr.split(',') : [];
              }
              break;
            case 'file':
              val = $q.find('.dfp-file-value').val() || '';
              break;
            default:
              val = $q.find('input, textarea, select').first().val() || '';
          }

          answers[qid] = val;
        });
        return answers;
      }

      // ----------------------------------------------------------------
      // Client-side required-field validation
      // ----------------------------------------------------------------
      function validateStep($step) {
        var valid   = true;
        var answers = collectStepAnswers($step);

        $step.find('.dfp-question[data-required="1"]').each(function () {
          var $q   = $(this);
          var qid  = $q.data('question-id');
          var val  = answers[qid];
          var empty = (val === '' || val === null || val === undefined)
            || (Array.isArray(val) && val.length === 0);

          var $err = $q.find('.dfr-field-error');
          if (empty) {
            $err.text('This field is required.').show();
            $q.addClass('dfp-question-error');
            if (valid) {
              $('html, body').animate({ scrollTop: $q.offset().top - 20 }, 200);
            }
            valid = false;
          } else {
            $err.text('').hide();
            $q.removeClass('dfp-question-error');
          }
        });
        return valid;
      }

      // Clear error styling as soon as the user starts answering.
      $container.on(
        'input change',
        '.dfp-input, .dfp-textarea, .dfp-select2, input[type="radio"], input[type="checkbox"]',
        function () {
          var $q = $(this).closest('.dfp-question');
          $q.find('.dfr-field-error').text('').hide();
          $q.removeClass('dfp-question-error');
        }
      );

      // ----------------------------------------------------------------
      // AJAX save
      // ----------------------------------------------------------------
      function saveStep($step, cb) {
        var payload = {
          form_id:     formId,
          response_id: responseId,
          answers:     collectStepAnswers($step),
          token:       token
        };
        if (isAnon && guestEmail) {
          payload.guest_email = guestEmail;
        }
        $.ajax({
          url:      saveUrl,
          type:     'POST',
          data:     payload,
          dataType: 'json',
          success: function (data) {
            if (data.status === 'ok') {
              responseId = data.response_id;
              $('#dfr-response-id').val(responseId);
            }
            cb(data);
          },
          error: function () {
            cb({ status: 'error', message: 'Network error. Please check your connection.' });
          }
        });
      }

      // ----------------------------------------------------------------
      // Step navigation
      // ----------------------------------------------------------------
      function goToStep(num) {
        $steps.removeClass('dfr-step-active');
        $steps.filter('[data-step="' + num + '"]').addClass('dfr-step-active');
        currentStep = num;
        updateProgress();
        $('html, body').animate({ scrollTop: $container.offset().top - 20 }, 200);
      }

      $container.on('click', '.dfr-btn-next', function () {
        var $current = $steps.filter('.dfr-step-active');
        if (!validateStep($current)) { return; }

        if (isAnon && !emailCollected) {
          var $self = $(this);
          showEmailPopup(function () { $self.trigger('click'); });
          return;
        }

        var nextStep = parseInt($(this).data('step'), 10);
        var $btn     = $(this).prop('disabled', true).text('Saving…');

        saveStep($current, function (data) {
          $btn.prop('disabled', false).text('Next →');
          if (data.status === 'ok') {
            goToStep(nextStep);
          } else {
            alert(data.message || 'Could not save. Please try again.');
          }
        });
      });

      $container.on('click', '.dfr-btn-prev', function () {
        goToStep(parseInt($(this).data('step'), 10));
      });

      // ----------------------------------------------------------------
      // Final submit: save last step, then submit
      // ----------------------------------------------------------------
      $container.on('click', '.dfr-btn-submit', function () {
        var $current = $steps.filter('.dfr-step-active');
        if (!validateStep($current)) { return; }

        if (isAnon && !emailCollected) {
          var $self = $(this);
          showEmailPopup(function () { $self.trigger('click'); });
          return;
        }

        var $btn = $(this).prop('disabled', true).text('Submitting…');

        saveStep($current, function (saveData) {
          if (saveData.status !== 'ok') {
            $btn.prop('disabled', false).text('Submit');
            alert(saveData.message || 'Could not save. Please try again.');
            return;
          }

          $.ajax({
            url:      submitUrl,
            type:     'POST',
            data:     { form_id: formId, response_id: responseId, token: token },
            dataType: 'json',
            success: function (data) {
              if (data.status === 'ok') {
                window.location.href = thankyouUrl;
                return;
              }

              $btn.prop('disabled', false).text('Submit');

              if (data.errors) {
                var firstStep = null;
                $.each(data.errors, function (qid, msg) {
                  var $q   = $container.find('.dfp-question[data-question-id="' + qid + '"]');
                  var sNum = parseInt($q.closest('.dfr-step').data('step'), 10);
                  $q.find('.dfr-field-error').text(msg).show();
                  $q.addClass('dfp-question-error');
                  if (firstStep === null || sNum < firstStep) { firstStep = sNum; }
                });
                if (firstStep !== null) { goToStep(firstStep); }
              } else {
                alert(data.message || 'Submission failed. Please try again.');
              }
            },
            error: function () {
              $btn.prop('disabled', false).text('Submit');
              alert('Network error. Please check your connection and try again.');
            }
          });
        });
      });
    }
  };

})(jQuery);
