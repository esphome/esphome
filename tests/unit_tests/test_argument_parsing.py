import pytest

from esphome.__main__ import parse_args


def test_socket_option_is_mutually_exclusive_with_systemd_socket():
    with pytest.raises(SystemExit) as stop_ex:
        parse_args(
            [
                "esphome",
                "dashboard",
                "--systemd-socket",
                "--socket",
                "/var/run/esphome.sock",
                "/var/lib/esphome",
            ]
        )


def test_open_ui_option_is_mutually_exclusive_with_socket():
    with pytest.raises(SystemExit) as stop_ex:
        parse_args(
            [
                "esphome",
                "dashboard",
                "--open-ui",
                "--socket",
                "/var/run/esphome.sock",
                "/var/lib/esphome",
            ]
        )


def test_open_ui_option_is_mutually_exclusive_with_systemd_socket():
    with pytest.raises(SystemExit) as stop_ex:
        parse_args(
            [
                "esphome",
                "dashboard",
                "--open-ui",
                "--systemd-socket",
                "/var/lib/esphome",
            ]
        )


def test_socket_option_with_nonempty_path():
    args = parse_args(
        [
            "esphome",
            "dashboard",
            "--socket",
            "/run/esphome/esphome.sock",
            "/var/lib/esphome",
        ]
    )
    assert args.socket is "/run/esphome/esphome.sock"
