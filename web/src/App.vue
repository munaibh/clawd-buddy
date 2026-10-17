<script setup lang="ts">
import { onMounted, ref } from 'vue'
import { useFirmware } from './composables/useFirmware'
import ThemeToggle from './components/ThemeToggle.vue'
import BrandHero from './components/BrandHero.vue'
import FlashTool from './components/FlashTool.vue'
import VisualPanel from './components/VisualPanel.vue'
import FlashDetails from './components/FlashDetails.vue'
import Faq from './components/Faq.vue'
import About from './components/About.vue'
import PageFooter from './components/PageFooter.vue'

const {
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
  dismissUpdate,
  setSelection,
  selectVariant,
  init,
} = useFirmware()

const flashTool = ref<InstanceType<typeof FlashTool> | null>(null)

function toggleTheme() {
  const next = document.documentElement.dataset.theme === 'light' ? 'dark' : 'light'
  document.documentElement.dataset.theme = next
  localStorage.setItem('clawd-theme', next)
}

function onVariantChange(id: string) {
  variantSelectValue.value = id
  void selectVariant(id)
}

function onVersionChange(index: number) {
  versionSelectValue.value = String(index)
  setSelection(variantSelectValue.value, currentVersions.value[index], index)
}

function onUpdateClick() {
  dismissUpdate()
  flashTool.value?.focusVersion()
}

onMounted(() => {
  // <esp-web-install-button> is registered by the module in index.html.
  void customElements.whenDefined('esp-web-install-button').then(init)
})
</script>

<template>
  <div class="page">
    <ThemeToggle @toggle="toggleTheme" />

    <BrandHero :update-visible="updateBubbleVisible" @update-click="onUpdateClick" />

    <main>
      <section class="hero" aria-labelledby="page-title">
        <FlashTool
          ref="flashTool"
          :variants="variants"
          :versions="currentVersions"
          :variant-value="variantSelectValue"
          :version-value="versionSelectValue"
          :firmware-version="firmwareVersion"
          :version-note="versionNote"
          :manifest-url="manifestUrl"
          :load-error="loadError"
          @variant-change="onVariantChange"
          @version-change="onVersionChange"
        />

        <VisualPanel :caption-version="captionVersion" :caption-path="captionPath" />
      </section>

      <FlashDetails :device-detail="deviceDetail" />

      <Faq />

      <About />
    </main>

    <PageFooter />
  </div>
</template>
