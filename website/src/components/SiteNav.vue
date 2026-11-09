<script setup>
import { onMounted, onUnmounted, ref } from 'vue';
import AppIcon from './AppIcon.vue';
import { repoPublic, repoUrl } from '../release.js';

const scrolled = ref(false);
const onScroll = () => (scrolled.value = window.scrollY > 12);
onMounted(() => window.addEventListener('scroll', onScroll, { passive: true }));
onUnmounted(() => window.removeEventListener('scroll', onScroll));
</script>

<template>
  <header class="nav" :class="{ scrolled }">
    <div class="container inner">
      <a href="#top" class="lockup" aria-label="Yannosay Harmony home">
        <img src="/brand/yannosay-logo.png" alt="" width="28" height="28" />
        <span>Harmony</span>
      </a>
      <nav class="links">
        <a href="#features">Features</a>
        <a href="#create">Create</a>
        <a href="#tour">Tour</a>
        <a href="#download">Download</a>
      </nav>
      <div class="actions">
        <a v-if="repoPublic" :href="repoUrl" class="icon-link" aria-label="Source code on GitHub" target="_blank" rel="noopener"><AppIcon name="github" filled /></a>
        <a href="#download" class="btn btn-primary small">Download</a>
      </div>
    </div>
  </header>
</template>

<style scoped>
.nav {
  position: fixed;
  inset: 0 0 auto 0;
  z-index: 50;
  transition: background 0.3s, border-color 0.3s, backdrop-filter 0.3s;
  border-bottom: 1px solid transparent;
}

.nav.scrolled {
  background: rgba(10, 10, 11, 0.72);
  backdrop-filter: blur(18px) saturate(140%);
  border-bottom-color: var(--line-soft);
}

.inner {
  display: flex;
  align-items: center;
  justify-content: space-between;
  height: 72px;
}

.lockup {
  display: flex;
  align-items: center;
  gap: 10px;
  font-weight: 600;
  font-size: 17px;
  letter-spacing: -0.01em;
}

.lockup img {
  width: 28px;
  height: 28px;
}

.links {
  display: none;
  gap: 32px;
  font-size: 13px;
  color: var(--text-dim);
}

.links a {
  transition: color 0.2s;
}

.links a:hover {
  color: var(--text);
}

.actions {
  display: flex;
  align-items: center;
  gap: 10px;
}

.icon-link {
  display: grid;
  place-items: center;
  width: 40px;
  height: 40px;
  border-radius: 12px;
  color: var(--text-dim);
  transition: color 0.2s, background 0.2s;
}

.icon-link:hover {
  color: var(--text);
  background: var(--control);
}

.icon-link svg {
  width: 20px;
  height: 20px;
}

.small {
  height: 40px;
  padding: 0 18px;
  font-size: 12px;
  border-radius: 12px;
}

@media (min-width: 860px) {
  .links {
    display: flex;
  }
}
</style>
