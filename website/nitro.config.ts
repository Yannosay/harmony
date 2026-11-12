import { fileURLToPath } from 'node:url';
import { defineNitroConfig } from 'nitropack/config';

const security = {
  'x-content-type-options': 'nosniff',
  'referrer-policy': 'strict-origin-when-cross-origin'
};

export default defineNitroConfig({
  preset: 'cloudflare_module',
  compatibilityDate: '2026-10-01',
  srcDir: 'server',
  publicAssets: [{ dir: fileURLToPath(new URL('./dist', import.meta.url)), baseURL: '/', maxAge: 0 }],
  routeRules: {
    '/**': { headers: security },
    '/latest.json': { headers: { 'cache-control': 'no-cache', 'access-control-allow-origin': '*' } },
    '/assets/**': { headers: { 'cache-control': 'public, max-age=31536000, immutable' } },
    '/download/**': { headers: { 'cache-control': 'public, max-age=3600' } }
  },
  cloudflare: {
    deployConfig: true,
    wrangler: {
      name: 'harmony-yannosay',
      routes: [{ pattern: 'harmony.yannosay.com', custom_domain: true }]
    }
  }
});
