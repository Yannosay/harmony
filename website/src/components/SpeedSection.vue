<script setup>
const shortcuts = [
  { keys: ['Space'], label: 'Play and pause' },
  { keys: ['L'], label: 'Pattern or song mode' },
  { keys: ['Ctrl', 'K'], label: 'Command palette' },
  { keys: ['F6', 'F7', 'F9'], label: 'Rack, piano roll, mixer' },
  { keys: ['Ctrl', 'G'], label: 'Chord generator' },
  { keys: ['Ctrl', 'B'], label: 'Duplicate selection' },
  { keys: ['P', 'E', 'C', 'T'], label: 'Draw, select, slice, mute' },
  { keys: ['Ctrl', 'R'], label: 'Render to audio' }
];

const stats = [
  { value: 'C++20', label: 'Native engine, no web views' },
  { value: 'Lock-free', label: 'Edit while it plays, no dropouts' },
  { value: '24 + 1', label: 'Mixer inserts with PDC' },
  { value: 'SHA-256', label: 'Verified automatic updates' }
];
</script>

<template>
  <section class="speed">
    <div class="container">
      <div class="layout">
        <div>
          <span class="eyebrow" v-reveal>Feels like home</span>
          <h2 class="section-title" v-reveal="80">Your shortcuts already work.</h2>
          <p class="section-lead" v-reveal="160">
            Familiar pattern-based workflow, function keys for every window, and scrolling with real inertia. Zoom with
            Ctrl+wheel, pan with the middle mouse button and fling, Alt to bypass the snap.
          </p>
          <div class="stats">
            <div v-for="(s, i) in stats" :key="s.label" class="stat" v-reveal="i * 80">
              <strong>{{ s.value }}</strong>
              <span>{{ s.label }}</span>
            </div>
          </div>
        </div>
        <ul class="keys card" v-reveal="120">
          <li v-for="s in shortcuts" :key="s.label">
            <span>{{ s.label }}</span>
            <span class="combo"><kbd v-for="k in s.keys" :key="k">{{ k }}</kbd></span>
          </li>
        </ul>
      </div>
    </div>
  </section>
</template>

<style scoped>
.speed {
  padding: 120px 0 40px;
}

.layout {
  display: grid;
  gap: 40px;
  align-items: center;
}

@media (min-width: 980px) {
  .layout { grid-template-columns: 1fr 1fr; gap: 64px; }
}

.stats {
  display: grid;
  grid-template-columns: repeat(2, 1fr);
  gap: 26px 20px;
  margin-top: 40px;
}

.stat strong {
  display: block;
  font-size: 26px;
  font-weight: 600;
  background: linear-gradient(100deg, var(--pink), var(--lilac));
  -webkit-background-clip: text;
  background-clip: text;
  color: transparent;
}

.stat span {
  font-size: 11px;
  color: var(--text-dim);
  font-weight: 300;
}

.keys {
  list-style: none;
  margin: 0;
  padding: 10px;
}

.keys li {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 12px;
  padding: 14px 14px;
  border-radius: 10px;
  font-size: 12px;
  color: var(--text-dim);
  transition: background 0.2s, color 0.2s;
}

.keys li:hover {
  background: var(--raised);
  color: var(--text);
}

.keys li + li {
  border-top: 1px solid var(--line-soft);
}

.combo {
  display: flex;
  gap: 5px;
  flex-wrap: wrap;
  justify-content: flex-end;
}
</style>
