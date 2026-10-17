<script setup lang="ts">
withDefaults(defineProps<{ deviceDetail?: string }>(), { deviceDetail: '' })
</script>

<template>
  <section class="details" aria-label="Flash details">
    <div class="detail">
      <h2>Install</h2>
      <p>This writes everything the device needs for its first start.</p>
    </div>
    <div class="detail">
      <h2>Device</h2>
      <p id="device-detail">{{ deviceDetail }}</p>
    </div>
    <div class="detail">
      <h2>Next step</h2>
      <p>In the Claude desktop app, enable Developer Mode, open Developer → Hardware Buddy, Connect, and type the passkey shown on the screen.</p>
    </div>
  </section>
</template>
