"""Unit tests for esphome/pins.py."""

from collections.abc import Iterator
import logging
from unittest.mock import AsyncMock, MagicMock, patch

import pytest

from esphome import config_validation as cv, pins
import esphome.codegen as cg
from esphome.config import Config, path_context
from esphome.const import (
    CONF_ADDRESS,
    CONF_ALLOW_OTHER_USES,
    CONF_ID,
    CONF_IGNORE_STRAPPING_WARNING,
    CONF_INVERTED,
    CONF_MODE,
    CONF_NUMBER,
    KEY_CORE,
    KEY_TARGET_PLATFORM,
)
from esphome.core import CORE, ID
from esphome.schema_extractors import SCHEMA_EXTRACT

SomeHub = cg.esphome_ns.class_("SomeHub")


def test_use_id_or_address_with_explicit_id() -> None:
    """An explicit id string still resolves exactly like cv.use_id."""
    validator = pins.use_id_or_address(SomeHub)
    result = validator("my_hub")

    assert isinstance(result, ID)
    assert result.id == "my_hub"
    assert result.type is SomeHub
    assert result.is_declaration is False
    assert result.match_config is None


def test_use_id_or_address_with_omitted_id() -> None:
    """Omitting the id (None) still falls back to the single-instance auto-pick."""
    validator = pins.use_id_or_address(SomeHub)
    result = validator(None)

    assert isinstance(result, ID)
    assert result.id is None
    assert result.type is SomeHub
    assert result.match_config is None


def test_use_id_or_address_with_address_mapping() -> None:
    """A {address: ...} mapping produces an unnamed ID with match_config set."""
    validator = pins.use_id_or_address(SomeHub)
    result = validator({CONF_ADDRESS: 0x21})

    assert isinstance(result, ID)
    assert result.id is None
    assert result.type is SomeHub
    assert result.is_declaration is False
    assert result.match_config == {CONF_ADDRESS: 0x21}


def test_use_id_or_address_with_string_address() -> None:
    """A hex string address is normalized the same way cv.i2c_address does."""
    validator = pins.use_id_or_address(SomeHub)
    result = validator({CONF_ADDRESS: "0x21"})

    assert result.match_config == {CONF_ADDRESS: 0x21}


def test_use_id_or_address_mapping_requires_address_key() -> None:
    """A mapping without the address key is rejected, not silently ignored."""
    validator = pins.use_id_or_address(SomeHub)

    with pytest.raises(cv.Invalid):
        validator({})


def test_use_id_or_address_mapping_rejects_invalid_address() -> None:
    """An out-of-range address is still validated by cv.i2c_address."""
    validator = pins.use_id_or_address(SomeHub)

    with pytest.raises(cv.Invalid):
        validator({CONF_ADDRESS: 0x1FF})


def test_use_id_or_address_custom_address_key() -> None:
    """A custom address_key is honored both in the mapping and match_config."""
    validator = pins.use_id_or_address(SomeHub, address_key="i2c_address")
    result = validator({"i2c_address": 0x10})

    assert result.match_config == {"i2c_address": 0x10}


def test_use_id_or_address_marks_address_match_as_manual() -> None:
    """The resolved id must survive strip_default_ids(), unlike a plain
    omitted-id auto-pick, since the user explicitly gave selection criteria."""
    validator = pins.use_id_or_address(SomeHub)
    result = validator({CONF_ADDRESS: 0x21})

    assert result.is_manual is True


def test_use_id_or_address_schema_extract_passthrough() -> None:
    """Called with the SCHEMA_EXTRACT sentinel (as build_language_schema.py
    does), the wrapper must fall through to cv.use_id's own handling and
    return the hub type, not try to treat it as an address mapping."""
    validator = pins.use_id_or_address(SomeHub)

    assert validator(SCHEMA_EXTRACT) is SomeHub


def test_use_id_or_address_registers_as_use_id_schema(monkeypatch) -> None:
    """The wrapper must be discoverable by build_language_schema.py as a
    `use_id` schema (like a bare cv.use_id), or the generated language schema
    silently drops hub-id completion for every pin schema using it."""
    from esphome import schema_extractors

    monkeypatch.setattr(schema_extractors, "EnableSchemaExtraction", True)
    monkeypatch.setattr(schema_extractors, "hidden_schemas", {})

    validator = pins.use_id_or_address(SomeHub)

    assert schema_extractors.hidden_schemas.get(repr(validator)) == "use_id"


