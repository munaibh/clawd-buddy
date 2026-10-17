import { defineConfig } from 'vite-plus'
import vue from '@vitejs/plugin-vue'

// scripts/copy-release.mjs copies ../release/ into public/ (dev) or dist/
// (build) so the flasher can fetch the firmware images at runtime.
export default defineConfig({
  // relative asset paths, so dist/ works from any sub-path (GitHub Pages etc.)
  base: './',
  plugins: [
    vue({
      template: {
        compilerOptions: {
          isCustomElement: (tag) => tag === 'esp-web-install-button',
        },
      },
    }),
  ],
  run: {
    tasks: {
      dev: { command: 'node scripts/copy-release.mjs public && vp dev', cache: false },
      build: { command: 'vp build && node scripts/copy-release.mjs dist', dependsOn: ['app#build'], cache: false },
      // Flasher only, from the committed release/ images (used by CI).
      'build:site': { command: 'vp build && node scripts/copy-release.mjs dist', cache: false },
      check: { command: 'vp check' },
    },
  },
})
