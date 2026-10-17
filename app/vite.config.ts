import { defineConfig } from 'vite-plus'

// Not cached: builds write images into ../release/, outside this workspace,
// so a cached replay would skip them.
const pio = 'sh tools/pio.sh'

export default defineConfig({
  run: {
    tasks: {
      build: { command: `${pio} run`, cache: false },
      'build:normal': { command: `${pio} run -e waveshare-146b`, cache: false },
      'build:flipped': { command: `${pio} run -e waveshare-146b-flipped`, cache: false },
      upload: { command: `${pio} run -e \${ENV:-waveshare-146b-flipped} -t upload`, cache: false },
      monitor: { command: `${pio} device monitor`, cache: false },
    },
  },
})
