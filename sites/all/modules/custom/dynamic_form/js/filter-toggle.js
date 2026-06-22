(function ($) {
  'use strict';

  Drupal.behaviors.dfbFilterToggle = {
    attach: function (context, settings) {
      var baseUrl = (settings.dfbFilterToggle && settings.dfbFilterToggle.toggleUrl)
        ? settings.dfbFilterToggle.toggleUrl
        : '';

      $('.dfb-filter-toggle-input', context).once('dfb-filter-toggle').on('change', function () {
        var $toggle     = $(this);
        var questionId  = $toggle.data('question-id');
        var newVal      = $toggle.is(':checked') ? 1 : 0;

        $toggle.prop('disabled', true);

        $.ajax({
          url:      baseUrl + '/' + questionId + '/toggle-filter',
          type:     'POST',
          dataType: 'json',
          data:     { allow_filter: newVal },
          success: function (data) {
            $toggle.prop('disabled', false);
            if (data.status === 'ok') {
              if (typeof DFBToast !== 'undefined') {
                DFBToast.success(data.message);
              }
            } else {
              // Revert on server-side rejection.
              $toggle.prop('checked', !newVal);
              if (typeof DFBToast !== 'undefined') {
                DFBToast.error(data.message || Drupal.t('Could not update filter setting.'));
              }
            }
          },
          error: function () {
            $toggle.prop('disabled', false);
            $toggle.prop('checked', !newVal);
            if (typeof DFBToast !== 'undefined') {
              DFBToast.error(Drupal.t('Request failed. Please try again.'));
            }
          }
        });
      });
    }
  };
}(jQuery));
