#!/bin/sh
# Runs a local PlatformIO ($PIO, `pio` on PATH, or ~/.platformio) if there is
# one, else builds in Docker with toolchains cached in the clawd-buddy-pio
# volume. PIO_DOCKER=1 forces Docker. Docker on macOS can't reach USB, so
# upload/monitor are refused there.
set -e

APP_DIR=$(cd "$(dirname "$0")/.." && pwd)
ROOT_DIR=$(dirname "$APP_DIR")
IMAGE=clawd-buddy-pio
VOLUME=clawd-buddy-pio

local_pio() {
  if [ -n "$PIO" ]; then echo "$PIO"; return 0; fi
  if command -v pio >/dev/null 2>&1; then command -v pio; return 0; fi
  if [ -x "$HOME/.platformio/penv/bin/pio" ]; then echo "$HOME/.platformio/penv/bin/pio"; return 0; fi
  return 1
}

if [ "${PIO_DOCKER:-0}" != 1 ] && P=$(local_pio); then
  cd "$APP_DIR"
  exec "$P" "$@"
fi

if ! command -v docker >/dev/null 2>&1; then
  echo "pio.sh: PlatformIO isn't installed and Docker isn't available." >&2
  echo "        Install either: pip install platformio, or Docker Desktop." >&2
  exit 1
fi
if ! docker info >/dev/null 2>&1; then
  echo "pio.sh: Docker is installed but not running. Start Docker Desktop and try again." >&2
  exit 1
fi

for arg in "$@"; do
  if [ "$arg" = upload ] || [ "$1" = device ]; then
    echo "pio.sh: building in Docker, which can't reach the board's USB port." >&2
    echo "        Flash with the web flasher instead: npm run dev, then Install." >&2
    echo "        (Or install PlatformIO locally to upload from the command line.)" >&2
    exit 1
  fi
done

if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
  echo "pio.sh: no local PlatformIO, building the $IMAGE Docker image (once)..." >&2
  docker build -t "$IMAGE" "$APP_DIR/tools/docker"
fi

TTY=
[ -t 1 ] && TTY=-t
# Separate build dir: objects from a local (macOS) toolchain aren't
# reusable by the Linux one in the container.
exec docker run --rm $TTY \
  -v "$ROOT_DIR:/work" \
  -v "$VOLUME:/root/.platformio" \
  -e PLATFORMIO_BUILD_DIR=/work/app/.pio/build-docker \
  -w /work/app \
  "$IMAGE" "$@"
