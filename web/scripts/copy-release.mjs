// Copies ../release/ into public/ or dist/: the flasher fetch()es it at
// runtime, and without it Vite's SPA fallback serves index.html (no 404).
import { cpSync, existsSync, rmSync } from 'node:fs'
import { dirname, join } from 'node:path'
import { fileURLToPath } from 'node:url'

const webDir = dirname(dirname(fileURLToPath(import.meta.url)))
const src = join(dirname(webDir), 'release')
const destName = process.argv[2]
if (!['public', 'dist'].includes(destName)) {
  console.error('usage: copy-release.mjs <public|dist>')
  process.exit(1)
}
const dest = join(webDir, destName, 'release')
if (!existsSync(src)) {
  console.warn(`copy-release: no release/ at ${src}, skipping`)
  process.exit(0)
}
rmSync(dest, { recursive: true, force: true })
cpSync(src, dest, { recursive: true, dereference: true })
console.log(`copy-release: ${src} -> ${dest}`)
