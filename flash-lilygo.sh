#!/bin/bash
# Flash Script für LILYGO T5 4.7" Plus Test-Build
set -e

# Farben
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m'

# Build-Verzeichnis
BUILD_DIR="/Users/holger/Repositories/esphome/tests/test_build_components/build/.esphome/build/componenttestesp32s3ard/.pioenvs/componenttestesp32s3ard"

# Port - kann als Argument übergeben werden
PORT="${1}"

echo -e "${GREEN}=== LILYGO T5 4.7\" Plus Flash ===${NC}"
echo ""

# Zeige verfügbare Ports wenn keiner angegeben
if [ -z "$PORT" ]; then
    echo -e "${YELLOW}Verfügbare Ports:${NC}"
    ls -1 /dev/cu.* 2>/dev/null | grep -E "usbmodem|usbserial" || echo "Kein USB-Gerät gefunden"
    echo ""
    echo -e "${RED}Verwendung: $0 <PORT>${NC}"
    echo -e "Beispiel: $0 /dev/cu.usbmodem1101"
    exit 1
fi

# Prüfe ob Port existiert
if [ ! -e "${PORT}" ]; then
    echo -e "${RED}Fehler: Port ${PORT} nicht gefunden!${NC}"
    exit 1
fi

# Prüfe Build-Dateien
BOOTLOADER="${BUILD_DIR}/bootloader.bin"
PARTITIONS="${BUILD_DIR}/partitions.bin"
FIRMWARE="${BUILD_DIR}/firmware.bin"

if [ ! -f "$BOOTLOADER" ] || [ ! -f "$PARTITIONS" ] || [ ! -f "$FIRMWARE" ]; then
    echo -e "${RED}Fehler: Build-Dateien nicht gefunden!${NC}"
    echo -e "${YELLOW}Führe zuerst den Build aus:${NC}"
    echo "python3 script/run-in-env.py ./script/test_build_components -c lilygo_t5_47_plus -t esp32-s3-ard"
    exit 1
fi

echo -e "${GREEN}✓ Build-Dateien gefunden${NC}"
echo -e "${BLUE}Bootloader: $(ls -lh $BOOTLOADER | awk '{print $5}')${NC}"
echo -e "${BLUE}Partitions: $(ls -lh $PARTITIONS | awk '{print $5}')${NC}"
echo -e "${BLUE}Firmware:   $(ls -lh $FIRMWARE | awk '{print $5}')${NC}"
echo ""

# Prüfe ob esptool verfügbar ist
if ! command -v esptool.py &> /dev/null; then
    echo -e "${RED}esptool nicht gefunden!${NC}"
    echo -e "${YELLOW}Installiere mit: pip3 install esptool${NC}"
    exit 1
fi

echo -e "${BLUE}Flashe auf ${PORT}...${NC}"
echo ""

# Flash mit esptool
esptool.py \
    --before default_reset \
    --after hard_reset \
    --baud 460800 \
    --port "${PORT}" \
    --chip esp32s3 \
    write_flash -z \
    --flash_mode qio \
    --flash_freq 80m \
    --flash_size detect \
    0x0 "${BOOTLOADER}" \
    0x8000 "${PARTITIONS}" \
    0x10000 "${FIRMWARE}"

echo ""
echo -e "${GREEN}✓ Flash erfolgreich!${NC}"
echo -e "${BLUE}Logs anzeigen mit:${NC}"
echo "esptool.py --port ${PORT} monitor"
