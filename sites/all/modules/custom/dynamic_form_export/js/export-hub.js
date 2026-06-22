(function ($) {
  Drupal.behaviors.dfbExportHub = {
    attach: function (context, settings) {
      var $formPicker   = $('.dfb-export-form-picker', context);
      var $formSelect   = $('.dfb-export-form-select', context);
      var select2Inited = false;

      function maybeInitSelect2() {
        if (!select2Inited && $formSelect.length) {
          $formSelect.select2({
            placeholder: Drupal.t('— Select one or more forms —'),
            allowClear: true,
            width: '100%'
          });
          select2Inited = true;
        }
      }

      // Initialize immediately if the picker is already visible on page load
      // (e.g. the form reloaded after a validation error with responses selected).
      if ($formPicker.is(':visible')) {
        maybeInitSelect2();
      }

      // Drupal states fires state:visible on the container when #states makes
      // it visible. The new boolean value is on e.value (not a second argument —
      // Drupal uses .trigger({ type, value }) not .trigger(type, [value])).
      // Defer with setTimeout so the document-level states handler has applied
      // .toggle() before Select2 tries to measure the element's dimensions.
      $formPicker.on('state:visible', function (e) {
        if (e.value) {
          setTimeout(maybeInitSelect2, 0);
        }
      });
    }
  };
}(jQuery));
