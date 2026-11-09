<script setup>
import AppIcon from './AppIcon.vue';

const features = [
  { icon: 'plug', title: 'Every plugin you own', text: 'VST3, VST 2.x and LV2 instruments and effects. Scanned in a separate process, so a broken plugin never crashes your session.' },
  { icon: 'piano', title: 'A piano roll that keeps up', text: 'Ghost notes, scale highlighting and scale lock, velocity lane, slicing, live chord detection and inertial zoom.' },
  { icon: 'wand', title: 'Chord generator', text: 'Fifteen progressions in any key, smooth voice leading, bass lines, strums and arpeggios. One click into the piano roll.' },
  { icon: 'drum', title: 'Drum generator', text: 'Trap, drill, house, techno, drum & bass and more, mapped to your own kick, snare and hat channels by name.' },
  { icon: 'mixer', title: 'Mixer with delay compensation', text: '24 inserts and a master, ten effect slots each, solo, mute, meters and automatic plugin delay compensation.' },
  { icon: 'wave', title: 'SoundFonts and samples', text: 'Drop an SF2 and browse its presets, or a sample for a pitched sampler. Audio clips go straight onto the playlist.' },
  { icon: 'download', title: 'Render your way', text: 'WAV up to 32-bit float, FLAC, OGG and AIFF. Stems per insert, normalise, dither, and render in the background.' },
  { icon: 'history', title: 'Time machine', text: 'Harmony keeps versions of your project as you work. Jump back to any of them, even after a crash.' },
  { icon: 'search', title: 'Command palette', text: 'Press Ctrl+K and type. Every action in Harmony is two keystrokes away, with the shortcut shown next to it.' }
];
</script>

<template>
  <section id="features" class="features">
    <div class="container">
      <span class="eyebrow" v-reveal>Why Harmony</span>
      <h2 class="section-title" v-reveal="80">Everything a producer needs.<br />Nothing in the way.</h2>
      <p class="section-lead" v-reveal="160">
        Harmony comes with no built-in instruments, on purpose. It is a host built to make your own plugins and sounds feel
        faster than ever, written in native C++ for low latency and low CPU.
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
