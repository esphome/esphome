"""Tests for the audio_http media source's ca_certificate_path inheritance."""

from pathlib import Path

from esphome.components.audio_http.media_source import _inherit_ca_certificate_path
from esphome.core import ID
import esphome.final_validate as fv
from esphome.types import ConfigType


def _source_config() -> ConfigType:
    return {"id": ID("audio_http_source")}


def _run(full_config: dict, config: ConfigType) -> ConfigType:
    token = fv.full_config.set(full_config)
    try:
        return _inherit_ca_certificate_path(config)
    finally:
        fv.full_config.reset(token)


def test_explicit_option_wins() -> None:
    """A source with its own ca_certificate_path is left untouched."""
    config = {"id": ID("s"), "ca_certificate_path": Path("own.pem")}
    result = _run(
        {"http_request": {"verify_ssl": True, "ca_certificate_path": Path("hr.pem")}},
        config,
    )
    assert result["ca_certificate_path"] == Path("own.pem")


def test_inherited_from_http_request() -> None:
    """Without its own option, the source picks up http_request's CA path."""
    result = _run(
        {
            "http_request": {
                "verify_ssl": True,
                "ca_certificate_path": Path("hr.pem"),
            }
        },
        _source_config(),
    )
    assert result["ca_certificate_path"] == Path("hr.pem")


def test_no_http_request_component() -> None:
    """No http_request block means no inheritance."""
    result = _run({}, _source_config())
    assert "ca_certificate_path" not in result


def test_http_request_without_ca() -> None:
    """An http_request block without ca_certificate_path means no inheritance."""
    result = _run({"http_request": {"verify_ssl": True}}, _source_config())
    assert "ca_certificate_path" not in result


def test_not_inherited_when_verify_ssl_disabled() -> None:
    """verify_ssl: false disables the CA for http_request itself, so the
    inheritance must skip it too instead of silently pinning playback."""
    result = _run(
        {
            "http_request": {
                "verify_ssl": False,
                "ca_certificate_path": Path("hr.pem"),
            }
        },
        _source_config(),
    )
    assert "ca_certificate_path" not in result
