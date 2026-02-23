#!/bin/bash
# Build and flash script for LILYGO T5 4.7" Plus
# Usage: ./build-and-flash.sh [port]
# Default port: /dev/cu.usbmodem1101

set -e

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
VENV="${REPO_DIR}/.venv/bin/python"
CONFIG="$(dirname "${BASH_SOURCE[0]}")/test.esp32-s3-ard.yaml"
PORT="${1:-/dev/cu.usbmodem1101}"

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${GREEN}=== LILYGO T5 4.7\" Plus – Build & Flash ===${NC}"
echo -e "Config: ${YELLOW}${CONFIG}${NC}"
echo -e "Port:   ${YELLOW}${PORT}${NC}"
echo ""

if [ ! -f "${VENV}" ]; then
    echo -e "${RED}ESPHome venv nicht gefunden: ${VENV}${NC}"
    exit 1
fi

echo -e "${GREEN}[1/2] Kompilieren...${NC}"
"${VENV}" -m esphome compile "${CONFIG}"

echo ""
echo -e "${GREEN}[2/2] Flashen auf ${PORT}...${NC}"
if [ ! -e "${PORT}" ]; then
    echo -e "${RED}Port ${PORT} nicht gefunden. Verfügbare Ports:${NC}"
    ls /dev/cu.usbmodem* 2>/dev/null || echo "Keine USB-Geräte gefunden"
    exit 1
fi

"${VENV}" -m esphome upload "${CONFIG}" --device "${PORT}"

echo ""
echo -e "${GREEN}✓ Fertig! Logs anzeigen mit:${NC}"
echo -e "  ${YELLOW}${VENV} -m esphome logs ${CONFIG} --device ${PORT}${NC}"
