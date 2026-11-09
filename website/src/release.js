import { reactive } from 'vue';

export const release = reactive({
  version: '1.0.0',
  file: 'YannosayHarmony-1.0.0-Setup-x64.exe',
  size: 0,
  sha256: '',
  loaded: false
});

export const repoUrl = 'https://github.com/Yannosay/harmony';
export const repoPublic = import.meta.env.VITE_REPO_PUBLIC === '1';

export function downloadUrl() {
  return `/download/${release.file}`;
}

export function formatSize(bytes) {
  return bytes > 0 ? `${(bytes / 1024 / 1024).toFixed(1)} MB` : '';
}

fetch('/latest.json', { cache: 'no-cache' })
  .then((r) => (r.ok ? r.json() : null))
  .then((data) => {
    if (data?.version && data?.file) Object.assign(release, data, { loaded: true });
  })
  .catch(() => {});
