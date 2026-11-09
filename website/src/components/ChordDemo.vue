<script setup>
import { computed, onUnmounted, ref } from 'vue';
import AppIcon from './AppIcon.vue';
import { generate, pitchNames, progressions } from '../theory.js';

const root = ref(9);
const scale = ref('Minor');
const progression = ref(2);
const size = ref(3);
const playing = ref(false);
const active = ref(-1);

const chords = computed(() => generate({ root: root.value, scale: scale.value, progression: progression.value, size: size.value }));

const range = computed(() => {
  const keys = chords.value.flatMap((c) => [...c.notes, c.bass]);
  return { low: Math.min(...keys) - 2, high: Math.max(...keys) + 2 };
});

const notes = computed(() => {
  const { low, high } = range.value;
  const span = high - low + 1;
  return chords.value.flatMap((chord) =>
    [...chord.notes, chord.bass].map((key, voice) => ({
      id: `${chord.index}-${voice}`,
      chord: chord.index,
      left: `${chord.index * 25}%`,
      top: `${((high - key) / span) * 100}%`,
      height: `${100 / span}%`,
      bass: voice === chord.notes.length
    }))
  );
});

let audio = null;
let timer = 0;

function voice(ctx, freq, start, length, gain) {
  const osc = ctx.createOscillator();
  const osc2 = ctx.createOscillator();
  const amp = ctx.createGain();
  const filter = ctx.createBiquadFilter();
  osc.type = 'sawtooth';
  osc2.type = 'triangle';
  osc.frequency.value = freq;
  osc2.frequency.value = freq * 1.003;
  filter.type = 'lowpass';
  filter.frequency.setValueAtTime(900, start);
  filter.frequency.linearRampToValueAtTime(2400, start + 0.4);
  amp.gain.setValueAtTime(0, start);
  amp.gain.linearRampToValueAtTime(gain, start + 0.04);
  amp.gain.setTargetAtTime(gain * 0.6, start + 0.1, 0.4);
  amp.gain.setTargetAtTime(0, start + length - 0.05, 0.12);
  osc.connect(filter);
  osc2.connect(filter);
  filter.connect(amp).connect(ctx.destination);
  osc.start(start);
  osc2.start(start);
  osc.stop(start + length + 0.6);
  osc2.stop(start + length + 0.6);
}

function stop() {
  playing.value = false;
  active.value = -1;
  clearTimeout(timer);
  audio?.close();
  audio = null;
}

function play() {
  if (playing.value) {
    stop();
    return;
  }
  audio = new AudioContext();
  playing.value = true;
  const barSeconds = 1.6;
  let bar = 0;
  const tick = () => {
    if (!playing.value || !audio) return;
    const index = bar % chords.value.length;
    const chord = chords.value[index];
    const now = audio.currentTime + 0.03;
    active.value = index;
    for (const key of chord.notes) voice(audio, 440 * 2 ** ((key - 69) / 12), now, barSeconds, 0.055);
    voice(audio, 440 * 2 ** ((chord.bass - 69) / 12), now, barSeconds, 0.07);
    bar++;
    timer = setTimeout(tick, barSeconds * 1000);
  };
  tick();
}

function surprise() {
  root.value = Math.floor(Math.random() * 12);
  progression.value = Math.floor(Math.random() * progressions.length);
  scale.value = Math.random() < 0.5 ? 'Minor' : 'Major';
}

onUnmounted(stop);
</script>

<template>
  <div class="demo card">
    <div class="head">
      <div class="title">
        <span class="dot"></span>
        Chord Generator
      </div>
      <div class="buttons">
        <button class="mini" @click="surprise">Surprise me</button>
        <button class="mini play" :class="{ on: playing }" @click="play" :aria-label="playing ? 'Stop' : 'Listen'">
          <AppIcon :name="playing ? 'pause' : 'play'" filled />
          {{ playing ? 'Stop' : 'Listen' }}
        </button>
      </div>
    </div>

    <div class="controls">
      <label>
        <span>Key</span>
        <select v-model.number="root">
          <option v-for="(n, i) in pitchNames" :key="n" :value="i">{{ n }}</option>
        </select>
      </label>
      <label>
        <span>Scale</span>
        <select v-model="scale">
          <option>Minor</option>
          <option>Major</option>
          <option>Dorian</option>
          <option>Harmonic Minor</option>
        </select>
      </label>
      <label>
        <span>Progression</span>
        <select v-model.number="progression">
          <option v-for="(p, i) in progressions" :key="p.name" :value="i">{{ p.name }}</option>
        </select>
      </label>
      <label>
        <span>Chords</span>
        <select v-model.number="size">
          <option :value="3">Triads</option>
          <option :value="4">Sevenths</option>
        </select>
      </label>
    </div>

    <div class="chips">
      <TransitionGroup name="chip">
        <div v-for="c in chords" :key="`${c.index}-${c.name}`" class="chip" :class="{ active: active === c.index }">{{ c.name }}</div>
      </TransitionGroup>
    </div>

    <div class="roll" role="img" :aria-label="`Piano roll showing ${chords.map((c) => c.name).join(', ')}`">
      <div v-for="i in 4" :key="i" class="bar-line" :style="{ left: `${(i - 1) * 25}%` }"></div>
      <div v-for="n in notes" :key="n.id" class="note" :class="{ bass: n.bass, lit: active === n.chord }"
           :style="{ left: n.left, top: n.top, height: n.height }"></div>
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
  background: var(--pink);
  box-shadow: 0 0 12px var(--pink);
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
  transition: background 0.2s, border-color 0.2s, color 0.2s;
}

