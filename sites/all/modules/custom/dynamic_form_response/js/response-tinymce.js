(function ($) {
  Drupal.behaviors.dfrTinymceInit = {
    attach: function (context, settings) {
      $('.dfr-tinymce-area', context).once('dfr-tinymce', function () {
        if (typeof tinymce === 'undefined') { return; }
        tinymce.init({
          selector: '#' + this.id,
          height: 220,
          menubar: false,
          statusbar: false,
          plugins: 'lists link',
          toolbar: 'bold italic underline | bullist numlist | link'
        });
      });
    }
  };
})(jQuery);
