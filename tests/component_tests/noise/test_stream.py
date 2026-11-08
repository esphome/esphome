"""Tests for the noise stream helpers: the link key schema, the checks and codegen."""

from __future__ import annotations

from typing import Any

import pytest

from esphome import config_validation as cv
from esphome.components import noise
from esphome.const import (
    CONF_API,
    CONF_ENCRYPTION,
    CONF_ESPHOME,
    CONF_ID,
    CONF_KEY,
    CONF_OTA,
    CONF_PLATFORM,
    PlatformFramework,
)
from esphome.core import CORE, ID
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable

LINK_KEY = "MDEyMzQ1Njc4OWFiY2RlZjAxMjM0NTY3ODlhYmNkZWY="
OTHER_KEY = "ZmVkY2JhOTg3NjU0MzIxMGZlZGNiYTk4NzY1NDMyMTA="
ZERO_KEY = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA="

check = noise.final_validate_stream_key("my_link")


def _check(
    set_core_config: SetCoreConfigCallable,
    link: ConfigType,
    full_config: dict[str, Any] | None = None,
) -> ConfigType:
    set_core_config(PlatformFramework.ESP32_IDF, full_config=full_config or {})
    return check(link)


def _link(key: str) -> ConfigType:
    return {CONF_ID: ID("link"), CONF_ENCRYPTION: {CONF_KEY: key}}


def test_the_link_key_is_accepted() -> None:
    assert noise.STREAM_ENCRYPTION_SCHEMA({CONF_KEY: LINK_KEY})[CONF_KEY] == LINK_KEY


@pytest.mark.parametrize(
    ("config", "message"),
    [
        ({}, "required key not provided"),
        # The key cannot be left out, so the error does not suggest it
        (
            {CONF_KEY: ZERO_KEY},
            "all-zeros key is reserved and provides no protection; generate",
        ),
        ({CONF_KEY: "not base64!"}, "Invalid key format"),
    ],
    ids=["missing", "all-zeros", "not-base64"],
)
def test_a_bad_link_key_is_rejected(config: ConfigType, message: str) -> None:
    with pytest.raises(cv.Invalid, match=message):
        noise.STREAM_ENCRYPTION_SCHEMA(config)


def test_only_an_encrypted_entry_loads_noise() -> None:
    auto_load = noise.stream_auto_load("my_link", ["socket"])
    CORE.raw_config = {"my_link": [{CONF_ID: "plain"}]}
    assert auto_load() == ["socket"]
    # A single entry is not a list before validation
    CORE.raw_config = {"my_link": {CONF_ENCRYPTION: {CONF_KEY: LINK_KEY}}}
    assert auto_load() == ["socket", "noise"]
    # Tooling asks without a config
    CORE.raw_config = None
    assert auto_load() == ["socket", "noise"]


def _ota(key: str) -> list[ConfigType]:
    return [{CONF_PLATFORM: CONF_ESPHOME, CONF_ENCRYPTION: {CONF_KEY: key}}]


@pytest.mark.parametrize(
    ("link", "full_config"),
    [
        ({CONF_ID: ID("link")}, {CONF_API: {CONF_ENCRYPTION: {CONF_KEY: LINK_KEY}}}),
        (
            _link(LINK_KEY),
            {
                CONF_API: {CONF_ENCRYPTION: {CONF_KEY: OTHER_KEY}},
                CONF_OTA: _ota(OTHER_KEY),
            },
        ),
        # A key provisioned at runtime cannot be compared
        (_link(LINK_KEY), {CONF_API: {CONF_ENCRYPTION: {}}}),
    ],
    ids=["no-encryption", "different-keys", "runtime-api-key"],
)
def test_the_link_key_check_accepts(
    set_core_config: SetCoreConfigCallable, link: ConfigType, full_config: ConfigType
) -> None:
    _check(set_core_config, link, full_config)


@pytest.mark.parametrize(
    ("full_config", "owner"),
    [
        ({CONF_API: {CONF_ENCRYPTION: {CONF_KEY: LINK_KEY}}}, CONF_API),
        ({CONF_OTA: _ota(LINK_KEY)}, CONF_OTA),
    ],
    ids=["api", "ota"],
)
def test_the_link_key_check_rejects_a_shared_key(
    set_core_config: SetCoreConfigCallable, full_config: ConfigType, owner: str
) -> None:
    with pytest.raises(
        cv.Invalid,
        match=rf"'my_link' encryption key must differ from the '{owner}' encryption "
        r"key; the peer device holds this key",
    ) as err:
        _check(set_core_config, _link(LINK_KEY), full_config)
    assert err.value.path == [CONF_ENCRYPTION, CONF_KEY]


@pytest.mark.parametrize(
    ("initiator", "spare"), [(True, False), (False, True)], ids=["client", "server"]
)
def test_new_stream_compiles_the_stream(
    set_core_config: SetCoreConfigCallable, initiator: bool, spare: bool
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    # The stream source compiles only on request
    assert noise.FILTER_SOURCE_FILES() == ["noise_stream.cpp"]
    stream = noise.new_stream(
        ID("link", is_declaration=True), LINK_KEY, initiator, "my_link"
    )
    assert noise.FILTER_SOURCE_FILES() == []
    defines = {define.name for define in CORE.defines}
    assert "USE_NOISE_STREAM" in defines
    assert ("USE_NOISE_SPARE_EPHEMERAL" in defines) is spare
    assert ("USE_NOISE_STREAM_INITIATOR" in defines) is initiator
    assert ("USE_NOISE_STREAM_RESPONDER" in defines) is not initiator
    assert str(stream) == "link_noise"
