<script setup>
import { ref } from 'vue';
import AppIcon from './AppIcon.vue';
import { displayVersion, downloadUrl, formatSize, release, repoPublic, repoUrl } from '../release.js';

const copied = ref(false);

async function copyHash() {
  try {
    await navigator.clipboard.writeText(release.sha256);
    copied.value = true;
    setTimeout(() => (copied.value = false), 1600);
  } catch {
    copied.value = false;
  }
}
</script>

<template>
  <section id="download" class="download">
    <div class="container">
      <div class="panel card" v-reveal>
        <div class="aura" aria-hidden="true"></div>
        <img class="icon" src="/brand/harmony-icon-512.png" alt="" width="120" height="120" loading="lazy" />
        <h2 class="section-title">Get Harmony {{ displayVersion(release.version) }}</h2>
        <p class="section-lead">
          For Windows 10 and 11, 64-bit. Installs for your user, no admin rights needed.
        </p>
        <div class="cta">
          <a :href="downloadUrl()" class="btn btn-primary" download>
            <AppIcon name="windows" filled />
            Download for Windows
          </a>
          <a v-if="repoPublic" :href="repoUrl" class="btn" target="_blank" rel="noopener">
            <AppIcon name="github" filled />
            Source code
          </a>
        </div>
        <dl class="facts">
          <div><dt>Version</dt><dd>{{ displayVersion(release.version) }}</dd></div>
          <div v-if="release.size"><dt>Size</dt><dd>{{ formatSize(release.size) }}</dd></div>
          <div><dt>Formats</dt><dd>VST3 · VST · LV2 · SF2</dd></div>
          <div v-if="release.sha256" class="hash">
            <dt>SHA-256</dt>
            <dd><button class="hash-btn" @click="copyHash" :title="release.sha256">{{ copied ? 'Copied' : `${release.sha256.slice(0, 16)}...` }}</button></dd>
          </div>
        </dl>
        <p class="note">Windows may show a SmartScreen notice the first time because the installer is new. Choose “More info”, then “Run anyway”.</p>
      </div>
    </div>
  </section>
</template>

<style scoped>
.download {
  padding: 120px 0 100px;
}

.panel {
  position: relative;
  overflow: hidden;
  display: flex;
  flex-direction: column;
  align-items: center;
  text-align: center;
  padding: 64px 22px 44px;
}

.aura {
  position: absolute;
  inset: -40% -20% auto;
  height: 520px;
  background: radial-gradient(closest-side, rgba(247, 178, 202, 0.18), transparent 70%);
  pointer-events: none;
}

.icon {
  position: relative;
  width: 120px;
  height: 120px;
  margin-bottom: 26px;
  filter: drop-shadow(0 24px 50px rgba(247, 178, 202, 0.35));
}

.section-lead {
  margin: 0 auto;
}

.cta {
  display: flex;
  flex-wrap: wrap;
  justify-content: center;
  gap: 12px;
  margin-top: 34px;
}

.facts {
  display: flex;
  flex-wrap: wrap;
  justify-content: center;
  gap: 12px 34px;
  margin: 40px 0 0;
}

.facts div {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

dt {
  font-size: 10px;
  color: var(--text-faint);
  text-transform: uppercase;
  letter-spacing: 0.12em;
}

dd {
  margin: 0;
  font-size: 13px;
  font-weight: 500;
}

.hash-btn {
  border: 0;
  background: none;
  padding: 0;
  font: 500 13px var(--font);
  color: var(--pink);
  cursor: copy;
}

.note {
  margin: 34px 0 0;
  font-size: 11px;
  color: var(--text-faint);
  max-width: 560px;
}
</style>
