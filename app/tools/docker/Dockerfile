# PlatformIO for app/tools/pio.sh when there's no local install. Toolchains
# go into the clawd-buddy-pio volume (/root/.platformio).
FROM python:3.11-slim

RUN apt-get update \
 && apt-get install -y --no-install-recommends git \
 && rm -rf /var/lib/apt/lists/*

RUN pip install --no-cache-dir platformio==6.2.0

WORKDIR /work/app
ENTRYPOINT ["pio"]
