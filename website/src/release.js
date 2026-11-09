import { reactive } from 'vue';

export function displayVersion(version) {
  const [numbers, pre] = version.split('-');
  if (!pre) return numbers;
  const [stage, ...rest] = pre.split('.');
  return `${stage[0].toUpperCase()}${stage.slice(1)} ${numbers}${rest.length ? '.' + rest.join('.') : ''}`;
}

export const release = reactive({
  version: __HARMONY_VERSION__,
  file: `YannosayHarmony-${__HARMONY_VERSION__}-Setup-x64.exe`,
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
