const reduced = typeof window !== 'undefined' && window.matchMedia('(prefers-reduced-motion: reduce)').matches;

let observer;

function getObserver() {
  if (!observer) {
    observer = new IntersectionObserver(
      (entries) => {
        for (const entry of entries) {
          if (entry.isIntersecting) {
            entry.target.classList.add('is-visible');
            observer.unobserve(entry.target);
          }
        }
      },
      { rootMargin: '0px 0px -8% 0px', threshold: 0.12 }
    );
  }
  return observer;
}

export const reveal = {
  mounted(el, binding) {
    el.classList.add('reveal');
    el.style.setProperty('--reveal-delay', `${binding.value ?? 0}ms`);
    if (reduced) {
      el.classList.add('is-visible');
      return;
    }
    getObserver().observe(el);
  },
  unmounted(el) {
    observer?.unobserve(el);
  }
};
