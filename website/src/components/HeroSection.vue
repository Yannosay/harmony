<script setup>
import { onMounted, onUnmounted, ref } from 'vue';
import AppIcon from './AppIcon.vue';
import { downloadUrl, formatSize, release } from '../release.js';

const frame = ref(null);
const tilt = { x: 0, y: 0, tx: 0, ty: 0 };
let raf = 0;

function onMove(event) {
  const r = frame.value?.getBoundingClientRect();
  if (!r) return;
  tilt.tx = ((event.clientY - (r.top + r.height / 2)) / window.innerHeight) * -6;
  tilt.ty = ((event.clientX - (r.left + r.width / 2)) / window.innerWidth) * 8;
}

function loop() {
  tilt.x += (tilt.tx - tilt.x) * 0.08;
  tilt.y += (tilt.ty - tilt.y) * 0.08;
  if (frame.value) frame.value.style.transform = `perspective(1600px) rotateX(${8 + tilt.x}deg) rotateY(${tilt.y}deg)`;
  raf = requestAnimationFrame(loop);
}

onMounted(() => {
  if (!window.matchMedia('(prefers-reduced-motion: reduce)').matches) {
    window.addEventListener('pointermove', onMove, { passive: true });
    raf = requestAnimationFrame(loop);
  }
});

onUnmounted(() => {
  window.removeEventListener('pointermove', onMove);
  cancelAnimationFrame(raf);
});
</script>

<template>
  <section id="top" class="hero">
    <div class="glow" aria-hidden="true"></div>
    <div class="lanes" aria-hidden="true"></div>
    <div class="container content">
      <img class="app-icon" src="/brand/harmony-icon-512.png" alt="Harmony app icon" width="88" height="88" />
      <h1>Make music at the <span class="gradient">speed of thought.</span></h1>
      <p class="lead">
        A fast, dark digital audio workstation that hosts your VST3, VST and LV2 plugins, SoundFonts and samples.
        Pattern-based like you are used to, smoother than you expect.
      </p>
      <div class="cta">
        <a :href="downloadUrl()" class="btn btn-primary" download>
          <AppIcon name="windows" filled />
          Download for Windows
        </a>
        <a href="#tour" class="btn">Take the tour</a>
      </div>
      <p class="meta">Windows 10 and 11, 64-bit<template v-if="release.size"> · {{ formatSize(release.size) }}</template></p>
    </div>

    <div class="container stage">
      <div ref="frame" class="frame">
        <img src="/screenshots/harmony-playlist.webp" alt="Harmony showing the playlist with drum, chord and lead patterns above the channel rack" width="1720" height="1040" fetchpriority="high" />
        <div class="track" aria-hidden="true"><div class="playhead"></div></div>
        <div class="sheen" aria-hidden="true"></div>
      </div>
    </div>
  </section>
</template>

<style scoped>
.hero {
  position: relative;
  padding: 150px 0 40px;
  overflow: hidden;
}

.glow {
  position: absolute;
  inset: -20% -10% auto;
  height: 900px;
  background: radial-gradient(closest-side at 50% 35%, rgba(247, 178, 202, 0.22), transparent 70%),
    radial-gradient(closest-side at 65% 55%, rgba(199, 178, 250, 0.14), transparent 70%);
  filter: blur(10px);
  animation: breathe 9s ease-in-out infinite;
  pointer-events: none;
}

.lanes {
  position: absolute;
  inset: 0;
  background-image: linear-gradient(180deg,
    transparent 0 20px, rgba(255, 255, 255, 0.022) 20px 40px, transparent 40px 60px, rgba(255, 255, 255, 0.022) 60px 80px,
    transparent 80px 120px, rgba(255, 255, 255, 0.022) 120px 140px, transparent 140px 160px,
    rgba(255, 255, 255, 0.022) 160px 180px, transparent 180px 200px, rgba(255, 255, 255, 0.022) 200px 220px, transparent 220px 240px);
  background-size: 100% 240px;
  mask-image: radial-gradient(ellipse at 50% 25%, black, transparent 72%);
  pointer-events: none;
}

.content {
  position: relative;
  display: flex;
  flex-direction: column;
  align-items: center;
  text-align: center;
}

.app-icon {
  width: 88px;
  height: 88px;
  margin-bottom: 28px;
  filter: drop-shadow(0 18px 40px rgba(247, 178, 202, 0.35));
  animation: float 6s ease-in-out infinite;
}

h1 {
  font-size: clamp(36px, 7vw, 82px);
  font-weight: 600;
  line-height: 1.04;
  letter-spacing: -0.035em;
  margin: 18px 0 22px;
  max-width: 980px;
}

.gradient {
  background: linear-gradient(100deg, var(--pink), var(--lilac) 60%, var(--pink));
  background-size: 200% auto;
  -webkit-background-clip: text;
  background-clip: text;
  color: transparent;
  animation: shimmer 8s linear infinite;
}

.lead {
  max-width: 640px;
  color: var(--text-dim);
  font-size: clamp(14px, 1.6vw, 16px);
  font-weight: 300;
  margin: 0 0 34px;
}

.cta {
  display: flex;
  flex-wrap: wrap;
  justify-content: center;
  gap: 12px;
}

.meta {
  margin-top: 18px;
  font-size: 11px;
  color: var(--text-faint);
}

.stage {
  position: relative;
  margin-top: 70px;
}

.frame {
  position: relative;
  border-radius: 18px;
  overflow: hidden;
  border: 1px solid #2c2c33;
  box-shadow: 0 60px 120px -40px rgba(0, 0, 0, 0.9), 0 0 0 1px rgba(255, 255, 255, 0.03), 0 30px 120px -50px rgba(247, 178, 202, 0.45);
  transform: perspective(1600px) rotateX(8deg);
  transform-origin: 50% 0;
  will-change: transform;
}

.frame img {
  width: 100%;
  height: auto;
}

.track {
  position: absolute;
  top: 9%;
  left: 22.8%;
  right: 0;
  height: 44%;
  pointer-events: none;
}

.playhead {
  position: absolute;
  top: 0;
  bottom: 0;
  left: 0;
  width: 2px;
  background: linear-gradient(var(--text), rgba(245, 245, 246, 0.4));
  box-shadow: 0 0 16px rgba(247, 178, 202, 0.9);
  animation: sweep 9s linear infinite;
}

.playhead::before {
  content: '';
  position: absolute;
  top: -6px;
  left: -5px;
  border: 6px solid transparent;
  border-top-color: var(--text);
}

.sheen {
  position: absolute;
  inset: 0;
  background: linear-gradient(115deg, transparent 40%, rgba(255, 255, 255, 0.05) 50%, transparent 60%);
  background-size: 250% 100%;
  animation: sheen 7s ease-in-out infinite;
  pointer-events: none;
}

@keyframes sweep {
  from { left: 0; }
  to { left: 100%; }
}

@keyframes breathe {
  0%, 100% { opacity: 0.85; transform: scale(1); }
  50% { opacity: 1; transform: scale(1.06); }
}

@keyframes float {
  0%, 100% { transform: translateY(0); }
  50% { transform: translateY(-8px); }
}

@keyframes shimmer {
  to { background-position: 200% center; }
}

@keyframes sheen {
  0%, 60% { background-position: 120% 0; }
  100% { background-position: -120% 0; }
}
</style>
