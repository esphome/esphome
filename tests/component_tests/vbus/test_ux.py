"""Short controller configurations preserve explicit settings and generic TX."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome import yaml_util
from esphome.components import vbus
import esphome.config_validation as cv
from esphome.core import CORE


@pytest.mark.parametrize("explicit", [False, True])
def test_controller_address_defaults_preserve_explicit_addresses(
    explicit: bool,
) -> None:
    source = {"transactions": {"controller": {"model": "deltasol_cs_plus"}}}
    if explicit:
        source.update(destination=8721, source=32)
    result = vbus.TRANSMIT_SCHEMA(source)
    assert result["destination"] == 8721
    assert result["source"] == 32


def test_generic_tx_still_requires_an_explicit_destination() -> None:
    with pytest.raises(cv.Invalid, match="Set destination for generic TX"):
        vbus.TRANSMIT_SCHEMA({})
    assert vbus.TRANSMIT_SCHEMA({"destination": 16913})["destination"] == 16913


@pytest.mark.parametrize(
    "value,armed",
    [
        ("direct", False),
        ("armed", True),
        ({}, True),
        ({"require_write_arm": False}, False),
    ],
)
def test_write_modes_normalize_to_the_existing_configuration(
    value: object, armed: bool
) -> None:
    result = vbus.CONTROLLER_SCHEMA({"model": "deltasol_cs_plus", "writes": value})
    assert result["writes"]["require_write_arm"] is armed


@pytest.mark.parametrize("value", [True, False, "automatic"])
def test_write_modes_do_not_enable_writes_implicitly(value: object) -> None:
    with pytest.raises(cv.Invalid, match="writes must be"):
        vbus.CONTROLLER_SCHEMA({"model": "deltasol_cs_plus", "writes": value})


def test_short_and_nested_controller_generate_the_same_operations(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    tmp_path: Path,
) -> None:
    path = component_config_path("controller.yaml")
    before = generate_main(path)
    source = yaml_util.load_yaml(path)
    hub = source["vbus"]
    hub["controller"] = hub.pop("transmit")["transactions"]["controller"]
    shortened = tmp_path / "short.yaml"
    shortened.write_text(yaml_util.dump(source))
    CORE.reset()
    after = generate_main(shortened)
    for call in [
        "set_destination(8721)",
        "set_source(32)",
        "vbus::ResolDeltaSolCSPlus(controller_bus,",
        "add_on_snapshot_callback(",
        "start_scan()",
        "suspend_polling()",
        "resume_polling()",
    ]:
        assert before.count(call) == after.count(call) > 0


def test_controller_cannot_be_declared_twice() -> None:
    with pytest.raises(cv.Invalid, match="not both"):
        vbus.CONFIG_SCHEMA(
            {
                "controller": {"model": "deltasol_cs_plus"},
                "transmit": {
                    "transactions": {"controller": {"model": "deltasol_cs_plus"}}
                },
            }
        )
