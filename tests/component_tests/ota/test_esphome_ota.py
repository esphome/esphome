"""Tests for the esphome OTA platform final_validate logic."""

from __future__ import annotations

import logging
from typing import Any

import pytest

from esphome import config_validation as cv
from esphome.components.esphome.ota import (
    AUTO_LOAD,
    FILTER_SOURCE_FILES,
    _validate_no_password_with_encryption,
    ota_esphome_final_validate,
)
from esphome.const import (
    CONF_API,
    CONF_ENCRYPTION,
    CONF_ESPHOME,
    CONF_ID,
    CONF_KEY,
    CONF_OTA,
    CONF_PASSWORD,
    CONF_PLATFORM,
    CONF_PORT,
    CONF_VERSION,
)
from esphome.core import CORE, ID
import esphome.final_validate as fv


def _make_ota_config(port: int = 3232, **kwargs: Any) -> dict[str, Any]:
    config: dict[str, Any] = {
        CONF_PLATFORM: CONF_ESPHOME,
        CONF_ID: ID(f"ota_esphome_{port}", is_manual=False),
        CONF_VERSION: 2,
        CONF_PORT: port,
    }
    config.update(kwargs)
    return config


def test_single_esphome_ota_instance_accepted() -> None:
    """A single ESPHome OTA config passes final_validate untouched."""
    full_conf = {CONF_OTA: [_make_ota_config(port=3232)]}
    token = fv.full_config.set(full_conf)
    try:
        ota_esphome_final_validate({})
        updated = fv.full_config.get()
        assert len(updated[CONF_OTA]) == 1
        assert updated[CONF_OTA][0][CONF_PORT] == 3232
    finally:
        fv.full_config.reset(token)


def test_same_port_configs_merge(caplog: pytest.LogCaptureFixture) -> None:
    """Two ESPHome OTA configs on the same port merge into one instance."""
    full_conf = {
        CONF_OTA: [
            _make_ota_config(port=3232, **{CONF_PASSWORD: "pw"}),
            _make_ota_config(port=3232),
        ]
    }
    token = fv.full_config.set(full_conf)
    try:
        with caplog.at_level(logging.WARNING):
            ota_esphome_final_validate({})
        updated = fv.full_config.get()
        assert len(updated[CONF_OTA]) == 1
        assert updated[CONF_OTA][0][CONF_PORT] == 3232
        assert any("Found and merged" in record.message for record in caplog.records), (
            "Expected merge warning not found in log"
        )
    finally:
        fv.full_config.reset(token)


def test_multiple_ports_rejected() -> None:
    """Two ESPHome OTA configs on different ports raise cv.Invalid."""
    full_conf = {
        CONF_OTA: [
            _make_ota_config(port=3232),
            _make_ota_config(port=3233),
        ]
    }
    token = fv.full_config.set(full_conf)
    try:
        with pytest.raises(
            cv.Invalid,
            match=r"Only a single port is supported for 'ota' 'platform: esphome'",
        ):
            ota_esphome_final_validate({})
    finally:
        fv.full_config.reset(token)


def test_non_esphome_ota_unaffected() -> None:
    """Non-esphome OTA platforms are not subject to the single-instance rule."""
    full_conf = {
        CONF_OTA: [
            _make_ota_config(port=3232),
            {CONF_PLATFORM: "web_server", CONF_ID: ID("ota_ws", is_manual=False)},
            {CONF_PLATFORM: "http_request", CONF_ID: ID("ota_hr", is_manual=False)},
        ]
    }
    token = fv.full_config.set(full_conf)
    try:
        ota_esphome_final_validate({})
        updated = fv.full_config.get()
        assert len(updated[CONF_OTA]) == 3
    finally:
        fv.full_config.reset(token)


API_KEY = "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8="
OTHER_KEY = "AQIDBAUGBwgJCgsMDQ4PEBESExQVFhcYGRobHB0eHyA="
ZEROS_KEY = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA="


