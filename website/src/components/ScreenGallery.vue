<script setup>
import { ref } from 'vue';

const shots = [
  { id: 'playlist', label: 'Playlist', file: 'harmony-playlist', text: 'Arrange patterns, audio and automation clips. Every pattern sits in the pattern bar, ready to drag into the song.' },
  { id: 'pianoroll', label: 'Piano Roll', file: 'harmony-pianoroll', text: 'Pastel notes with velocity built in, scale highlighting, ghost notes from other channels and a velocity lane.' },
  { id: 'chords', label: 'Chord Generator', file: 'harmony-chords', text: 'Pick a key and a feel. Harmony writes voice-led chords, bass and rhythm straight into your pattern.' },
  { id: 'mixer', label: 'Mixer', file: 'harmony-mixer', text: 'Every channel gets its own insert with ten effect slots. Built-in EQ, compressor and limiter on the master, latency compensated for you.' },
  { id: 'settings', label: 'Settings', file: 'harmony-settings', text: 'Interface size, note previews, snapping, playhead behaviour, project defaults, audio devices and plugin folders.' },
  { id: 'palette', label: 'Command Palette', file: 'harmony-palette', text: 'Ctrl+K opens every command in Harmony. Type a few letters and press Enter.' }
];

const current = ref(0);
</script>

<template>
  <section id="tour" class="tour">
    <div class="container">
      <span class="eyebrow" v-reveal>Take the tour</span>
      <h2 class="section-title" v-reveal="80">Real screenshots. No mockups.</h2>
      <p class="section-lead" v-reveal="160">Every image below was captured from Harmony itself.</p>

      <div class="tabs" role="tablist" v-reveal="200">
        <button v-for="(s, i) in shots" :key="s.id" role="tab" :aria-selected="current === i" class="tab" :class="{ on: current === i }" @click="current = i">
          {{ s.label }}
        </button>
      </div>

      <div class="viewer" v-reveal="260">
        <div class="frame">
          <img v-for="(s, i) in shots" :key="s.id" :src="`/screenshots/${s.file}.webp`" :alt="`Harmony ${s.label}`"
               width="1720" height="1040" loading="lazy" :class="{ on: current === i }" />
        </div>
        <Transition name="caption" mode="out-in">
          <p :key="current" class="caption">{{ shots[current].text }}</p>
        </Transition>
      </div>
    </div>
  </section>
</template>

<style scoped>
.tour {
  padding: 120px 0 40px;
}

.tabs {
  display: flex;
  gap: 6px;
  margin-top: 40px;
  overflow-x: auto;
  scrollbar-width: none;
  padding-bottom: 4px;
}

.tab {
  flex: 0 0 auto;
  height: 40px;
  padding: 0 16px;
  border-radius: 11px;
  border: 1px solid var(--line);
  background: transparent;
  color: var(--text-dim);
  font-size: 12px;
  font-weight: 500;
  cursor: pointer;
  transition: all 0.25s;
}

.tab:hover {
  color: var(--text);
}

.tab.on {
  background: var(--pink);
  border-color: var(--pink);
  color: #141417;
}

.viewer {
  margin-top: 18px;
}

.frame {
  position: relative;
  aspect-ratio: 1720 / 1040;
  border-radius: 16px;
  overflow: hidden;
  border: 1px solid #2c2c33;
  background: var(--panel);
  box-shadow: 0 50px 100px -40px rgba(0, 0, 0, 0.9);
}

.frame img {
  position: absolute;
  inset: 0;
  width: 100%;
  height: 100%;
  opacity: 0;
  transform: scale(1.015);
  transition: opacity 0.6s ease, transform 0.9s cubic-bezier(0.2, 0.8, 0.2, 1);
}

.frame img.on {
  opacity: 1;
  transform: none;
}

.caption {
  margin: 18px 0 0;
  color: var(--text-dim);
  font-size: 13px;
  font-weight: 300;
  max-width: 760px;
}

.caption-enter-active,
.caption-leave-active {
  transition: opacity 0.25s, transform 0.25s;
}

.caption-enter-from {
  opacity: 0;
  transform: translateY(6px);
}

.caption-leave-to {
  opacity: 0;
}
</style>
