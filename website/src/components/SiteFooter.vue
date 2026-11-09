<script setup>
import { repoPublic, repoUrl } from '../release.js';

const year = new Date().getFullYear();
</script>

<template>
  <footer class="footer">
    <div class="container inner">
      <div class="lockup">
        <img src="/brand/yannosay-logo.png" alt="" width="24" height="24" />
        <span>Harmony</span>
      </div>
      <p>© {{ year }} Yannosay Productions. VST is a trademark of Steinberg Media Technologies GmbH.</p>
      <nav>
        <a href="#features">Features</a>
        <a href="#download">Download</a>
        <a v-if="repoPublic" :href="repoUrl" target="_blank" rel="noopener">GitHub</a>
      </nav>
    </div>
  </footer>
</template>

<style scoped>
.footer {
  border-top: 1px solid var(--line-soft);
  padding: 36px 0 44px;
}

.inner {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  justify-content: space-between;
  gap: 18px;
}

.lockup {
  display: flex;
  align-items: center;
  gap: 9px;
  font-weight: 600;
  font-size: 14px;
}

p {
  margin: 0;
  font-size: 11px;
  color: var(--text-faint);
  flex: 1 1 320px;
}

nav {
  display: flex;
  gap: 22px;
  font-size: 12px;
  color: var(--text-dim);
}

nav a:hover {
  color: var(--text);
}
</style>
