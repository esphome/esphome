"""Tests for the noise stream helpers: the key, interval and set key action checks and codegen."""

from __future__ import annotations

from typing import Any

import pytest

from esphome import config_validation as cv
from esphome.components import noise
from esphome.components.const import CONF_RECONNECT_INTERVAL
from esphome.const import (
    CONF_API,
    CONF_ENCRYPTION,
    CONF_ESPHOME,
    CONF_ID,
    CONF_KEY,
    CONF_ON_BOOT,
    CONF_OTA,
    CONF_PLATFORM,
    CONF_PRIORITY,
    CONF_THEN,
    PlatformFramework,
)
from esphome.core import CORE, ID
from esphome.helpers import fnv1_hash
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


def test_the_link_key_may_be_left_out() -> None:
    # It is then set at runtime
    assert noise.STREAM_ENCRYPTION_SCHEMA(None) == {}
    assert noise.STREAM_ENCRYPTION_SCHEMA({CONF_KEY: LINK_KEY})[CONF_KEY] == LINK_KEY
    with pytest.raises(cv.Invalid, match="all-zeros key is reserved") as err:
        noise.STREAM_ENCRYPTION_SCHEMA({CONF_KEY: ZERO_KEY})
    assert "omit the key" in str(err.value)


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


def test_a_link_without_encryption_is_not_checked(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _check(
        set_core_config,
        {CONF_ID: ID("link")},
        {CONF_API: {CONF_ENCRYPTION: {CONF_KEY: LINK_KEY}}},
    )


def test_a_key_differing_from_api_and_ota_is_accepted(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _check(
        set_core_config,
        _link(LINK_KEY),
        {
            CONF_API: {CONF_ENCRYPTION: {CONF_KEY: OTHER_KEY}},
            CONF_OTA: [
                {CONF_PLATFORM: CONF_ESPHOME, CONF_ENCRYPTION: {CONF_KEY: OTHER_KEY}}
            ],
        },
    )


def test_the_api_key_is_rejected(set_core_config: SetCoreConfigCallable) -> None:
    with pytest.raises(
        cv.Invalid,
        match=r"'my_link' encryption key must differ from the 'api' encryption key; "
        r"the peer device holds this key",
    ) as err:
        _check(
            set_core_config,
            _link(LINK_KEY),
            {CONF_API: {CONF_ENCRYPTION: {CONF_KEY: LINK_KEY}}},
        )
    assert err.value.path == [CONF_ENCRYPTION, CONF_KEY]


def test_the_ota_key_is_rejected(set_core_config: SetCoreConfigCallable) -> None:
    with pytest.raises(cv.Invalid, match=r"must differ from the 'ota' encryption key"):
        _check(
            set_core_config,
            _link(LINK_KEY),
            {
                CONF_OTA: [
                    {CONF_PLATFORM: CONF_ESPHOME, CONF_ENCRYPTION: {CONF_KEY: LINK_KEY}}
                ]
            },
        )


def test_a_runtime_api_key_cannot_be_compared(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _check(set_core_config, _link(LINK_KEY), {CONF_API: {CONF_ENCRYPTION: {}}})


def test_encryption_needs_a_reconnect_interval_of_one_loop_pass(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF, full_config={CONF_ESPHOME: {}})
    encrypted = {CONF_ENCRYPTION: {CONF_KEY: LINK_KEY}}
    zero = {CONF_RECONNECT_INTERVAL: cv.positive_time_period_milliseconds("0s")}
    with pytest.raises(
        cv.Invalid,
        match=r"With encryption, reconnect_interval must last at least one main loop "
        r"pass \(loop_interval of 16ms\)",
    ) as err:
        noise.final_validate_reconnect_interval({**zero, **encrypted})
    assert err.value.path == [CONF_RECONNECT_INTERVAL]
    noise.final_validate_reconnect_interval(zero)
    one_pass = {CONF_RECONNECT_INTERVAL: cv.positive_time_period_milliseconds("16ms")}
    noise.final_validate_reconnect_interval({**one_pass, **encrypted})


@pytest.mark.parametrize(
    ("initiator", "spare"), [(True, False), (False, True)], ids=["client", "server"]
)
def test_new_stream_compiles_the_stream(
    set_core_config: SetCoreConfigCallable, initiator: bool, spare: bool
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    # The stream source compiles only on request
    assert noise.FILTER_SOURCE_FILES() == ["noise_stream.cpp"]
    stream = noise.new_stream(ID("link", is_declaration=True), LINK_KEY, initiator)
    assert noise.FILTER_SOURCE_FILES() == []
    defines = {define.name for define in CORE.defines}
    assert "USE_NOISE_STREAM" in defines
    assert ("USE_NOISE_SPARE_EPHEMERAL" in defines) is spare
    assert str(stream) == "link_noise"


def _set_key_action(entry_id: str, priority: float = 100.0) -> ConfigType:
    # The action as it sits in a validated config, deep inside an automation
    action = {"my_link.set_encryption_key": {CONF_ID: ID(entry_id), CONF_KEY: LINK_KEY}}
    boot = {CONF_PRIORITY: priority, CONF_THEN: [action]}
    return {CONF_ESPHOME: {CONF_ON_BOOT: [boot]}}


def test_the_set_key_action_needs_a_runtime_key(
    set_core_config: SetCoreConfigCallable,
) -> None:
    runtime = {CONF_ID: ID("link"), CONF_ENCRYPTION: {}}
    assert _check(set_core_config, runtime, _set_key_action("link")) == runtime
    for entry in ({CONF_ID: ID("link")}, _link(LINK_KEY)):
        with pytest.raises(
            cv.Invalid,
            match=r"'my_link.set_encryption_key' needs 'encryption:' without a 'key' "
            r"on 'link'",
        ):
            _check(set_core_config, entry, _set_key_action("link"))
    # An action on another entry is that entry's business
    _check(set_core_config, _link(LINK_KEY), _set_key_action("other"))


def test_an_on_boot_action_runs_after_the_key_is_loaded(
    set_core_config: SetCoreConfigCallable,
) -> None:
    runtime = {CONF_ID: ID("link"), CONF_ENCRYPTION: {}}
    with pytest.raises(
        cv.Invalid,
        match=r"'my_link.set_encryption_key' in 'on_boot' needs a 'priority' below 200",
    ):
        _check(set_core_config, runtime, _set_key_action("link", 600.0))
    assert _check(set_core_config, runtime, _set_key_action("link", 100.0)) == runtime


def test_an_api_action_without_api_encryption_warns(
    set_core_config: SetCoreConfigCallable, caplog: pytest.LogCaptureFixture
) -> None:
    runtime = {CONF_ID: ID("link"), CONF_ENCRYPTION: {}}
    action = {"my_link.set_encryption_key": {CONF_ID: ID("link"), CONF_KEY: LINK_KEY}}
    api_action = {"actions": [{"then": [action]}]}
    _check(set_core_config, runtime, {CONF_API: api_action})
    assert "crosses the network in plain text" in caplog.text
    # An api with encryption, provisioned or not, and an action outside the api do not warn
    caplog.clear()
    _check(set_core_config, runtime, {CONF_API: {**api_action, CONF_ENCRYPTION: {}}})
    _check(set_core_config, runtime, {**_set_key_action("link"), CONF_API: {}})
    assert "plain text" not in caplog.text


def test_new_stream_without_a_key_takes_it_at_runtime(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    stream = noise.new_stream(ID("link", is_declaration=True), None, True)
    defines = {define.name for define in CORE.defines}
    assert {"USE_NOISE_STREAM", "USE_NOISE_STREAM_RUNTIME_KEY"} <= defines
    assert str(stream) == "link_noise"
    # The preference hash is per entry
    key_hash = fnv1_hash("noise_stream_key:link")
    assert f"noise::NoiseStream({key_hash}UL, true)" in CORE.cpp_main_section
    # Without api and ota there is no key to keep apart
    CORE.config = {}
    CORE.flush_tasks()
    assert "set_api_context" not in CORE.cpp_main_section
    assert "set_ota_key" not in CORE.cpp_main_section


def test_a_key_in_yaml_compiles_no_runtime_key(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    noise.new_stream(ID("link", is_declaration=True), LINK_KEY, True)
    defines = {define.name for define in CORE.defines}
    assert "USE_NOISE_STREAM_RUNTIME_KEY" not in defines
