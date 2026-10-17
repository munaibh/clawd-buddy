<script setup lang="ts">
import placeholder from '../assets/device-placeholder.svg'

withDefaults(defineProps<{ captionVersion?: string; captionPath?: string }>(), {
  captionVersion: 'Stable',
  captionPath: '…',
})
</script>

<template>
  <!-- Placeholder illustration: swap src for a real photo of the device. -->
  <aside class="visual" aria-label="Clawd Buddy device">
    <img :src="placeholder" alt="Clawd Buddy device (placeholder illustration)" />
    <div class="caption">
      <strong>{{ captionVersion }}</strong>
      <span>{{ captionPath }}</span>
    </div>
  </aside>
</template>
