#!/bin/bash

set -e

if [ ! -z "$TRAVIS_BUILD_DIR" ]; then
	export GITHUB_WORKSPACE="$TRAVIS_BUILD_DIR"
	export GITHUB_REPOSITORY="$TRAVIS_REPO_SLUG"
elif [ -z "$GITHUB_WORKSPACE" ]; then
	export GITHUB_WORKSPACE="$PWD"
	export GITHUB_REPOSITORY="me-no-dev/ESPAsyncTCP"
fi

CHUNK_INDEX=$1
CHUNKS_CNT=$2
BUILD_PIO=0
if [ "$#" -lt 2 ] || [ "$CHUNKS_CNT" -le 0 ]; then
	CHUNK_INDEX=0
	CHUNKS_CNT=1
elif [ "$CHUNK_INDEX" -gt "$CHUNKS_CNT" ]; then
	CHUNK_INDEX=$CHUNKS_CNT
elif [ "$CHUNK_INDEX" -eq "$CHUNKS_CNT" ]; then
	BUILD_PIO=1
fi

if [ "$BUILD_PIO" -eq 0 ]; then
	# ArduinoIDE Test
	source ./.github/scripts/install-arduino-ide.sh
	source ./.github/scripts/install-arduino-core-esp8266.sh

	echo "Installing ESPAsyncTCP ..."
	cp -rf "$GITHUB_WORKSPACE" "$ARDUINO_USR_PATH/libraries/ESPAsyncTCP"

	FQBN="esp8266com:esp8266:generic:eesz=4M1M,ip=lm2f"
	build_sketches "$FQBN" "$GITHUB_WORKSPACE/examples"
	if [ ! "$OS_IS_WINDOWS" == "1" ]; then
		echo "Installing ESPAsyncWebServer ..."
		git clone https://github.com/me-no-dev/ESPAsyncWebServer "$ARDUINO_USR_PATH/libraries/ESPAsyncWebServer" > /dev/null 2>&1

		echo "Installing ArduinoJson ..."
		git clone https://github.com/bblanchon/ArduinoJson "$ARDUINO_USR_PATH/libraries/ArduinoJson" > /dev/null 2>&1

		build_sketches "$FQBN" "$ARDUINO_USR_PATH/libraries/ESPAsyncWebServer/examples"
	fi
else
	# PlatformIO Test
	source ./.github/scripts/install-platformio.sh

	echo "Installing ESPAsyncTCP ..."
	python -m platformio lib --storage-dir "$GITHUB_WORKSPACE" install

	BOARD="esp12e"
	build_pio_sketches "$BOARD" "$GITHUB_WORKSPACE/examples"

	if [[ "$OSTYPE" != "cygwin" ]] && [[ "$OSTYPE" != "msys" ]] && [[ "$OSTYPE" != "win32" ]]; then
		echo "Installing ESPAsyncWebServer ..."
		python -m platformio lib -g install https://github.com/me-no-dev/ESPAsyncWebServer.git > /dev/null 2>&1
		git clone https://github.com/me-no-dev/ESPAsyncWebServer "$HOME/ESPAsyncWebServer" > /dev/null 2>&1

		echo "Installing ArduinoJson ..."
		python -m platformio lib -g install https://github.com/bblanchon/ArduinoJson.git > /dev/null 2>&1

		build_pio_sketches "$BOARD" "$HOME/ESPAsyncWebServer/examples"
	fi
fi
