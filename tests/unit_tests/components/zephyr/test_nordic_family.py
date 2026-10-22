"""nordic_family.to_code: random number source for variants whose RNG is on a second core."""

from __future__ import annotations

import pytest

from esphome.components.zephyr import zephyr_data
from esphome.components.zephyr.const import KEY_PRJ_CONF, KEY_ZEPHYR
from esphome.components.zephyr.variants import nordic_family
from esphome.core import CORE
from tests.unit_tests.components.zephyr.zephyr_state import empty_zephyr_data

_WARNING = "insecure software one is used"


def _run(framework_type: str, config: dict) -> None:
    CORE.data[KEY_ZEPHYR] = empty_zephyr_data("NRF5340", framework_type=framework_type)
    CORE.config = config
    nordic_family.to_code({})


@pytest.mark.parametrize(
    ("framework_type", "config", "kconfig", "warns"),
    [
        ("zephyr", {"network": {}}, "TEST_RANDOM_GENERATOR", True),
        ("zephyr", {}, "TEST_RANDOM_GENERATOR", False),
        ("ncs", {"network": {}}, "ENTROPY_GENERATOR", False),
    ],
)
def test_nrf5340_random_source(
    caplog: pytest.LogCaptureFixture,
    framework_type: str,
    config: dict,
    kconfig: str,
    warns: bool,
) -> None:
    _run(framework_type, config)

    prj_conf = zephyr_data()[KEY_PRJ_CONF][""]
    assert {k for k in prj_conf if k.endswith("_GENERATOR")} == {f"CONFIG_{kconfig}"}
    assert (_WARNING in caplog.text) is warns
