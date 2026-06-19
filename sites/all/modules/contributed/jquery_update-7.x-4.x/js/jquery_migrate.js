/**
 * Apply settings for jQuery Migrate from the jQuery Update module.
 */
var Drupal = Drupal || { "settings": {}, "behaviors": {}, "locale": {} };

(function (jQuery, Drupal) {
  Drupal.behaviors.jqueryUpdate = {
    attach: function () {
      if (Drupal.settings.jqueryUpdate !== undefined) {
        jQuery.migrateMute = Drupal.settings.jqueryUpdate.migrateMute;
        jQuery.migrateTrace = Drupal.settings.jqueryUpdate.migrateTrace;
      }
    }
  };
})(jQuery, Drupal);
