"""The derivative inherits its source's metadata through the source's own inheritance chain.

A source that inherits the metadata itself (a ``copy`` sensor) may not have been
final-validated yet when the derivative is listed first, so the derivative must resolve the
source's chain instead of reading only the properties the source already carries.
"""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.config import read_config
from esphome.const import CONF_ACCURACY_DECIMALS, CONF_ICON, CONF_UNIT_OF_MEASUREMENT
from esphome.core import CORE


def _derivative_metadata(
    component_config_path: Callable[[str], Path], yaml_file: str
) -> dict:
    CORE.config_path = component_config_path(yaml_file)
    config = read_config({})
    derivative = next(
        entry for entry in config["sensor"] if entry["platform"] == "derivative"
    )
    return {
        CONF_UNIT_OF_MEASUREMENT: derivative.get(CONF_UNIT_OF_MEASUREMENT),
        CONF_ACCURACY_DECIMALS: derivative.get(CONF_ACCURACY_DECIMALS),
        CONF_ICON: derivative.get(CONF_ICON),
    }


@pytest.mark.parametrize(
    "yaml_file",
    ["derivative_before_copy.yaml", "copy_before_derivative.yaml"],
    ids=["derivative-first", "copy-first"],
)
def test_derivative_inherits_the_sources_metadata_regardless_of_order(
    component_config_path: Callable[[str], Path], yaml_file: str
) -> None:
    # `copy` inherits W / 1 / mdi:flash from `power`; the derivative turns W into W/h.
    assert _derivative_metadata(component_config_path, yaml_file) == {
        CONF_UNIT_OF_MEASUREMENT: "W/h",
        CONF_ACCURACY_DECIMALS: 1,
        CONF_ICON: "mdi:flash",
    }
