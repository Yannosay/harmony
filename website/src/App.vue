<script setup>
import SiteNav from './components/SiteNav.vue';
import HeroSection from './components/HeroSection.vue';
import FeatureGrid from './components/FeatureGrid.vue';
import ChordDemo from './components/ChordDemo.vue';
import StepDemo from './components/StepDemo.vue';
import ScreenGallery from './components/ScreenGallery.vue';
import SpeedSection from './components/SpeedSection.vue';
import DownloadSection from './components/DownloadSection.vue';
import SiteFooter from './components/SiteFooter.vue';
</script>

<template>
  <SiteNav />
  <main>
    <HeroSection />
    <FeatureGrid />
    <section id="create" class="create">
      <div class="container">
        <span class="eyebrow" v-reveal>Built-in ideas, your sounds</span>
        <h2 class="section-title" v-reveal="80">Start with a progression.<br />Finish with a groove.</h2>
        <p class="section-lead" v-reveal="160">
          These are the same chord and drum generators that ship inside Harmony, running right here in your browser.
          In the DAW they write straight into the piano roll and channel rack.
        </p>
        <div class="demos">
          <ChordDemo v-reveal="100" />
          <StepDemo v-reveal="220" />
        </div>
      </div>
    </section>
    <ScreenGallery />
    <SpeedSection />
    <DownloadSection />
  </main>
  <SiteFooter />
</template>

<style scoped>
.create {
  padding: 120px 0 40px;
}

.demos {
  display: grid;
  gap: 20px;
  margin-top: 48px;
}

@media (min-width: 1020px) {
  .demos {
    grid-template-columns: 1.25fr 1fr;
  }
}
</style>
