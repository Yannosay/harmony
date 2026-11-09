import { createApp } from 'vue';
import '@fontsource/unbounded/300.css';
import '@fontsource/unbounded/400.css';
import '@fontsource/unbounded/500.css';
import '@fontsource/unbounded/600.css';
import '@fontsource/unbounded/700.css';
import './style.css';
import App from './App.vue';
import { reveal } from './reveal.js';

createApp(App).directive('reveal', reveal).mount('#app');
