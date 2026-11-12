<script setup>
import AppIcon from './AppIcon.vue';

const features = [
  { icon: 'download', title: 'Opens your FL Studio projects', text: 'Channels, patterns, notes, the arrangement, routing and mixer names come across from any .flp since FL 12.9. FL effects become Harmony effects.' },
  { icon: 'wand', title: 'Parts that follow your chords', text: 'Write chords or let the generator do it, then get a bassline, arpeggio or melody that follows them, in key, in one click.' },
  { icon: 'automation', title: 'Automation where you arrange', text: 'Right-click any knob, or press Automate in a plugin window to grab the last control you touched. Drag points and bend curves right in the playlist.' },
  { icon: 'playlist', title: 'MIDI in and out', text: 'Paste MIDI you copied elsewhere, drop .mid files on the piano roll, or drag your notes straight out into another DAW.' },
  { icon: 'piano', title: 'Velocity without hunting', text: 'Alt+scroll over a note to set its velocity. Stems grab from a wide area, chords move together, selections scale as one.' },
  { icon: 'scale', title: 'Ghost notes that take you there', text: 'See the other channels behind your notes, then double-click one to jump to that channel. F12 gives the piano roll the whole screen.' },
  { icon: 'sparkle', title: 'Effects where you drop them', text: 'Eleven built-in effects with a live EQ curve and gain reduction meters. Drop any effect on any of 64 mixer strips, master included.' },
  { icon: 'search', title: 'Every pattern, one click away', text: 'All patterns live in the pattern bar. Click to edit, drag straight into the song. Ctrl+K reaches every other command.' },
  { icon: 'shield', title: 'Nothing gets lost', text: 'Plugins are scanned in their own process, so a broken one cannot crash your session, and the time machine keeps versions of your work.' }
];
</script>

<template>
  <section id="features" class="features">
    <div class="container">
      <h2 class="section-title" v-reveal="80">Small things that save hours.</h2>
      <p class="section-lead" v-reveal="160">
        Bring your own instruments as VST3, VST, LV2, SoundFonts or samples. Harmony takes care of the rest.
      </p>
      <div class="grid">
        <article v-for="(f, i) in features" :key="f.title" class="feature card" v-reveal="(i % 3) * 90">
          <div class="icon"><AppIcon :name="f.icon" /></div>
          <h3>{{ f.title }}</h3>
          <p>{{ f.text }}</p>
        </article>
      </div>
    </div>
  </section>
</template>

<style scoped>
.features {
  padding: 120px 0 40px;
}

.grid {
  display: grid;
  gap: 16px;
  margin-top: 48px;
}

@media (min-width: 640px) {
  .grid { grid-template-columns: repeat(2, 1fr); }
}

@media (min-width: 1000px) {
  .grid { grid-template-columns: repeat(3, 1fr); }
}

.feature {
  position: relative;
  padding: 28px 26px 26px;
  overflow: hidden;
  transition: transform 0.4s cubic-bezier(0.2, 0.8, 0.2, 1), border-color 0.3s, opacity 0.9s cubic-bezier(0.2, 0.8, 0.2, 1) var(--reveal-delay, 0ms);
}

.feature::after {
  content: '';
  position: absolute;
  inset: -1px;
  border-radius: inherit;
  background: radial-gradient(400px circle at 0% 0%, rgba(247, 178, 202, 0.12), transparent 50%);
  opacity: 0;
  transition: opacity 0.4s;
  pointer-events: none;
}

.feature:hover {
  transform: translateY(-4px);
  border-color: #34343c;
}

.feature:hover::after {
  opacity: 1;
}

.icon {
  display: grid;
  place-items: center;
  width: 44px;
  height: 44px;
  border-radius: 12px;
  background: rgba(247, 178, 202, 0.1);
  color: var(--pink);
  margin-bottom: 20px;
}

.icon svg {
  width: 22px;
  height: 22px;
}

h3 {
  font-size: 15px;
  font-weight: 600;
  margin: 0 0 8px;
}

p {
  font-size: 12.5px;
  font-weight: 300;
  color: var(--text-dim);
  margin: 0;
}
</style>
