#!/bin/bash
# Build the firmware for the Seeed Studio XIAO ESP32-S3 Sense with arduino-cli.
#
# Uses the libraries from ./libraries (exact tested versions) and the esp32:esp32 3.1.0 core.
#
# Install once:
#   arduino-cli config init
#   arduino-cli config add board_manager.additional_urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
#   arduino-cli core update-index
#   arduino-cli core install esp32:esp32@3.1.0
#
# Usage:
#   ./build_xiao.sh [output_dir]
#   ARDUINO_CLI="/path/arduino-cli --config-file /path/arduino-cli.yaml" ./build_xiao.sh
#
# Output (flash addresses):
#   ESP32_PrusaConnectCam.ino.bootloader.bin  -> 0x0
#   ESP32_PrusaConnectCam.ino.partitions.bin  -> 0x8000
#   ESP32_PrusaConnectCam.ino.bin             -> 0x10000
#   ESP32_PrusaConnectCam.ino.merged.bin      -> 0x0 (everything in one file)

set -e
cd "$(dirname "$0")"
ARDUINO_CLI=${ARDUINO_CLI:-arduino-cli}
OUT=${1:-build/xiao-esp32-s3}
mkdir -p "$OUT"

if ! grep -q "^#define CAMERA_MODEL_XIAO_ESP32_S3_CAM  true" ESP32_PrusaConnectCam/mcu_cfg.h; then
  echo "ERROR: CAMERA_MODEL_XIAO_ESP32_S3_CAM is not enabled (true) in ESP32_PrusaConnectCam/mcu_cfg.h"
  exit 1
fi

$ARDUINO_CLI compile \
  -b esp32:esp32:XIAO_ESP32S3:USBMode=hwcdc,CDCOnBoot=default,MSCOnBoot=default,DFUOnBoot=default,UploadMode=default,CPUFreq=160,FlashMode=qio,FlashSize=8M,PartitionScheme=default_8MB,DebugLevel=none,PSRAM=opi,LoopCore=1,EventsCore=1,EraseFlash=none,JTAGAdapter=default \
  --libraries libraries \
  --output-dir "$OUT" \
  ESP32_PrusaConnectCam

rm -f "$OUT"/*.elf "$OUT"/*.map
echo "Firmware: $OUT"
ls -la "$OUT"