def _mode(input_: bool, output: bool, pullup: bool = False) -> dict[str, bool]:
    return {"input": input_, "output": output, "pullup": pullup}


@pytest.mark.parametrize(("input_", "output"), [(True, False), (False, True)])
def test_input_xor_output_accepts_one(input_: bool, output: bool) -> None:
    mode = _mode(input_, output)
    assert pins.gpio_validate_input_xor_output(mode) is mode


@pytest.mark.parametrize(("input_", "output"), [(True, True), (False, False)])
def test_input_xor_output_rejects_both_or_neither(input_: bool, output: bool) -> None:
    with pytest.raises(cv.Invalid, match="either input or output"):
        pins.gpio_validate_input_xor_output(_mode(input_, output))


def test_input_xor_output_pullup_allows_input_pullup() -> None:
    mode = _mode(True, False, pullup=True)
    assert pins.gpio_validate_input_xor_output_pullup(mode) is mode


def test_input_xor_output_pullup_rejects_output_pullup() -> None:
    with pytest.raises(cv.Invalid, match="Pullup only available with input"):
        pins.gpio_validate_input_xor_output_pullup(_mode(False, True, pullup=True))


def test_input_xor_output_pullup_still_requires_one_mode() -> None:
    with pytest.raises(cv.Invalid, match="either input or output"):
        pins.gpio_validate_input_xor_output_pullup(_mode(True, True))


@pytest.mark.asyncio
@pytest.mark.parametrize("args", [(), ("templ",)])
async def test_new_parented_pin(args: tuple[str, ...]) -> None:
    config = {
        "id": "pin_id",
        "hub": "hub_id",
        "number": 5,
        "inverted": True,
        "mode": {"input": True, "output": False},
    }
    var = MagicMock()
    with (
        patch("esphome.codegen.new_Pvariable", return_value=var) as new_pvariable,
        patch("esphome.codegen.register_parented", new=AsyncMock()) as parented,
        patch("esphome.codegen.add") as add,
    ):
        assert await pins.new_parented_pin(config, "hub", *args) is var
    new_pvariable.assert_called_once_with("pin_id", *args)
    parented.assert_awaited_once_with(var, "hub_id")
    var.set_pin.assert_called_once_with(5)
    var.set_inverted.assert_called_once_with(True)
    var.set_flags.assert_called_once()
    assert add.call_count == 3


@pytest.fixture
def registry() -> pins.PinRegistry:
    reg = pins.PinRegistry()
    reg.register("hub", lambda conf: conf)(lambda conf: conf)
    return reg


@pytest.fixture
def pin_path() -> Iterator[None]:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: "esp32"}
    token = path_context.set(["pin"])
    try:
        yield
    finally:
        path_context.reset(token)


@pytest.mark.usefixtures("pin_path")
def test_registry_validate_tracks_usage_and_get_count(
    registry: pins.PinRegistry,
) -> None:
    assert registry.get_count("hub", "h1", 4) == 0

    registry.validate({"hub": "h1", CONF_NUMBER: 4}, "hub")

    assert registry.get_count("hub", "h1", 4) == 1
    assert registry.get_count("hub", "h1", 5) == 0


@pytest.mark.usefixtures("pin_path")
def test_registry_validate_without_number_is_not_tracked(
    registry: pins.PinRegistry,
) -> None:
    assert registry.validate({"hub": "h1"}, "hub") == {"hub": "h1"}
    assert not registry.pins_used


def test_registry_get_to_code(registry: pins.PinRegistry) -> None:
    to_code = registry["hub"][0]

    assert registry.get_to_code("hub") is to_code
    assert registry.get_to_code({"hub": "h1"}) is to_code


def _final_config(client_id: ID) -> Config:
    config = Config()
    config["hub"] = [{CONF_ID: client_id}]
    config.declare_ids.append((client_id, ["hub", 0, CONF_ID]))
    return config


