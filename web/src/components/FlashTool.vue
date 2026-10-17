<script setup lang="ts">
import { ref } from 'vue'
import type { Variant, VersionEntry, VersionNote } from '../types'

withDefaults(
  defineProps<{
    variants?: Variant[]
    versions?: VersionEntry[]
    variantValue?: string
    versionValue?: string
    firmwareVersion?: string
    versionNote?: VersionNote
    manifestUrl?: string | null
    loadError?: string
  }>(),
  {
    variants: () => [],
    versions: () => [],
    variantValue: '',
    versionValue: '0',
    firmwareVersion: '…',
    versionNote: () => ({ version: '', note: '' }),
    manifestUrl: null,
    loadError: '',
  },
)

const emit = defineEmits<{
  'variant-change': [id: string]
  'version-change': [index: number]
}>()

const versionSelect = ref<HTMLSelectElement | null>(null)

function onVariantChange(event: Event) {
  emit('variant-change', (event.target as HTMLSelectElement).value)
}

function onVersionChange(event: Event) {
  emit('version-change', Number((event.target as HTMLSelectElement).value))
}

// Lets the parent jump here when the "New Version" bubble is clicked.
function focusVersion() {
  versionSelect.value?.scrollIntoView({ behavior: 'smooth', block: 'center' })
  versionSelect.value?.focus()
}

defineExpose({ focusVersion })
</script>

<template>
  <div class="tool">
    <h1 id="page-title">Install Clawd Buddy</h1>
    <p class="lead">
      Install the firmware straight from your browser. Connect the board with a USB-C data cable, pick your setup, then start the install below.
    </p>

    <div class="field">
      <label for="variant-select">Hardware</label>
      <select id="variant-select" autocomplete="off" :value="variantValue" @change="onVariantChange">
        <option v-for="variant in variants" :key="variant.id" :value="variant.id">
          {{ variant.label }}
        </option>
      </select>
    </div>

    <div class="field">
      <label for="version-select">Firmware version</label>
      <select id="version-select" ref="versionSelect" autocomplete="off" :value="versionValue" :disabled="!versions.length" @change="onVersionChange">
        <option v-for="(entry, index) in versions" :key="entry.version" :value="String(index)">
          {{ index === 0 ? `v${entry.version} (latest)` : `v${entry.version}` }}
        </option>
      </select>
    </div>
    <p v-if="versionNote.note" class="version-note visible">
      <strong>What's new in v{{ versionNote.version }}: </strong>{{ versionNote.note }}
    </p>
    <p v-if="loadError" class="note warning">{{ loadError }}</p>

    <div class="release-info" aria-live="polite">
      <div class="fact">
        <span>Channel</span>
        <strong>Stable</strong>
      </div>
      <div class="fact">
        <span>Version</span>
        <strong>{{ firmwareVersion }}</strong>
      </div>
      <div class="fact">
        <span>Flash offset</span>
        <strong>0x0</strong>
      </div>
    </div>

    <p class="note">This writes the bootloader, partition table, and app in one step. No separate tools required.</p>
    <p class="note warning">
      Your Bluetooth pairing and settings are kept. If the installer offers an "Erase device" option, leave it unchecked; checking it wipes pairing,
      settings and stats.
    </p>

    <div class="install-row">
      <esp-web-install-button v-if="manifestUrl" :manifest="manifestUrl">
        <button class="primary-button" slot="activate" type="button">Install Clawd Buddy</button>
        <span slot="unsupported">Use Chrome or Edge on a desktop browser.</span>
        <span slot="not-allowed">Open this page through HTTPS or localhost.</span>
      </esp-web-install-button>
      <span class="support">Use Chrome or Edge with a USB data cable</span>
    </div>
  </div>
</template>
