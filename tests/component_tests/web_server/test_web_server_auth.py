"""Tests for web_server authentication codegen."""

from collections.abc import Callable

import pytest

from esphome.core import CORE

_DEFAULT_CHANGE_WARNING = "default will change to 'digest' in ESPHome 2027.1.0"


def _has_define(name: str) -> bool:
    return any(d.name == name for d in CORE.defines)


def test_web_server_auth_default_is_basic_with_deprecation_warning(
    generate_main: Callable[[str], str],
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Auth without an explicit type builds Basic and warns about the upcoming default change."""
    main_cpp = generate_main(
        "tests/component_tests/web_server/web_server_auth_default.yaml"
    )

    assert '->set_auth_username("admin");' in main_cpp
    assert '->set_auth_password("password");' in main_cpp
    assert _has_define("USE_WEBSERVER_AUTH")
    assert not _has_define("USE_WEBSERVER_AUTH_DIGEST")
    assert _DEFAULT_CHANGE_WARNING in caplog.text


def test_web_server_auth_explicit_basic_no_warning(
    generate_main: Callable[[str], str],
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Auth type basic builds Basic and does not warn."""
    generate_main("tests/component_tests/web_server/web_server_auth_basic.yaml")

    assert _has_define("USE_WEBSERVER_AUTH")
    assert not _has_define("USE_WEBSERVER_AUTH_DIGEST")
    assert _DEFAULT_CHANGE_WARNING not in caplog.text


def test_web_server_auth_explicit_digest(
    generate_main: Callable[[str], str],
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Auth type digest builds Digest and does not warn."""
    generate_main("tests/component_tests/web_server/web_server_auth_digest.yaml")

    assert _has_define("USE_WEBSERVER_AUTH")
    assert _has_define("USE_WEBSERVER_AUTH_DIGEST")
    assert _DEFAULT_CHANGE_WARNING not in caplog.text


def test_web_server_without_auth(
    generate_main: Callable[[str], str],
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Without an auth block, no auth is compiled in and no warning is emitted."""
    generate_main("tests/component_tests/web_server/web_server_no_auth.yaml")

    assert not _has_define("USE_WEBSERVER_AUTH")
    assert not _has_define("USE_WEBSERVER_AUTH_DIGEST")
    assert _DEFAULT_CHANGE_WARNING not in caplog.text
