<script setup lang="ts">
import ClawdMark from './ClawdMark.vue'

withDefaults(defineProps<{ updateVisible?: boolean }>(), { updateVisible: false })
defineEmits<{ 'update-click': [] }>()
</script>

<template>
  <header class="brand-hero" aria-label="Clawd Buddy">
    <p class="wordmark">
      <span>CLAWD</span>
      <ClawdMark />
      <button
        type="button"
        class="update-bubble"
        :class="{ visible: updateVisible }"
        aria-label="A new firmware version is available"
        @click="$emit('update-click')"
      >
        <span>New</span>
        <span>Version</span>
      </button>
    </p>
    <p class="brand-tagline">Your Claude Desk Buddy</p>
    <hr class="divider" />
    <p class="brand-description">
      Clawd Buddy sits on your desk and keeps an eye on Claude: approve or deny its requests from a round touchscreen, and see at a glance when it's
      working, waiting or done.
    </p>
    <hr class="divider" />
  </header>
</template>
