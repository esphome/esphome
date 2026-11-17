"""The nrf52 sdk-nrf native backend: idedata from the app's compile database."""

from pathlib import Path
from unittest.mock import patch

import pytest

from esphome.components.nrf52 import toolchain
import esphome.config_validation as cv
from esphome.const import KEY_CORE, KEY_FRAMEWORK_VERSION
from esphome.core import CORE


@pytest.mark.parametrize(
    ("version", "app_dir"),
    [(cv.Version(2, 9, 2), ("zephyr",)), (cv.Version(2, 9, 1), ())],
)
def test_get_idedata_reads_the_app_compile_database(
    tmp_path: Path, version: cv.Version, app_dir: tuple[str, ...]
) -> None:
    """The compile database sits in the app build dir of the SDK's layout."""
    CORE.config_path = tmp_path / "test.yaml"
    CORE.build_path = tmp_path / "build"
    CORE.name = "livingroom"
    with (
        patch.dict(CORE.data, {KEY_CORE: {KEY_FRAMEWORK_VERSION: version}}),
        patch(
            "esphome.build_helpers.idedata.load_or_build_idedata",
            return_value={"prog_path": "x"},
        ) as load,
    ):
        assert toolchain.get_idedata() == {"prog_path": "x"}

    app = CORE.relative_pioenvs_path("livingroom", *app_dir)
    compile_commands, elf, _cache = load.call_args.args
    assert compile_commands == app / "compile_commands.json"
    assert elf == app / "zephyr" / "zephyr.elf"