def test_encryption_key_inherited_from_api() -> None:
    """A bare encryption block resolves to the api encryption key."""
    full_conf = {
        CONF_API: {CONF_ENCRYPTION: {CONF_KEY: API_KEY}},
        CONF_OTA: [_make_ota_config(port=3232, **{CONF_ENCRYPTION: {}})],
    }
    token = fv.full_config.set(full_conf)
    try:
        ota_esphome_final_validate({})
        updated = fv.full_config.get()
        assert updated[CONF_OTA][0][CONF_ENCRYPTION][CONF_KEY] == API_KEY
    finally:
        fv.full_config.reset(token)


def test_encryption_explicit_key_matching_api_accepted() -> None:
    """An explicit ota key equal to the api key validates."""
    full_conf = {
        CONF_API: {CONF_ENCRYPTION: {CONF_KEY: API_KEY}},
        CONF_OTA: [
            _make_ota_config(port=3232, **{CONF_ENCRYPTION: {CONF_KEY: API_KEY}})
        ],
    }
    token = fv.full_config.set(full_conf)
    try:
        ota_esphome_final_validate({})
        updated = fv.full_config.get()
        assert updated[CONF_OTA][0][CONF_ENCRYPTION][CONF_KEY] == API_KEY
    finally:
        fv.full_config.reset(token)


def test_encryption_key_differing_from_api_rejected() -> None:
    """There is one key per device; an ota key differing from the api key raises."""
    full_conf = {
        CONF_API: {CONF_ENCRYPTION: {CONF_KEY: API_KEY}},
        CONF_OTA: [
            _make_ota_config(port=3232, **{CONF_ENCRYPTION: {CONF_KEY: OTHER_KEY}})
        ],
    }
    token = fv.full_config.set(full_conf)
    try:
        with pytest.raises(cv.Invalid, match="must match the 'api' encryption key"):
            ota_esphome_final_validate({})
    finally:
        fv.full_config.reset(token)


def test_encryption_explicit_key_without_api_encryption_accepted() -> None:
    """An explicit ota key with a plaintext api has nothing to match; it stands."""
    full_conf = {
        CONF_API: {},
        CONF_OTA: [
            _make_ota_config(port=3232, **{CONF_ENCRYPTION: {CONF_KEY: OTHER_KEY}})
        ],
    }
    token = fv.full_config.set(full_conf)
    try:
        ota_esphome_final_validate({})
        updated = fv.full_config.get()
        assert updated[CONF_OTA][0][CONF_ENCRYPTION][CONF_KEY] == OTHER_KEY
    finally:
        fv.full_config.reset(token)


def test_encryption_without_any_key_rejected() -> None:
    """A bare encryption block with no api key to inherit raises."""
    full_conf = {
        CONF_API: {},
        CONF_OTA: [_make_ota_config(port=3232, **{CONF_ENCRYPTION: {}})],
    }
    token = fv.full_config.set(full_conf)
    try:
        with pytest.raises(cv.Invalid, match="no 'api' encryption key to inherit"):
            ota_esphome_final_validate({})
    finally:
        fv.full_config.reset(token)


def test_encryption_explicit_all_zeros_key_rejected() -> None:
    """The all-zeros key is the provisioning sentinel; the device would treat
    it as no PSK and accept plaintext, so it must fail validation."""
    full_conf = {
        CONF_OTA: [
            _make_ota_config(port=3232, **{CONF_ENCRYPTION: {CONF_KEY: ZEROS_KEY}})
        ],
    }
    token = fv.full_config.set(full_conf)
    try:
        with pytest.raises(cv.Invalid, match="all-zeros key is reserved"):
            ota_esphome_final_validate({})
    finally:
        fv.full_config.reset(token)


