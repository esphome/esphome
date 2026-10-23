"""Tests for esphome.final_validate helpers."""

from collections.abc import Iterator
from contextlib import contextmanager

import pytest

from esphome import config_validation as cv, final_validate as fv
from esphome.config import Config
from esphome.const import CONF_CHANNEL, CONF_ID, CONF_PLATFORM
from esphome.core import ID


def _config() -> tuple[Config, ID, dict]:
    hub_id = ID("my_hub", is_declaration=True)
    hub = {CONF_ID: hub_id, "mode": "fast"}
    config = Config()
    config["other"] = {"x": 1}
    config["my_comp"] = [{"y": 2}, hub]
    config.declare_ids.append((hub_id, ["my_comp", 1, CONF_ID]))
    return config, hub_id, hub


def test_returns_the_declaring_block_from_the_full_config() -> None:
    config, hub_id, hub = _config()
    token = fv.full_config.set(config)
    try:
        assert fv.get_declaring_config(hub_id) is hub
    finally:
        fv.full_config.reset(token)


def test_uses_the_given_config() -> None:
    config, hub_id, hub = _config()
    assert fv.get_declaring_config(ID("my_hub"), config) is hub


def test_unknown_id_raises_key_error() -> None:
    config, _, _ = _config()
    with pytest.raises(KeyError):
        fv.get_declaring_config(ID("missing"), config)


def test_non_dict_parent_raises_type_error() -> None:
    config, _, _ = _config()
    list_id = ID("in_list", is_declaration=True)
    config["listed"] = [list_id]
    config.declare_ids.append((list_id, ["listed", 0]))
    with pytest.raises(TypeError):
        fv.get_declaring_config(ID("in_list"), config)


@pytest.mark.parametrize("pre_built", [False, True])
def test_id_declaration_match_schema_validates_declaration(pre_built: bool) -> None:
    config, hub_id, _ = _config()
    schema = {cv.Required("mode"): cv.string}
    if pre_built:
        schema = cv.Schema(schema, extra=cv.ALLOW_EXTRA)
    validator = fv.id_declaration_match_schema(schema)
    token = fv.full_config.set(config)
    try:
        result = validator(ID("my_hub"))
    finally:
        fv.full_config.reset(token)

    assert result["mode"] == "fast"


def test_id_declaration_match_schema_rejects_mismatch() -> None:
    config, _, _ = _config()
    validator = fv.id_declaration_match_schema({cv.Required("mode"): cv.one_of("slow")})
    token = fv.full_config.set(config)
    try:
        with pytest.raises(cv.Invalid):
            validator(ID("my_hub"))
    finally:
        fv.full_config.reset(token)


def _switches(*entries: tuple[str, str, int]) -> list[dict]:
    return [
        {CONF_PLATFORM: platform, "hub_id": ID(hub), CONF_CHANNEL: channel}
        for platform, hub, channel in entries
    ]


@contextmanager
def _with_switches(switches: list[dict]) -> Iterator[None]:
    config = Config()
    config["switch"] = switches
    token = fv.full_config.set(config)
    try:
        yield
    finally:
        fv.full_config.reset(token)


def test_platform_entries_filters_by_platform_and_match() -> None:
    switches = _switches(("relay", "a", 1), ("relay", "b", 1), ("gpio", "a", 1))
    with _with_switches(switches):
        assert fv.platform_entries(domain="switch", platform="relay") == switches[:2]
        assert (
            fv.platform_entries(
                domain="switch", platform="relay", match={"hub_id": ID("a")}
            )
            == switches[:1]
        )
        assert (
            fv.platform_entries(
                domain="switch", platform="relay", match={CONF_CHANNEL: 2}
            )
            == []
        )
        assert fv.platform_entries(domain="light", platform="relay") == []


@pytest.mark.parametrize(
    ("entries", "fails"),
    [
        ((("relay", "a", 1), ("relay", "a", 2), ("relay", "b", 1)), False),
        ((("relay", "a", 1), ("gpio", "a", 1)), False),
        ((("relay", "a", 1), ("relay", "a", 1)), True),
    ],
)
def test_unique_platform_key(
    entries: tuple[tuple[str, str, int], ...], fails: bool
) -> None:
    switches = _switches(*entries)
    validator = fv.unique_platform_key(
        domain="switch",
        platform="relay",
        keys=("hub_id", CONF_CHANNEL),
        message="Channel {channel} of {0} is used twice",
    )
    with _with_switches(switches):
        if fails:
            with pytest.raises(cv.Invalid, match="Channel 1 of a is used twice") as err:
                validator(switches[0])
            assert err.value.path == [CONF_CHANNEL]
        else:
            assert validator(switches[0]) is switches[0]


@pytest.mark.parametrize(("count", "fails"), [(1, False), (2, True)])
def test_unique_platform_key_without_keys(count: int, fails: bool) -> None:
    switches = _switches(*[("relay", "a", n) for n in range(count)])
    validator = fv.unique_platform_key(
        domain="switch", platform="relay", message="Only one relay"
    )
    with _with_switches(switches):
        if fails:
            with pytest.raises(cv.Invalid, match="Only one relay") as err:
                validator(switches[0])
            assert err.value.path == []
        else:
            validator(switches[0])


def test_unique_platform_key_with_missing_optional_key() -> None:
    switches = [{CONF_PLATFORM: "relay", "hub_id": ID("a")} for _ in range(2)]
    validator = fv.unique_platform_key(
        domain="switch",
        platform="relay",
        keys=("hub_id", CONF_CHANNEL),
        message="Channel {channel} of {0} is used twice",
    )
    with (
        _with_switches(switches),
        pytest.raises(cv.Invalid, match="Channel None of a is used twice"),
    ):
        validator(switches[0])
