<script setup lang="ts">
const faqs: Array<{ q: string; a: string }> = [
  {
    q: 'Which hardware option should I pick?',
    a: 'The normal build is for a bare board or a case that holds it the right way up. The Clawd case mounts the board upside down, so pick the flipped build: the picture, touch and motion sensor are all turned to match.',
  },
  {
    q: 'Nothing happens when I click Install',
    a: "Make sure you're on Chrome or Edge on a desktop, not a phone or another browser - Web Serial isn't supported elsewhere. Also try a different USB cable; some cables are power-only and can't carry data.",
  },
  {
    q: "My board doesn't show up in the port list",
    a: 'The board connects through the ESP32-S3\'s own USB port (it shows as "USB JTAG/serial debug unit"), so no driver is needed. If it still doesn\'t appear, hold BOOT, tap PWR (or replug the cable), release BOOT, and try again.',
  },
  {
    q: 'It says "Open this page through HTTPS or localhost"',
    a: 'Web Serial only works in a secure context. Use the hosted page (HTTPS), or run it locally with npm run dev and open the localhost address - not a plain file:// path.',
  },
  {
    q: 'Will this erase my pairing or settings?',
    a: 'No. The install only writes the bootloader, partition table, and app. If the installer offers an "Erase device" option, leave it unchecked - checking it wipes Bluetooth pairing, settings and stats, and you\'d pair again from the desktop app.',
  },
]
</script>

<template>
  <section class="faq" aria-labelledby="faq-title">
    <h2 id="faq-title" class="about-title">Troubleshooting</h2>
    <div class="faq-list">
      <details v-for="(item, index) in faqs" :key="index" class="faq-item">
        <summary>{{ item.q }}</summary>
        <p>{{ item.a }}</p>
      </details>
    </div>
  </section>
</template>