def test_encryption_inherited_all_zeros_key_rejected() -> None:
    """An all-zeros api key must not silently disable ota encryption either."""
    full_conf = {
        CONF_API: {CONF_ENCRYPTION: {CONF_KEY: ZEROS_KEY}},
        CONF_OTA: [_make_ota_config(port=3232, **{CONF_ENCRYPTION: {}})],
    }
    token = fv.full_config.set(full_conf)
    try:
        with pytest.raises(cv.Invalid, match="all-zeros key is reserved"):
            ota_esphome_final_validate({})
    finally:
        fv.full_config.reset(token)


def test_encryption_key_mismatch_between_merged_configs_rejected() -> None:
    """Same-port configs with different encryption keys raise."""
    full_conf = {
        CONF_OTA: [
            _make_ota_config(port=3232, **{CONF_ENCRYPTION: {CONF_KEY: API_KEY}}),
            _make_ota_config(port=3232, **{CONF_ENCRYPTION: {CONF_KEY: OTHER_KEY}}),
        ]
    }
    token = fv.full_config.set(full_conf)
    try:
        with pytest.raises(cv.Invalid, match="encryption is inconsistent"):
            ota_esphome_final_validate({})
    finally:
        fv.full_config.reset(token)


@pytest.mark.parametrize("keyed_first", [True, False])
def test_encryption_bare_and_keyed_blocks_merge(keyed_first: bool) -> None:
    """A bare encryption block (package/device split) is compatible with a
    keyed one on the same port; the merge resolves to the keyed result."""
    keyed = _make_ota_config(port=3232, **{CONF_ENCRYPTION: {CONF_KEY: OTHER_KEY}})
    bare = _make_ota_config(port=3232, **{CONF_ENCRYPTION: {}})
    full_conf = {
        CONF_OTA: [keyed, bare] if keyed_first else [bare, keyed],
    }
    token = fv.full_config.set(full_conf)
    try:
        ota_esphome_final_validate({})
        updated = fv.full_config.get()
        assert len(updated[CONF_OTA]) == 1
        assert updated[CONF_OTA][0][CONF_ENCRYPTION][CONF_KEY] == OTHER_KEY
    finally:
        fv.full_config.reset(token)


def test_encryption_runtime_provisioned_api_key_not_inheritable() -> None:
    """A keyless api encryption block provisions its key at runtime; a bare
    ota encryption block cannot inherit it and the message says so."""
    full_conf = {
        CONF_API: {CONF_ENCRYPTION: {}},
        CONF_OTA: [_make_ota_config(port=3232, **{CONF_ENCRYPTION: {}})],
    }
    token = fv.full_config.set(full_conf)
    try:
        with pytest.raises(cv.Invalid, match="provisioned at runtime"):
            ota_esphome_final_validate({})
    finally:
        fv.full_config.reset(token)


def test_encryption_explicit_key_with_runtime_provisioned_api_accepted() -> None:
    """The documented remedy for a runtime-provisioned api key: set an
    explicit ota key."""
    full_conf = {
        CONF_API: {CONF_ENCRYPTION: {}},
        CONF_OTA: [
            _make_ota_config(port=3232, **{CONF_ENCRYPTION: {CONF_KEY: OTHER_KEY}})
        ],
    }
    token = fv.full_config.set(full_conf)
    try:
        ota_esphome_final_validate({})
        updated = fv.full_config.get()
        assert updated[CONF_OTA][0][CONF_ENCRYPTION][CONF_KEY] == OTHER_KEY
    finally:
        fv.full_config.reset(token)


