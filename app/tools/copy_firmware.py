"""
PlatformIO post-build hook: publishes each build into ../release/<env>/.

  - firmware-vX.Y.Z.bin: merged factory image (bootloader + partitions +
    boot_app0 + app) flashed at 0x0, built with esptool merge_bin from the
    same pieces and offsets `pio run -t upload` writes.
  - versions.json: {"version", "note", "date"} prepended for the flasher.

Version from release/meta/VERSION.txt, note from the matching "## X.Y.Z"
section of release/meta/CHANGELOG.md. Rebuilding a version keeps its date.
release/variants.json is hand-maintained: add an entry per new board env.
"""

import json
import os
import shutil
import subprocess
from datetime import date

Import("env")


def read_version(repo_root):
    path = os.path.join(repo_root, "release", "meta", "VERSION.txt")
    if not os.path.isfile(path):
        print("copy_firmware.py: no release/meta/VERSION.txt, using 0.0.0")
        return "0.0.0"
    with open(path) as f:
        return f.read().strip()


def read_changelog_note(repo_root, version):
    """Body of the "## <version>" section of CHANGELOG.md as one line."""
    path = os.path.join(repo_root, "release", "meta", "CHANGELOG.md")
    if not os.path.isfile(path):
        return ""
    note, capturing = [], False
    with open(path) as f:
        for line in f:
            s = line.strip()
            if s.startswith("## "):
                if capturing:
                    break
                capturing = s[3:].strip() == version
                continue
            if capturing and s:
                note.append(s)
    return " ".join(note)


def update_versions_index(dest_dir, version, note):
    path = os.path.join(dest_dir, "versions.json")
    versions = []
    if os.path.isfile(path):
        with open(path) as f:
            versions = json.load(f)
    existing = next((v for v in versions if v["version"] == version), None)
    released = (existing or {}).get("date") or date.today().isoformat()
    versions = [v for v in versions if v["version"] != version]
    versions.insert(0, {"version": version, "note": note, "date": released})
    with open(path, "w") as f:
        json.dump(versions, f, indent=4)
        f.write("\n")
    return path


def make_factory_bin(env, out_path):
    """Merge bootloader, partitions, boot_app0 and the app into one image."""
    build_dir = env.subst("$BUILD_DIR")
    app_bin = os.path.join(build_dir, env.subst("$PROGNAME") + ".bin")
    built = os.path.join(build_dir, env.subst("$PROGNAME") + ".factory.bin")
    if os.path.isfile(built):                     # newer platforms make it for us
        shutil.copy2(built, out_path)
        return True

    parts = []
    for offset, image in env.get("FLASH_EXTRA_IMAGES", []):
        parts += [env.subst(offset), env.subst(image)]
    parts += [env.subst("$ESP32_APP_OFFSET") or "0x10000", app_bin]

    esptool = os.path.join(env.PioPlatform().get_package_dir("tool-esptoolpy"), "esptool.py")
    cmd = [env.subst("$PYTHONEXE"), esptool, "--chip", env.BoardConfig().get("build.mcu", "esp32s3"),
           "merge_bin", "-o", out_path,
           "--flash_mode", "keep", "--flash_freq", "keep", "--flash_size", "keep"] + parts
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print("copy_firmware.py: merge_bin failed:\n" + result.stdout + result.stderr)
        return False
    return True


def publish(source, target, env):
    repo_root = os.path.dirname(env.subst("$PROJECT_DIR"))   # app/ -> repo root
    pioenv = env.subst("$PIOENV")
    version = read_version(repo_root)
    dest_dir = os.path.join(repo_root, "release", pioenv)
    os.makedirs(dest_dir, exist_ok=True)

    dest = os.path.join(dest_dir, f"firmware-v{version}.bin")
    if not make_factory_bin(env, dest):
        return
    print(f"copy_firmware.py: [{pioenv}] wrote {os.path.relpath(dest, repo_root)}")
    index = update_versions_index(dest_dir, version, read_changelog_note(repo_root, version))
    print(f"copy_firmware.py: [{pioenv}] updated {os.path.relpath(index, repo_root)}")


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", publish)

# Bumping the version or editing the changelog alone must still re-run the
# hook, so make the app image depend on them.
_root = os.path.dirname(env.subst("$PROJECT_DIR"))
env.Depends("$BUILD_DIR/${PROGNAME}.bin", [
    os.path.join(_root, "release", "meta", "VERSION.txt"),
    os.path.join(_root, "release", "meta", "CHANGELOG.md"),
])
