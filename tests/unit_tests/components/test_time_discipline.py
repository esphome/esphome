"""Validation of the opt-in clock discipline configuration."""

from unittest.mock import patch

import pytest

from esphome.components.time import _final_validate_discipline
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.types import ConfigType


@pytest.mark.parametrize(
    ("sources", "variant", "native_idf", "stack", "error"),
    [
        ([{}], "ESP8266", False, 8192, None),
        ([{"discipline": {}}], "ESP32C6", True, 32768, None),
        ([{"discipline": {}}, {}], "ESP32S3", True, 32768, None),
        ([{"discipline": {}}], "ESP32", True, 32768, None),
        ([{"discipline": {}}], "ESP32C3", True, 32768, "native ESP-IDF"),
        ([{"discipline": {}}], "ESP32C6", False, 32768, "native ESP-IDF"),
        ([{"discipline": {}}], "ESP32C6", True, 8192, "loop_task_stack_size"),
        ([{"discipline": {}}, {"discipline": {}}], "ESP32C6", True, 32768, "one shared"),
        ([{"discipline": {}}, {}, {}], "ESP32C6", True, 32768, "at most two"),
    ],
)
def test_final_validation(
    sources: list[ConfigType],
    variant: str,
    native_idf: bool,
    stack: int,
    error: str | None,
) -> None:
    """Keep legacy configs valid and reject unsupported discipline setups early."""
    config = {
        "time": sources,
        "esp32": {"framework": {"advanced": {"loop_task_stack_size": stack}}},
    }
    token = fv.full_config.set(config)
    try:
        with (
            patch("esphome.components.time.CORE") as core,
            patch("esphome.components.esp32.get_esp32_variant", return_value=variant),
        ):
            core.using_toolchain_esp_idf = native_idf
            if error is None:
                _final_validate_discipline(sources)
            else:
                with pytest.raises(cv.Invalid, match=error):
                    _final_validate_discipline(sources)
    finally:
        fv.full_config.reset(token)