@pytest.mark.parametrize(
    ("uses", "allow_others", "testing_mode", "error"),
    [
        (2, False, False, "used in multiple places"),
        (2, True, False, None),
        (2, False, True, None),
        (1, False, False, None),
        (1, True, False, "incorrectly sets allow_other_uses"),
        (1, True, True, None),
    ],
)
@pytest.mark.usefixtures("pin_path")
def test_registry_final_validate_reuse_rules(
    registry: pins.PinRegistry,
    monkeypatch: pytest.MonkeyPatch,
    uses: int,
    allow_others: bool,
    testing_mode: bool,
    error: str | None,
) -> None:
    client_id = ID("h1", is_declaration=True)
    extra = {CONF_ALLOW_OTHER_USES: True} if allow_others else {}
    for _ in range(uses):
        registry.validate({"hub": client_id, CONF_NUMBER: 4, **extra}, "hub")
    monkeypatch.setattr(CORE, "testing_mode", testing_mode)
    config = _final_config(client_id)

    registry.final_validate(config)

    if error is None:
        assert not config.errors
    else:
        assert all(error in str(err) for err in config.errors)
        assert config.errors


@pytest.mark.usefixtures("pin_path")
def test_registry_final_validate_runs_hook_with_parent_config(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    hook = MagicMock()
    registry = pins.PinRegistry()
    registry.register("hub", lambda conf: conf, hook)(lambda conf: conf)
    client_id = ID("h1", is_declaration=True)
    pin = registry.validate({"hub": client_id, CONF_NUMBER: 4}, "hub")
    config = _final_config(client_id)

    registry.final_validate(config)

    hook.assert_called_once_with(pin, config["hub"][0])
    assert not config.errors


@pytest.mark.parametrize(
    ("mode", "expected"),
    [
        ("input", {"input": True}),
        ("INPUT_PULLUP", {"input": True, "pullup": True}),
        ("output_open_drain", {"output": True, "open_drain": True}),
    ],
)
def test_set_mode_parses_string_modes(mode: str, expected: dict[str, bool]) -> None:
    assert pins._set_mode({CONF_NUMBER: 1, CONF_MODE: mode}, {}) == {
        CONF_NUMBER: 1,
        CONF_MODE: expected,
    }


def test_set_mode_rejects_unknown_string_mode() -> None:
    with pytest.raises(cv.Invalid, match="Unknown pin mode bogus"):
        pins._set_mode({CONF_MODE: "bogus"}, {})


def test_set_mode_keeps_dict_mode_and_applies_default() -> None:
    explicit = {CONF_MODE: {"input": True}}
    assert pins._set_mode(explicit, {"output": True}) is explicit
    assert pins._set_mode({}, {"output": True}) == {CONF_MODE: {"output": True}}


def test_gpio_flags_expr() -> None:
    assert str(pins.gpio_flags_expr({"input": False})) == str(cg.gpio_Flags.FLAG_NONE)
    flags = str(pins.gpio_flags_expr({"input": True, "pullup": True}))
    assert "FLAG_INPUT" in flags
    assert "FLAG_PULLUP" in flags


def test_gpio_validate_modes() -> None:
    mode = {"input": True, "output": False}
    assert pins.gpio_validate_modes(mode) is mode
    with pytest.raises(cv.Invalid, match="Mode must be input or output"):
        pins.gpio_validate_modes({"input": False, "output": False})


INPUT_MODE = {"input": True}


def test_gpio_base_schema_rejects_placeholder_number() -> None:
    schema = pins.gpio_base_schema(SomeHub, cv.int_)

    with pytest.raises(cv.Invalid, match="placeholder 'GPIOX'"):
        schema({CONF_NUMBER: "GPIOX", CONF_MODE: INPUT_MODE})
    assert schema({CONF_NUMBER: 3, CONF_MODE: INPUT_MODE})[CONF_NUMBER] == 3


@pytest.mark.parametrize("invertible", [True, False])
def test_gpio_base_schema_invertible(invertible: bool) -> None:
    schema = pins.gpio_base_schema(SomeHub, cv.int_, invertible=invertible)

    assert (
        CONF_INVERTED in schema({CONF_NUMBER: 3, CONF_MODE: INPUT_MODE})
    ) is invertible


def test_check_strapping_pin(caplog: pytest.LogCaptureFixture) -> None:
    logger = logging.getLogger("test_pins")
    with caplog.at_level(logging.WARNING, logger="test_pins"):
        pins.check_strapping_pin({CONF_NUMBER: 2}, {2}, logger)
        pins.check_strapping_pin(
            {CONF_NUMBER: 2, CONF_IGNORE_STRAPPING_WARNING: True}, {2}, logger
        )
    assert caplog.text.count("GPIO2 is a strapping pin") == 1
    with pytest.raises(cv.Invalid, match="not a strapping pin"):
        pins.check_strapping_pin(
            {CONF_NUMBER: 3, CONF_IGNORE_STRAPPING_WARNING: True}, {2}, logger
        )
