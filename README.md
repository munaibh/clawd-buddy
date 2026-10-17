# Clawd Buddy

A desk companion for Claude on the round Waveshare **ESP32-S3-Touch-LCD-1.46B**
(412x412 SPD2010 LCD, touch). It pairs with the Claude desktop app over
Bluetooth: permission requests pop up on the screen with big **Allow** /
**Deny** buttons, and the rest of the time Clawd's face keeps you company.

Pages (swipe left/right): **Clawd's face** (eyes follow what Claude is doing,
small clock) - **Home** (pet, big clock, battery, activity) - **Controls**
(mute, brightness, sleep, night mode). Approval cards take over whenever
Claude needs you. Wakes when picked up; dims and goes quiet at night.

Based on Anthropic's
[claude-desktop-buddy](https://github.com/anthropics/claude-desktop-buddy)
([docs/REFERENCE.md](docs/REFERENCE.md) is its Bluetooth protocol).

## Layout

- **`app/`** - the firmware: a PlatformIO project (`platformio.ini`, `src/`).
  `src/hal_ws146b/` is the Waveshare hardware layer behind the M5StickC API
  the original firmware was written for; `src/round_ui.h` is the round UI.
- **`web/`** - the browser flasher (Vue + Vite + ESP Web Tools).
- **`release/`** - versioned firmware images per board variant, plus
  `meta/VERSION.txt` and `meta/CHANGELOG.md`. Written by `app/`'s build,
  read by `web/`. It sits beside both because it belongs to neither.
- **`hardware/`** - Waveshare's 3D model of the board (STEP + STL) and the
  printable Clawd-shaped case in `hardware/clawd/`, with its own README.
- **`docs/`** - `REFERENCE.md`, the Bluetooth protocol the desktop app speaks.

## Firmware variants

| Variant (PlatformIO env) | Use it for                                                                                                            |
| ------------------------ | --------------------------------------------------------------------------------------------------------------------- |
| `waveshare-146b`         | Bare board, or a case that holds it the normal way up                                                                 |
| `waveshare-146b-flipped` | The Clawd case, which mounts the board upside down (`-DWS146B_ROTATE_180`: display, touch and IMU turned 180 degrees) |

## Running things from the repo root

The repo uses [Vite+](https://viteplus.dev) (`vp`), set up like
[munsonlabs/packages](https://github.com/munsonlabs/packages): `app/` and
`web/` are npm workspaces, each defines its tasks in its own `vite.config.ts`
(`run.tasks`), and the root scripts fan out with `vp run -r`.

```sh
npm install                 # once (then `vp config --hooks --no-agent` for the pre-commit hook)

npm run build               # firmware (both variants -> release/), then the flasher -> web/dist/
npm run dev                 # web flasher dev server (http://localhost:5173)
npm run check               # format + lint + type check
npm run fmt                 # format

npm run upload              # flash the flipped build over USB
ENV=waveshare-146b npm run upload   # ...or the normal one
npm run monitor             # serial log at 115200
```

Single workspace tasks: `vp run --filter app build` (firmware only),
`vp run --filter app build:flipped`, `vp run --filter web dev`. The web build
depends on `app#build`, so it always ships the firmware it was built with.
Firmware tasks aren't cached by `vp`: they write into `release/`, outside the
workspace.

### PlatformIO or Docker

The firmware tasks run through `app/tools/pio.sh`, which picks:

1. **A local PlatformIO** if there is one (`$PIO`, `pio` on your PATH, or
   `~/.platformio/penv/bin/pio`). Install with `pip install platformio` or
   `brew install platformio`.
2. **Otherwise Docker**: it builds the `clawd-buddy-pio` image
   (`app/tools/docker/Dockerfile`, PlatformIO 6.2.0) the first time and runs
   the build in it. Toolchains are cached in the `clawd-buddy-pio` volume, so
   only the first build downloads them. Docker Desktop must be running.
   `PIO_DOCKER=1 npm run build` forces Docker even with PlatformIO installed.

Docker on macOS can't reach USB devices, so in Docker mode `upload` and
`monitor` stop with a message: flash with the web flasher (`npm run dev`,
then **Install**) instead. Docker builds use their own build folder
(`app/.pio/build-docker/`) and write to `release/` the same way.

Without `vp`: `sh app/tools/pio.sh run`, or with PlatformIO installed
`cd app && pio run`, `pio run -e waveshare-146b-flipped -t upload`.

### Tooling

- `vite.config.ts` (root): shared `fmt` and `lint` settings for every
  workspace, and `staged` for the pre-commit hook.
- `.vite-hooks/pre-commit`: runs `vp staged` (`vp check --fix` on staged
  files). Installed by `vp config`; the generated `.vite-hooks/_/` dispatcher
  isn't committed.
- `.npmrc`: `ignore-scripts` (so `prepare` doesn't run on install; run
  `vp config` yourself), `min-release-age=3`, `save-exact`, `engine-strict`.
- `.node-version`: the Node version `vp env` uses.

## Web flasher

`web/` flashes a prebuilt image from Chrome or Edge using
[ESP Web Tools](https://esphome.github.io/esp-web-tools/) (Web Serial): pick
the **Hardware** variant and **Firmware version**, click **Install**. No
toolchain, just a USB-C data cable. Needs HTTPS or `localhost`.

How it works: an ESP32-S3 boots from several images at fixed offsets
(bootloader `0x0`, partitions `0x8000`, OTA selector `0xe000`, app `0x10000`).
Each build merges them into one `release/<variant>/firmware-vX.Y.Z.bin`
flashed at `0x0`. The page reads `release/variants.json` and
`release/<variant>/versions.json` at runtime and builds the ESP Web Tools
manifest in the browser, so routine releases need no web changes.

`release/` is outside what Vite serves, so `web/scripts/copy-release.mjs`
copies it into `web/public/release/` before the dev server starts and into
`web/dist/release/` after a build. That makes `web/dist/` self-contained:
upload it to any static host (GitHub Pages, Netlify...).

### Hosting on GitHub Pages

`.github/workflows/pages.yml` publishes the flasher whenever `web/` or
`release/` changes on `main` (or by hand from the Actions tab). It runs
`vp run --filter web build:site`, which builds the page from the firmware
images already committed in `release/` (no PlatformIO in CI), and deploys
`web/dist/`. One-time setup on GitHub: **Settings -> Pages -> Source: GitHub
Actions**. Pages serves over HTTPS, which Web Serial needs.

So the release flow is: build locally (`npm run build`), commit `release/`,
push to `main`, and the hosted flasher offers the new version.

The device image in the flasher (`web/src/assets/device-placeholder.svg`) is
a placeholder illustration; swap it for a photo in
`web/src/components/VisualPanel.vue`.

## Cutting a release

1. Bump `release/meta/VERSION.txt` (e.g. `0.2.0`).
2. Add a matching `## 0.2.0` section to `release/meta/CHANGELOG.md`.
3. `npm run build` (or `vp run --filter app build` for the firmware alone).

The post-build hook (`app/tools/copy_firmware.py`) writes
`release/<variant>/firmware-v0.2.0.bin` for both variants and adds the version
(with its changelog note and date) to the top of each `versions.json`. The
flasher's version dropdown then offers it, with older versions still
available. Rebuilding the same version keeps its original date.

To add a board: a new `[env:...]` in `app/platformio.ini` (with
`extra_scripts = post:tools/copy_firmware.py`, inherited via `extends`), an
entry in `release/variants.json`, then build.

## Connecting to Claude

In the Claude desktop app: **Help -> Troubleshooting -> Enable Developer
Mode**, then **Developer -> Open Hardware Buddy...**, **Connect**, pick
**Claude-XXXX** and type the passkey shown on the device. It reconnects by
itself after that. Only sessions in the desktop app (Claude Code or Cowork)
reach the device.
