import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';

const root = path.resolve(import.meta.dirname, '..', '..');
const version = fs.readFileSync(path.join(root, 'VERSION'), 'utf8').trim();
const name = `YannosayHarmony-${version}-Setup-x64.exe`;
const source = path.resolve(process.argv[2] ?? path.join(root, 'dist', name));

if (!fs.existsSync(source)) {
  console.error(`Installer not found: ${source}`);
  process.exit(1);
}

const publicDir = path.resolve(import.meta.dirname, '..', 'public');
const downloadDir = path.join(publicDir, 'download');
fs.rmSync(downloadDir, { recursive: true, force: true });
fs.mkdirSync(downloadDir, { recursive: true });
fs.copyFileSync(source, path.join(downloadDir, name));

const data = fs.readFileSync(source);
const manifest = {
  version,
  file: name,
  installer: `https://harmony.yannosay.com/download/${name}`,
  size: data.length,
  sha256: crypto.createHash('sha256').update(data).digest('hex'),
  published: new Date().toISOString()
};
fs.writeFileSync(path.join(publicDir, 'latest.json'), JSON.stringify(manifest, null, 2) + '\n');
console.log(manifest);
