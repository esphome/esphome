#!/bin/bash
# build_coord.sh DIR: build the esp-zigbee-sdk v2.0.5 examples/all_device_types_app copy in DIR for esp32h2 with
# ESPHome's own IDF (see the recipe for the sdkconfig and component pins). env: IDF_PATH (the cached framework),
# IDF_PYTHON_ENV_PATH (its penv), IDF_TOOLS_PATH, and PATH with the riscv32-esp-elf toolchain and the penv bin first.
export IDF_PYTHON_CHECK_CONSTRAINTS=no
cd "${1:?coordinator project dir}" || exit 1
[ -f sdkconfig ] || python "${IDF_PATH:?set IDF_PATH}/tools/idf.py" set-target esp32h2
python "$IDF_PATH/tools/idf.py" build
