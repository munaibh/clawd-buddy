import { ref, shallowRef } from 'vue'
import type { Variant, VersionEntry, VersionNote } from '../types'

const UPDATE_DISMISSED_KEY = 'clawd-update-dismissed'

// release/<variant>/versions.json is written by app/tools/copy_firmware.py;
// the ESP Web Tools manifest is built here: one factory image at offset 0.
export function useFirmware() {
  const variants = ref<Variant[]>([])
  const currentVersions = ref<VersionEntry[]>([])
  const variantSelectValue = ref('')
  const versionSelectValue = ref('0')

  const firmwareVersion = ref('…')
  const captionVersion = ref('Stable')
  const captionPath = ref('…')
  const deviceDetail = ref('For the Waveshare ESP32-S3-Touch-LCD-1.46B (412x412 round display).')
  const versionNote = ref<VersionNote>({ version: '', note: '' })
  const updateBubbleVisible = ref(false)
  const loadError = ref('')

  const manifestUrl = shallowRef<string | null>(null)

  function pathFor(variantId: string, version: string) {
    return `release/${variantId}/firmware-v${version}.bin`
  }

  // Show "New Version" for a release from the last 7 days, once per version.
  function maybeShowUpdateBubble(versions: VersionEntry[]) {
    const latest = versions[0]
    if (!latest?.date) return
    const ageDays = (Date.now() - new Date(`${latest.date}T00:00:00`).getTime()) / 86400000
    if (ageDays < 0 || ageDays > 7) return
    if (localStorage.getItem(UPDATE_DISMISSED_KEY) === latest.version) return
    updateBubbleVisible.value = true
  }

  function dismissUpdate() {
    const latest = currentVersions.value[0]
    if (latest) localStorage.setItem(UPDATE_DISMISSED_KEY, latest.version)
    updateBubbleVisible.value = false
  }

  function setSelection(variantId: string, entry: VersionEntry, index: number) {
    const { version, note } = entry
    const path = pathFor(variantId, version)
    const variant = variants.value.find((v) => v.id === variantId)

    const manifest = {
      name: 'Clawd Buddy',
      version,
      new_install_prompt_erase: true,
      // absolute: relative paths would resolve against the blob: manifest URL
      builds: [{ chipFamily: 'ESP32-S3', parts: [{ path: new URL(path, window.location.href).href, offset: 0 }] }],
    }
    if (manifestUrl.value) URL.revokeObjectURL(manifestUrl.value)
    manifestUrl.value = URL.createObjectURL(new Blob([JSON.stringify(manifest)], { type: 'application/json' }))

    firmwareVersion.value = `v${version}`
    captionVersion.value = `Stable v${version}${index === 0 ? ' (latest)' : ''}`
    captionPath.value = path
    versionNote.value = note ? { version, note } : { version: '', note: '' }
    if (variant) deviceDetail.value = `For the Waveshare ${variant.label}.`
  }

  async function fetchJson<T>(path: string): Promise<T | null> {
    try {
      const res = await fetch(path)
      if (!res.ok) return null
      return (await res.json()) as T
    } catch {
      return null // missing, or Vite's SPA fallback served index.html
    }
  }

  async function selectVariant(variantId: string) {
    const versions = (await fetchJson<VersionEntry[]>(`release/${variantId}/versions.json`)) ?? []
    currentVersions.value = versions
    versionSelectValue.value = '0'
    loadError.value = versions.length ? '' : 'No firmware has been built for this board yet.'
    maybeShowUpdateBubble(versions)
    if (versions.length > 0) setSelection(variantId, versions[0], 0)
  }

  async function init() {
    variants.value = (await fetchJson<Variant[]>('release/variants.json')) ?? []
    if (!variants.value.length) {
      loadError.value = "Couldn't load release/variants.json."
      return
    }
    variantSelectValue.value = variants.value[0].id
    await selectVariant(variants.value[0].id)
  }

  return {
    variants,
    currentVersions,
    variantSelectValue,
    versionSelectValue,
    firmwareVersion,
    captionVersion,
    captionPath,
    deviceDetail,
    versionNote,
    updateBubbleVisible,
    loadError,
    manifestUrl,
    pathFor,
    dismissUpdate,
    setSelection,
    selectVariant,
    init,
  }
}
