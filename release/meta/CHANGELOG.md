# Changelog

Newest first. `app/tools/copy_firmware.py` copies the section matching
`release/meta/VERSION.txt` into each variant's `versions.json`, which the web
flasher shows as "What's new". Bump `VERSION.txt` and add a section here
together, then build.

## 0.1.0

First release for the Waveshare ESP32-S3-Touch-LCD-1.46B: approve or deny
Claude's permission requests from the round touchscreen, Clawd's face with
mood eyes, clock and battery, mute/brightness/sleep controls, night mode,
wake on lift, and softer chimes. Flipped variant for the Clawd case.