.mini:hover {
  background: #26262c;
}

.mini svg {
  width: 13px;
  height: 13px;
}

.play.on {
  background: rgba(247, 178, 202, 0.16);
  border-color: rgba(247, 178, 202, 0.5);
  color: var(--pink);
}

.controls {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 10px;
}

@media (min-width: 560px) {
  .controls { grid-template-columns: repeat(4, minmax(0, 1fr)); }
}

label {
  display: flex;
  flex-direction: column;
  gap: 6px;
  font-size: 10px;
  color: var(--text-faint);
  text-transform: uppercase;
  letter-spacing: 0.1em;
  min-width: 0;
}

select {
  appearance: none;
  height: 38px;
  padding: 0 12px;
  border-radius: 10px;
  border: 1px solid var(--line);
  background: var(--control) url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' stroke='%23a1a1aa' stroke-width='2' stroke-linecap='round'%3E%3Cpath d='M7 10l5 5 5-5'/%3E%3C/svg%3E") no-repeat right 10px center / 14px;
  color: var(--text);
  font: 500 12px var(--font);
  text-transform: none;
  letter-spacing: 0;
  cursor: pointer;
  min-width: 0;
}

select:focus-visible {
  outline: 2px solid rgba(247, 178, 202, 0.6);
  outline-offset: 1px;
}

.chips {
  display: grid;
  grid-template-columns: repeat(4, 1fr);
  gap: 8px;
}

.chip {
  height: 46px;
  display: grid;
  place-items: center;
  border-radius: 10px;
  background: rgba(247, 178, 202, 0.08);
  border: 1px solid rgba(247, 178, 202, 0.14);
  color: var(--pink);
  font-weight: 600;
  font-size: 14px;
  transition: background 0.3s, transform 0.3s, box-shadow 0.3s;
}

.chip.active {
  background: rgba(247, 178, 202, 0.22);
  transform: translateY(-2px);
  box-shadow: 0 10px 30px -12px rgba(247, 178, 202, 0.7);
}

.chip-enter-from {
  opacity: 0;
  transform: translateY(8px) scale(0.96);
}

.chip-enter-active {
  transition: opacity 0.4s, transform 0.4s cubic-bezier(0.2, 0.8, 0.2, 1);
}

.chip-leave-active {
  display: none;
}

.roll {
  position: relative;
  height: 230px;
  border-radius: 12px;
  background: repeating-linear-gradient(to bottom, #121215 0 11px, #0e0e10 11px 12px);
  border: 1px solid var(--line-soft);
  overflow: hidden;
}

.bar-line {
  position: absolute;
  top: 0;
  bottom: 0;
  width: 1px;
  background: var(--line);
}

.note {
  position: absolute;
  width: calc(25% - 4px);
  margin-left: 2px;
  min-height: 6px;
  border-radius: 4px;
  background: linear-gradient(180deg, #f9bfd3, var(--pink));
  box-shadow: inset 0 -1px 0 rgba(0, 0, 0, 0.18);
  transition: top 0.55s cubic-bezier(0.2, 0.8, 0.2, 1), left 0.55s cubic-bezier(0.2, 0.8, 0.2, 1), height 0.4s, filter 0.3s, box-shadow 0.3s;
}

.note.bass {
  background: linear-gradient(180deg, #d6c6fb, var(--lilac));
}

.note.lit {
  filter: brightness(1.15);
  box-shadow: 0 0 18px rgba(247, 178, 202, 0.7);
}
</style>
