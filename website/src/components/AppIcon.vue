<script setup>
defineProps({ name: { type: String, required: true }, filled: { type: Boolean, default: false } });

const paths = {
  plug: 'M9 3.5V8M15 3.5V8M6 8h12v4c0 3.5-2.5 5-6 5s-6-1.5-6-5V8ZM12 17v4',
  piano: 'M5.5 5h13a2 2 0 0 1 2 2v10a2 2 0 0 1-2 2h-13a2 2 0 0 1-2-2V7a2 2 0 0 1 2-2ZM7.75 5v14M12 5v14M16.25 5v14',
  wave: 'M3 10.8v2.4M6 7.8v8.4M9 4.8v14.4M12 8.4v7.2M15 6v12M18 9.6v4.8M21 10.8v2.4',
  wand: 'M4.5 19.5l10-10M17 3.5v4M15 5.5h4M20 10.5v3M18.5 12h3M9 3.5v3M7.5 5h3',
  drum: 'M4 9a8 3 0 1 0 16 0a8 3 0 1 0 -16 0M4 9v7c0 3.5 16 3.5 16 0V9M7 3l4 5M17 3l-4 5',
  mixer: 'M6 4v16M12 4v16M18 4v16M3.8 7.5h4.4v3H3.8zM9.8 13.5h4.4v3H9.8zM15.8 5.5h4.4v3h-4.4z',
  download: 'M12 4v11M7.5 10.5 12 15l4.5-4.5M5 19.5h14',
  history: 'M4.5 7.8A8 8 0 1 1 4 12M2.8 4.8l1.8 3 3-1.2M12 8v4.5l3 2',
  search: 'M4.5 10a5.5 5.5 0 1 0 11 0a5.5 5.5 0 1 0 -11 0M14 14l5.5 5.5',
  magnet: 'M6 4.5V12c0 8.5 12 8.5 12 0V4.5M6 8.5h3.5M14.5 8.5H18',
  playlist: 'M4.5 5h8a1 1 0 0 1 1 1v2a1 1 0 0 1-1 1h-8a1 1 0 0 1-1-1V6a1 1 0 0 1 1-1ZM10.5 10h9a1 1 0 0 1 1 1v2a1 1 0 0 1-1 1h-9a1 1 0 0 1-1-1v-2a1 1 0 0 1 1-1ZM6.5 15h5a1 1 0 0 1 1 1v2a1 1 0 0 1-1 1h-5a1 1 0 0 1-1-1v-2a1 1 0 0 1 1-1Z',
  scale: 'M4 18h7M7 14h7M10 10h7M13 6h7',
  shield: 'M12 3l7 3v5c0 4.5-3 8-7 10c-4-2-7-5.5-7-10V6l7-3ZM9 12l2 2l4-4',
  bolt: 'M13 3L5 13h6l-1 8l8-10h-6l1-8Z',
  play: 'M7 4.5L19.5 12L7 19.5Z',
  pause: 'M6 5h4v14H6zM14 5h4v14h-4z',
  github: 'M12 2.5a9.5 9.5 0 0 0-3 18.5c.5.1.6-.2.6-.5v-1.7c-2.6.6-3.2-1.2-3.2-1.2c-.4-1.1-1-1.4-1-1.4c-.9-.6.1-.6.1-.6c1 .1 1.5 1 1.5 1c.9 1.5 2.3 1.1 2.9.8c.1-.6.3-1.1.6-1.3c-2.1-.2-4.3-1-4.3-4.7c0-1 .4-1.9 1-2.6c-.1-.2-.4-1.2.1-2.5c0 0 .8-.3 2.6 1a9 9 0 0 1 4.8 0c1.8-1.3 2.6-1 2.6-1c.5 1.3.2 2.3.1 2.5c.6.7 1 1.6 1 2.6c0 3.7-2.2 4.5-4.3 4.7c.3.3.6.9.6 1.8v2.6c0 .3.2.6.7.5A9.5 9.5 0 0 0 12 2.5Z',
  windows: 'M3 5.5l7.5-1v7H3zM11.5 4.4L21 3v8.5h-9.5zM3 12.5h7.5v7L3 18.5zM11.5 12.5H21V21l-9.5-1.4z',
  sparkle: 'M12 3l1.8 5.2L19 10l-5.2 1.8L12 17l-1.8-5.2L5 10l5.2-1.8ZM19 15l.8 2.2L22 18l-2.2.8L19 21l-.8-2.2L16 18l2.2-.8Z'
};
</script>

<template>
  <svg viewBox="0 0 24 24" aria-hidden="true" :fill="filled ? 'currentColor' : 'none'" :stroke="filled ? 'none' : 'currentColor'"
       stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round">
    <path :d="paths[name]" />
  </svg>
</template>
