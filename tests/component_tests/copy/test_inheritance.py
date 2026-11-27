"""A component that inherits metadata follows the source's own inheritance chain.

A source that inherits the metadata itself (another ``copy`` sensor) may not have been
final-validated yet when the consumer is listed first, so the consumer must resolve the
source's chain instead of reading only the properties the source already carries.
"""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.config import read_config
from esphome.const import (
    CONF_ACCURACY_DECIMALS,
    CONF_ICON,
    CONF_ID,
    CONF_UNIT_OF_MEASUREMENT,
)
from esphome.core import CORE


def _consumer_metadata(
    component_config_path: Callable[[str], Path], yaml_file: str
) -> dict:
    CORE.config_path = component_config_path(yaml_file)
    config = read_config({})
    consumer = next(
        entry for entry in config["sensor"] if entry[CONF_ID].id == "consumer"
    )
    return {
        CONF_UNIT_OF_MEASUREMENT: consumer.get(CONF_UNIT_OF_MEASUREMENT),
        CONF_ACCURACY_DECIMALS: consumer.get(CONF_ACCURACY_DECIMALS),
        CONF_ICON: consumer.get(CONF_ICON),
    }


@pytest.mark.parametrize(
    "yaml_file",
    ["consumer_before_copy.yaml", "copy_before_consumer.yaml"],
    ids=["consumer-first", "copy-first"],
)
def test_copy_inherits_the_sources_metadata_regardless_of_order(
    component_config_path: Callable[[str], Path], yaml_file: str
) -> None:
    # `middle` inherits W / 1 / mdi:flash from `source`; the consumer must see the same.
    assert _consumer_metadata(component_config_path, yaml_file) == {
        CONF_UNIT_OF_MEASUREMENT: "W",
        CONF_ACCURACY_DECIMALS: 1,
        CONF_ICON: "mdi:flash",
    }