def test_encryption_with_web_server_ota_warns(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """With the web_server component the plaintext /update endpoint is always
    on; the combination validates with a warning."""
    full_conf = {
        "web_server": {},
        CONF_OTA: [
            _make_ota_config(port=3232, **{CONF_ENCRYPTION: {CONF_KEY: OTHER_KEY}}),
            {CONF_PLATFORM: "web_server", CONF_ID: ID("ota_ws", is_manual=False)},
        ],
    }
    token = fv.full_config.set(full_conf)
    try:
        with caplog.at_level(logging.WARNING):
            ota_esphome_final_validate({})
        assert any("plaintext /update" in record.message for record in caplog.records)
    finally:
        fv.full_config.reset(token)


def test_encryption_with_captive_portal_web_server_ota_warns(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """captive_portal auto-loads the web_server ota platform without the
    web_server component; encryption stays usable and only warns, so the
    fallback AP recovery path is not lost."""
    full_conf = {
        "captive_portal": {},
        CONF_OTA: [
            _make_ota_config(port=3232, **{CONF_ENCRYPTION: {CONF_KEY: OTHER_KEY}}),
            {CONF_PLATFORM: "web_server", CONF_ID: ID("ota_ws", is_manual=False)},
        ],
    }
    token = fv.full_config.set(full_conf)
    try:
        with caplog.at_level(logging.WARNING):
            ota_esphome_final_validate({})
        assert any("captive_portal" in record.message for record in caplog.records)
        esphome_conf = next(
            conf
            for conf in fv.full_config.get()[CONF_OTA]
            if conf.get(CONF_PLATFORM) == CONF_ESPHOME
        )
        assert esphome_conf[CONF_ENCRYPTION][CONF_KEY] == OTHER_KEY
    finally:
        fv.full_config.reset(token)


def test_web_server_ota_without_encryption_unaffected() -> None:
    """web_server ota stays valid alongside an unencrypted esphome entry."""
    full_conf = {
        CONF_OTA: [
            _make_ota_config(port=3232),
            {CONF_PLATFORM: "web_server", CONF_ID: ID("ota_ws", is_manual=False)},
        ],
    }
    token = fv.full_config.set(full_conf)
    try:
        ota_esphome_final_validate({})
        assert len(fv.full_config.get()[CONF_OTA]) == 2
    finally:
        fv.full_config.reset(token)


def test_auto_load_pulls_noise_only_for_encryption() -> None:
    """A plain ota entry must never pull noise-c into the build."""
    assert AUTO_LOAD({CONF_PORT: 3232}) == ["sha256", "socket"]
    assert "noise" in AUTO_LOAD({CONF_ENCRYPTION: {}})
    # Tooling probes must get the maximal set: None from dependency
    # resolution, {} from the components-graph platform probe
    assert "noise" in AUTO_LOAD(None)
    assert "noise" in AUTO_LOAD({})


def test_filter_source_files_excludes_noise_without_encryption() -> None:
    """The noise transport source compiles only for encrypted builds."""
    old_config = CORE.config
    try:
        CORE.config = {CONF_OTA: [_make_ota_config(port=3232)]}
        assert FILTER_SOURCE_FILES() == ["ota_esphome_noise.cpp"]
        CORE.config = {
            CONF_OTA: [
                _make_ota_config(port=3232, **{CONF_ENCRYPTION: {CONF_KEY: API_KEY}})
            ]
        }
        assert FILTER_SOURCE_FILES() == []
    finally:
        CORE.config = old_config


def test_password_with_encryption_rejected() -> None:
    """The password and encryption options are mutually exclusive."""
    config = {CONF_PASSWORD: "pw", CONF_ENCRYPTION: {CONF_KEY: API_KEY}}
    with pytest.raises(cv.Invalid, match="cannot be combined"):
        _validate_no_password_with_encryption(config)


def test_password_alone_accepted() -> None:
    """A password without encryption still validates."""
    config = {CONF_PASSWORD: "pw"}
    assert _validate_no_password_with_encryption(config) is config


def test_merged_password_and_encryption_rejected() -> None:
    """A password block and an encryption block merged on one port raise."""
    full_conf = {
        CONF_OTA: [
            _make_ota_config(port=3232, **{CONF_PASSWORD: "pw"}),
            _make_ota_config(port=3232, **{CONF_ENCRYPTION: {CONF_KEY: API_KEY}}),
        ]
    }
    token = fv.full_config.set(full_conf)
    try:
        with pytest.raises(cv.Invalid, match="cannot be combined"):
            ota_esphome_final_validate({})
    finally:
        fv.full_config.reset(token)
