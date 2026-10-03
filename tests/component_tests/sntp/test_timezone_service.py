"""Tests for fetching the SNTP timezone from a service at runtime."""

from collections.abc import Callable
import logging
from pathlib import Path

import pytest

from esphome import config_validation as cv
from esphome.components.sntp.time import (
    DEFAULT_SERVICE_URL,
    SET_TIMEZONE_SCHEMA,
    TIMEZONE_SERVICE_SCHEMA,
    ZONE_IP,
    validate_zone,
)
from esphome.config import read_config
from esphome.core import CORE, EsphomeError

CONFIG_DIR = Path(__file__).parent / "config"


@pytest.mark.parametrize(
    ("value", "expected"),
    [
        ("ip", ZONE_IP),
        ("IP", ZONE_IP),
        ("Europe/London", "Europe/London"),
        ("America/Argentina/Buenos_Aires", "America/Argentina/Buenos_Aires"),
        ("UTC", "UTC"),
    ],
)
def test_validate_zone_accepts(value: str, expected: str) -> None:
    assert validate_zone(value) == expected


@pytest.mark.parametrize("value", ["Nowhere/Special", "EST5EDT,M3.2.0,M11.1.0", ""])
def test_validate_zone_rejects(value: str) -> None:
    with pytest.raises(cv.Invalid):
        validate_zone(value)


def test_timezone_service_codegen(
    generate_main: Callable[[str | Path], str],
) -> None:
    main_cpp = generate_main(CONFIG_DIR / "timezone_service.yaml")
    defines = {d.name for d in CORE.defines}

    assert "USE_SNTP_TIMEZONE_SERVICE" in defines
    assert "USE_TIME_TIMEZONE" in defines
    assert (
        f'set_timezone_service(http_request_httprequestidf_id, "{DEFAULT_SERVICE_URL}", '
        '"Europe/London", 1800000);' in main_cpp
    )
    assert "set_timezone_location" not in main_cpp
    # The rules for the configured zone apply until the service answers
    assert "time::set_global_tz(tz);" in main_cpp
    assert 'set_zone(ESPHOME_F("Australia/Sydney"));' in main_cpp
    # The action can also take a location
    assert "sntp_settimezoneaction_id_2->set_latitude(" in main_cpp
    assert "return 40.71f;" in main_cpp
    assert "return -74.01f;" in main_cpp

    # The timezone abbreviation text sensor is wired to the sntp instance
    assert "set_timezone_abbreviation_text_sensor(" in main_cpp


@pytest.mark.parametrize(
    ("yaml_file", "user"),
    [
        ("timezone_abbreviation_without_service.yaml", "The sntp text sensor"),
        ("timezone_action_without_service.yaml", "The time.sntp.set_timezone action"),
        ("timezone_action_fixed_zone.yaml", "The time.sntp.set_timezone action"),
    ],
)
def test_needs_timezone_service_is_rejected_by_validation(
    yaml_file: str, user: str, capsys: pytest.CaptureFixture[str]
) -> None:
    CORE.config_path = CONFIG_DIR / yaml_file

    assert read_config({}) is None

    captured = capsys.readouterr()
    assert f"{user} needs the 'timezone' option to be set to a service" in (
        captured.out + captured.err
    )


def test_timezone_service_ip(
    generate_main: Callable[[str | Path], str],
) -> None:
    main_cpp = generate_main(CONFIG_DIR / "timezone_service_ip.yaml")

    # Rules rarely change, so the default is to fetch them once a day
    assert '"ip", 86400000);' in main_cpp
    assert "USE_SNTP_TIMEZONE_SERVICE" in {d.name for d in CORE.defines}


def test_timezone_service_location(
    generate_main: Callable[[str | Path], str],
) -> None:
    main_cpp = generate_main(CONFIG_DIR / "timezone_service_location.yaml")

    assert (
        "set_timezone_service(http_request_httprequestidf_id, "
        '"https://timezone.example.com/v1/timezone", "", 86400000);' in main_cpp
    )
    assert "sntp_time->set_timezone_location(51.5f, -0.12f);" in main_cpp


@pytest.mark.parametrize(
    "config",
    [
        {},
        {"zone": "ip", "latitude": 1, "longitude": 2},
        {"latitude": 1},
        {"longitude": 2},
        {"latitude": 91, "longitude": 0},
        {"zone": "ip", "service": "ftp://example.com"},
    ],
)
def test_timezone_service_schema_rejects(config: dict) -> None:
    with pytest.raises(cv.Invalid):
        # Everything except the platform check, which needs a target platform
        cv.All(*TIMEZONE_SERVICE_SCHEMA.validators[:-1])(config)


@pytest.mark.parametrize(
    "config",
    [
        {},
        {"zone": "ip", "latitude": 1, "longitude": 2},
        {"latitude": 1},
        {"longitude": 2},
        {"latitude": 91, "longitude": 0},
        {"latitude": 0, "longitude": -181},
        {"zone": "Nowhere/Special"},
    ],
)
def test_set_timezone_schema_rejects(config: dict) -> None:
    with pytest.raises(cv.Invalid):
        SET_TIMEZONE_SCHEMA({"id": "sntp_time", **config})


def test_timezone_service_ip_warns_when_build_zone_is_unknown(
    generate_main: Callable[[str | Path], str],
    monkeypatch: pytest.MonkeyPatch,
    caplog: pytest.LogCaptureFixture,
) -> None:
    def detect_tz() -> str:
        raise EsphomeError("Could not automatically determine timezone")

    monkeypatch.setattr("esphome.components.time.detect_tz", detect_tz)
    with caplog.at_level(logging.WARNING):
        main_cpp = generate_main(CONFIG_DIR / "timezone_service_ip.yaml")

    assert "Could not find the time zone of this computer" in caplog.text
    assert "USE_SNTP_TIMEZONE_SERVICE" in {d.name for d in CORE.defines}
    assert "set_timezone_service(" in main_cpp
