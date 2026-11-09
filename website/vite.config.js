import fs from 'node:fs';
import { defineConfig } from 'vite';
import vue from '@vitejs/plugin-vue';

const version = fs.readFileSync(new URL('../VERSION', import.meta.url), 'utf8').trim();

export default defineConfig({
  plugins: [vue()],
  define: {
    __HARMONY_VERSION__: JSON.stringify(version)
  },
  build: {
    target: 'es2022',
    assetsInlineLimit: 0
  }
});
