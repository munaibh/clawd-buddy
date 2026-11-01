# Changelog

Newest first. `app/tools/copy_firmware.py` copies the section matching
`release/meta/VERSION.txt` into each variant's `versions.json`, which the web
flasher shows as "What's new". Bump `VERSION.txt` and add a section here
together, then build.

## 0.2.0

Clawd shows what Claude is doing under his eyes while it works, in a dark
speech panel on his face, and chimes when a job finishes, showing just that
job's reply. He plays dead when Claude hits a usage limit and comes back when
it's working again.

## 0.1.0

First release for the Waveshare ESP32-S3-Touch-LCD-1.46B: approve or deny
Claude's permission requests from the round touchscreen, Clawd's face with
mood eyes, clock and battery, mute/brightness/sleep controls, night mode,
wake on lift, and softer chimes. Flipped variant for the Clawd case.
