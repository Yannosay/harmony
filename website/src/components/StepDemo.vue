<script setup>
import { onMounted, onUnmounted, ref, watchEffect } from 'vue';
import AppIcon from './AppIcon.vue';
import { drumGrid, drumStyles } from '../theory.js';

const rows = ['Kick', 'Snare', 'Hat', 'Open hat'];
const styles = Object.keys(drumStyles);
const style = ref('Trap');
const density = ref(0.5);
const seed = ref(7);
const step = ref(-1);
const sound = ref(false);
const root = ref(null);

const grid = ref([]);
watchEffect(() => (grid.value = drumGrid(style.value, density.value, seed.value)));

let audio = null;
let timer = 0;
let visible = false;
let observer;

function noiseBuffer(ctx) {
  const buffer = ctx.createBuffer(1, ctx.sampleRate * 0.5, ctx.sampleRate);
  const data = buffer.getChannelData(0);
  for (let i = 0; i < data.length; i++) data[i] = Math.random() * 2 - 1;
  return buffer;
}

let noise = null;

function hit(row, velocity) {
  if (!audio) return;
  const t = audio.currentTime + 0.01;
  const out = audio.createGain();
  out.connect(audio.destination);

  if (row === 0) {
    const osc = audio.createOscillator();
    osc.frequency.setValueAtTime(150, t);
    osc.frequency.exponentialRampToValueAtTime(42, t + 0.18);
    out.gain.setValueAtTime(0.9 * velocity, t);
    out.gain.exponentialRampToValueAtTime(0.001, t + 0.45);
    osc.connect(out);
    osc.start(t);
    osc.stop(t + 0.5);
    return;
  }

  const src = audio.createBufferSource();
  src.buffer = noise;
  const filter = audio.createBiquadFilter();
  filter.type = row === 1 ? 'bandpass' : 'highpass';
  filter.frequency.value = row === 1 ? 1800 : 7500;
  const length = row === 1 ? 0.18 : row === 2 ? 0.05 : 0.3;
  out.gain.setValueAtTime((row === 1 ? 0.5 : 0.22) * velocity, t);
  out.gain.exponentialRampToValueAtTime(0.001, t + length);
  src.connect(filter).connect(out);
  src.start(t);
  src.stop(t + length + 0.05);
}

function tick() {
  if (visible || sound.value) {
    step.value = (step.value + 1) % 16;
    if (sound.value) grid.value.forEach((r, i) => r[step.value] > 0 && hit(i, r[step.value]));
  }
  timer = setTimeout(tick, (60 / 140 / 4) * 1000);
}

function toggleSound() {
  sound.value = !sound.value;
  if (sound.value) {
    audio = new AudioContext();
    noise = noiseBuffer(audio);
  } else {
    audio?.close();
    audio = null;
  }
}

function toggleCell(r, s) {
  grid.value[r][s] = grid.value[r][s] > 0 ? 0 : 1;
}

onMounted(() => {
  observer = new IntersectionObserver(([e]) => (visible = e.isIntersecting));
  observer.observe(root.value);
  tick();
});

onUnmounted(() => {
  clearTimeout(timer);
  observer?.disconnect();
  audio?.close();
});
</script>

<template>
  <div ref="root" class="demo card">
    <div class="head">
      <div class="title"><span class="dot"></span>Drum Generator</div>
      <div class="buttons">
        <button class="mini" @click="seed = Math.floor(Math.random() * 1e6) + 1">New variation</button>
        <button class="mini" :class="{ on: sound }" @click="toggleSound" :aria-label="sound ? 'Mute' : 'Listen'">
          <AppIcon :name="sound ? 'pause' : 'play'" filled />{{ sound ? 'Stop' : 'Listen' }}
        </button>
      </div>
    </div>

    <div class="styles" role="radiogroup" aria-label="Style">
      <button v-for="s in styles" :key="s" class="style" :class="{ on: s === style }" role="radio" :aria-checked="s === style" @click="style = s">{{ s }}</button>
    </div>

    <label class="density">
      <span>Density</span>
      <input v-model.number="density" type="range" min="0" max="1" step="0.01" />
    </label>

    <div class="grid">
      <template v-for="(row, r) in grid" :key="r">
        <div class="row-name">{{ rows[r] }}</div>
        <div class="steps">
          <button v-for="(v, s) in row" :key="s" class="step" :class="{ on: v > 0, beat: Math.floor(s / 4) % 2 === 1, now: s === step }"
                  :style="{ '--v': v }" :aria-label="`${rows[r]} step ${s + 1}`" @click="toggleCell(r, s)"></button>
        </div>
      </template>
    </div>
  </div>
</template>

<style scoped>
.demo {
  padding: 22px;
  display: flex;
  flex-direction: column;
  gap: 18px;
  min-width: 0;
}

.head {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 12px;
  flex-wrap: wrap;
}

.title {
  display: flex;
  align-items: center;
  gap: 10px;
  font-weight: 600;
  font-size: 14px;
}

.dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  background: var(--lilac);
  box-shadow: 0 0 12px var(--lilac);
}

.buttons {
  display: flex;
  gap: 8px;
}

.mini {
  display: inline-flex;
  align-items: center;
  gap: 8px;
  height: 36px;
  padding: 0 14px;
  border-radius: 10px;
  border: 1px solid var(--line);
  background: var(--control);
  font-size: 11px;
  font-weight: 500;
  cursor: pointer;
}

.mini svg {
  width: 13px;
  height: 13px;
}

.mini.on {
  background: rgba(199, 178, 250, 0.16);
  border-color: rgba(199, 178, 250, 0.5);
  color: var(--lilac);
}

.styles {
  display: flex;
  flex-wrap: wrap;
  gap: 6px;
}

.style {
  height: 32px;
  padding: 0 12px;
  border-radius: 9px;
  border: 1px solid var(--line);
  background: transparent;
  font-size: 11px;
  color: var(--text-dim);
  cursor: pointer;
  transition: all 0.2s;
}

.style.on {
  background: rgba(199, 178, 250, 0.14);
  border-color: rgba(199, 178, 250, 0.45);
  color: var(--text);
}

.density {
  display: flex;
  align-items: center;
  gap: 14px;
  font-size: 10px;
  color: var(--text-faint);
  text-transform: uppercase;
  letter-spacing: 0.1em;
}

input[type='range'] {
  flex: 1;
  accent-color: var(--lilac);
}

.grid {
  display: grid;
  grid-template-columns: auto 1fr;
  gap: 8px 12px;
  align-items: center;
}

.row-name {
  font-size: 10px;
  color: var(--text-dim);
  white-space: nowrap;
}

.steps {
  display: grid;
  grid-template-columns: repeat(16, 1fr);
  gap: 3px;
}

.step {
  aspect-ratio: 1 / 1.25;
  border: 0;
  border-radius: 4px;
  background: #232329;
  cursor: pointer;
  padding: 0;
  transition: background 0.15s, transform 0.15s, box-shadow 0.15s;
}

.step.beat {
  background: #1b1b20;
}

.step.on {
  background: color-mix(in srgb, var(--lilac) calc(55% + var(--v) * 45%), #1b1b20);
}

.step.now {
  transform: translateY(-2px);
  box-shadow: 0 0 0 1px rgba(245, 245, 246, 0.35);
}

.step.on.now {
  box-shadow: 0 0 16px rgba(199, 178, 250, 0.8);
}
</style>
