"""Tests for the mqtt TLS validation rules and the port default."""

import pytest

from esphome import config_validation as cv
from esphome.components.const import CONF_VERIFY_SSL
from esphome.components.mqtt import CONF_TLS, validate_config
from esphome.const import (
    CONF_BROKER,
    CONF_CERTIFICATE_AUTHORITY,
    CONF_CLIENT_CERTIFICATE,
    CONF_PORT,
    CONF_SKIP_CERT_CN_CHECK,
    CONF_TOPIC_PREFIX,
)
from esphome.types import ConfigType

BASE: ConfigType = {CONF_BROKER: "broker.local", CONF_TOPIC_PREFIX: "node"}


def _validate(**extra: object) -> ConfigType:
    return validate_config({**BASE, **extra})


def test_plain_defaults_to_port_1883() -> None:
    out = _validate()
    assert out[CONF_PORT] == 1883
    assert CONF_TLS not in out


@pytest.mark.parametrize(
    "extra",
    [
        {CONF_TLS: True},
        {CONF_CERTIFICATE_AUTHORITY: "cert"},
        {CONF_VERIFY_SSL: False},
        {CONF_CLIENT_CERTIFICATE: "cert"},
    ],
)
def test_tls_defaults_to_port_8883(extra: ConfigType) -> None:
    out = _validate(**extra)
    assert out[CONF_TLS] is True
    assert out[CONF_PORT] == 8883


def test_explicit_port_is_kept() -> None:
    assert _validate(**{CONF_TLS: True, CONF_PORT: 1234})[CONF_PORT] == 1234


def test_certificate_authority_with_verify_ssl_false_rejected() -> None:
    with pytest.raises(cv.Invalid, match="'verify_ssl' cannot be false"):
        _validate(**{CONF_CERTIFICATE_AUTHORITY: "cert", CONF_VERIFY_SSL: False})


def test_skip_cert_cn_check_with_verify_ssl_false_rejected() -> None:
    with pytest.raises(cv.Invalid, match="'skip_cert_cn_check' has no effect"):
        _validate(**{CONF_SKIP_CERT_CN_CHECK: True, CONF_VERIFY_SSL: False})


@pytest.mark.parametrize(
    "extra",
    [
        {CONF_CERTIFICATE_AUTHORITY: "cert"},
        {CONF_VERIFY_SSL: True},
        {CONF_CLIENT_CERTIFICATE: "cert"},
    ],
)
def test_tls_false_with_certificate_options_rejected(extra: ConfigType) -> None:
    with pytest.raises(cv.Invalid, match="'tls' cannot be false"):
        _validate(**{CONF_TLS: False, **extra})
