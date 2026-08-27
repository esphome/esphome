"""The pch script must reach PlatformIO builds only; the native ESP-IDF
toolchain has its own pch flow in build_gen/espidf.py."""

from contextlib import suppress
from unittest.mock import patch

import pytest

from esphome.components import esp32
from esphome.const import Toolchain
from esphome.core import CORE


@pytest.mark.parametrize(
    ("toolchain", "copied"),
    [(Toolchain.PLATFORMIO, True), (Toolchain.ESP_IDF, False)],
)
def test_copy_files_gates_pch_script_on_toolchain(
    toolchain: Toolchain, copied: bool
) -> None:
    CORE.toolchain = toolchain
    with (
        patch.object(esp32, "_write_sdkconfig"),
        patch.object(esp32, "_write_idf_component_yml"),
        patch.object(esp32, "copy_pch_script") as copy_script,
        patch.object(esp32, "write_file_if_changed"),
        patch.object(esp32, "get_partition_csv"),
        patch.dict(CORE.data, {}, clear=False),
        # Later copy_files steps need a full build dir; the pch gate runs first
        suppress(Exception),
    ):
        esp32.copy_files()
    assert copy_script.called is copied
