(function ($) {
  Drupal.behaviors.dynamicFormTheme = {
    attach: function (context) {
      // Mobile nav toggle
      var toggle = document.querySelector('.dfb-nav-toggle');
      var links = document.querySelector('.dfb-nav-links');
      if (toggle && links) {
        toggle.addEventListener('click', function () {
          links.classList.toggle('open');
        });
      }

      // Scroll-triggered animations
      var animElements = document.querySelectorAll('.dfb-animate');
      if (animElements.length && 'IntersectionObserver' in window) {
        var observer = new IntersectionObserver(function (entries) {
          entries.forEach(function (entry) {
            if (entry.isIntersecting) {
              entry.target.classList.add('visible');
              observer.unobserve(entry.target);
            }
          });
        }, { threshold: 0.15 });
        animElements.forEach(function (el) { observer.observe(el); });
      }

      // Nav background opacity on scroll
      var nav = document.querySelector('.dfb-nav');
      if (nav) {
        window.addEventListener('scroll', function () {
          if (window.scrollY > 20) {
            nav.style.background = 'rgba(15,23,42,.95)';
          } else {
            nav.style.background = 'rgba(15,23,42,.85)';
          }
        });
      }
    }
  };
})(jQuery);
