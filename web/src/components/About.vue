<script setup lang="ts">
const chips: string[] = ['Approve from your desk', 'Round touchscreen', 'Bluetooth', 'Night mode', 'Wake on lift']
</script>

<template>
  <section class="about" aria-labelledby="about-title">
    <h2 id="about-title" class="about-title">About Clawd Buddy</h2>
    <p class="lede">
      Clawd Buddy is a small desk companion for <strong>Claude</strong>. It pairs with the Claude desktop app over Bluetooth, shows permission
      requests as big <strong>Allow</strong> / <strong>Deny</strong> buttons, and the rest of the time Clawd's face keeps you company - eyes that scan
      while Claude works, close when it's idle, and light up when a task is done.
    </p>
    <p class="lede">
      It runs on the Waveshare ESP32-S3-Touch-LCD-1.46B and builds on Anthropic's open-source <em>claude-desktop-buddy</em> firmware.
    </p>
    <div class="chips">
      <span v-for="chip in chips" :key="chip" class="chip">{{ chip }}</span>
    </div>
  </section>
</template>
