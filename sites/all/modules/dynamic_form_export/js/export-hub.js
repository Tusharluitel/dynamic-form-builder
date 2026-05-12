(function ($) {
  Drupal.behaviors.dfbExportHub = {
    attach: function (context, settings) {
      var $typeSelect   = $('#dfb-export-type', context);
      var $formPicker   = $('#dfb-export-form-picker', context);
      var $formSelect   = $('#dfb-export-form-id', context);
      var select2Inited = false;

      function toggle(val) {
        if (val === 'responses') {
          $formPicker.slideDown(150);
          if (!select2Inited) {
            $formSelect.select2({
              placeholder: Drupal.t('— Select a form —'),
              allowClear: true,
              width: '100%'
            });
            select2Inited = true;
          }
        } else {
          $formPicker.slideUp(150);
        }
      }

      // Set initial state without animation.
      $formPicker.hide();
      toggle($typeSelect.val());

      $typeSelect.once('dfb-export-type').on('change', function () {
        toggle($(this).val());
      });
    }
  };
}(jQuery));
