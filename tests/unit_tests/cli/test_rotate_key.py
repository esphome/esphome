"""Tests for ``esphome rotate-key``."""

from __future__ import annotations

from collections.abc import Generator
from dataclasses import dataclass
from pathlib import Path
from typing import Any
from unittest.mock import ANY, Mock, patch

import pytest

from esphome import yaml_util
from esphome.cli.rotate_key import command_rotate_key
from esphome.const import (
    CONF_API,
    CONF_ENCRYPTION,
    CONF_ESPHOME,
    CONF_KEY,
    CONF_OTA,
    CONF_PLATFORM,
    CONF_PORT,
    CONF_WEB_SERVER,
    KEY_CORE,
    KEY_TARGET_PLATFORM,
    PLATFORM_ESP32,
)
from esphome.core import CORE, EsphomeError, Toolchain
from esphome.ota_key_edit import KeyEdit
from esphome.upload_targets import PortType
from esphome.util import ESPHOME_COMMAND
from esphome.yaml_edit import RestoreError


@dataclass
class MockArgs:
    device: list[str] | None = None
    prompt_new_key: bool = False
    yes: bool = False
    substitution: list[list[str]] | None = None
    toolchain: Toolchain | None = None


def _interrupt_after(writes: int) -> Any:
    """A write_keeping_mode that lets ``writes`` calls through, then Ctrl-C."""
    from esphome.yaml_edit import write_keeping_mode

    calls = 0

    def write(*args: Any, **kwargs: Any) -> None:
        nonlocal calls
        calls += 1
        if calls > writes:
            raise KeyboardInterrupt
        write_keeping_mode(*args, **kwargs)

    return write


def setup_core(tmp_path: Path) -> None:
    CORE.reset()
    CORE.config_path = tmp_path / "test.yaml"
    CORE.name = "test"
    CORE.build_path = tmp_path / ".esphome" / "build" / "test"
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: PLATFORM_ESP32}


OLD_KEY = "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8="
NEW_KEY = "AQIDBAUGBwgJCgsMDQ4PEBESExQVFhcYGRobHB0eHyA="
API_YAML = f"""esphome:
  name: test

api:
  encryption:
    key: "{OLD_KEY}"

ota:
  - platform: esphome
    encryption:
"""
OTA_CONF = {
    CONF_PLATFORM: CONF_ESPHOME,
    CONF_PORT: 3232,
    CONF_ENCRYPTION: {CONF_KEY: OLD_KEY},
}


@pytest.fixture
def env(tmp_path: Path) -> Generator[dict[str, Mock]]:
    """A config with an inline api key, loaded the way read_config does so
    the key node carries its source range, and every network step mocked."""
    setup_core(tmp_path)
    CORE.config_path.write_text(API_YAML, encoding="utf-8")
    CORE.raw_config = yaml_util.load_yaml(CORE.config_path)
    CORE.config = {
        CONF_API: {CONF_ENCRYPTION: {CONF_KEY: OLD_KEY}},
        CONF_OTA: [OTA_CONF],
    }
    with (
        patch("esphome.__main__.choose_upload_log_host", return_value=["dev.local"]),
        patch("esphome.__main__._resolve_network_devices", return_value=["dev.local"]),
        patch("esphome.cli.rotate_key.get_port_type", return_value=PortType.NETWORK),
        patch("esphome.espota2.probe_ota_key", return_value=True) as probe,
        patch(
            "esphome.cli.rotate_key.run_external_process", return_value=0
        ) as compile_,
        patch("esphome.espota2.run_ota") as upload,
        patch("esphome.cli.rotate_key.safe_input", return_value="y") as confirm,
        patch("esphome.cli.rotate_key.sys.stdin") as stdin,
        patch(
            "esphome.components.noise.secrets.token_bytes",
            return_value=bytes(range(1, 33)),
        ),
    ):
        stdin.isatty.return_value = True
        # The real upload reports the connection before its result
        upload.return_value = (0, "dev.local")

        def upload_after_connecting(
            *_args: Any, **kwargs: Any
        ) -> tuple[int, str | None]:
            kwargs["on_connect"]()
            return upload.return_value

        upload.side_effect = upload_after_connecting
        yield {
            "probe": probe,
            "compile": compile_,
            "upload": upload,
            "confirm": confirm,
            "stdin": stdin,
        }


def test_success(env: dict[str, Mock], capfd: pytest.CaptureFixture[str]) -> None:
    """Generate, write, compile in a child, upload with the old key, confirm
    with the new one, print it."""
    args = MockArgs()
    assert command_rotate_key(args, CORE.config) == 0

    assert (
        CORE.config_path.read_text()
        == API_YAML.replace(OLD_KEY, NEW_KEY) + f'      old_key: "{OLD_KEY}"\n'
    )
    env["confirm"].assert_called_once()
    precheck, confirm = env["probe"].call_args_list
    assert precheck.args[2] == OLD_KEY
    assert precheck.kwargs["retry_rejected"] is False
    assert confirm.args[2] == NEW_KEY
    compile_args = env["compile"].call_args.args
    assert compile_args[-2:] == ("compile", str(CORE.config_path))
    env["upload"].assert_called_once_with(
        ["dev.local"],
        3232,
        None,
        CORE.firmware_bin,
        noise_psk=OLD_KEY,
        plaintext_fallback=False,
        on_connect=ANY,
    )
    out = capfd.readouterr().out
    assert NEW_KEY in out
    assert "SUCCESS" in out


def test_yes_skips_confirmation(
    env: dict[str, Mock],
) -> None:
    assert command_rotate_key(MockArgs(yes=True), CORE.config) == 0
    env["confirm"].assert_not_called()


def test_declined(env: dict[str, Mock]) -> None:
    env["confirm"].return_value = "n"
    assert command_rotate_key(MockArgs(), CORE.config) == 1
    assert CORE.config_path.read_text() == API_YAML
    env["probe"].assert_not_called()


def test_warns_about_a_shared_secret(
    env: dict[str, Mock], capfd: pytest.CaptureFixture[str]
) -> None:
    """Other configurations using the rewritten secret are named before the
    confirmation."""

    def scan(edits: list[KeyEdit]) -> list[str]:
        edits[0].secret = "device_key"
        edits[0].shared_with = [CORE.config_dir / "b.yaml"]
        return ["c.yaml is a link"]

    with patch("esphome.cli.rotate_key.with_sharers", side_effect=scan):
        env["confirm"].return_value = "n"
        assert command_rotate_key(MockArgs(), CORE.config) == 1
    out = capfd.readouterr().out
    assert "b.yaml (secret 'device_key')" in out
    assert "Could not check every configuration for shared use: c.yaml is a link" in out


def test_no_terminal_needs_yes(
    env: dict[str, Mock],
) -> None:
    env["stdin"].isatty.return_value = False
    assert command_rotate_key(MockArgs(), CORE.config) == 1
    assert CORE.config_path.read_text() == API_YAML


def test_own_ota_key_asks_nothing(
    env: dict[str, Mock],
) -> None:
    """Without an api key nothing changes for Home Assistant."""
    config = {CONF_OTA: [OTA_CONF]}
    assert command_rotate_key(MockArgs(), config) == 0
    env["confirm"].assert_not_called()
    assert NEW_KEY in CORE.config_path.read_text()


def test_prompted_new_key(env: dict[str, Mock]) -> None:
    key = "AgMEBQYHCAkKCwwNDg8QERITFBUWFxgZGhscHR4fICE="
    with patch("esphome.cli.rotate_key.read_secret_line", return_value=key):
        assert command_rotate_key(MockArgs(prompt_new_key=True), CORE.config) == 0
    assert key in CORE.config_path.read_text()
    assert env["probe"].call_args_list[1].args[2] == key


@pytest.mark.parametrize("key", ["not-base64", OLD_KEY], ids=["invalid", "same"])
def test_rejects_bad_new_key(env: dict[str, Mock], key: str) -> None:
    with patch("esphome.cli.rotate_key.read_secret_line", return_value=key):
        assert command_rotate_key(MockArgs(prompt_new_key=True), CORE.config) == 1
    assert CORE.config_path.read_text() == API_YAML
    env["probe"].assert_not_called()


def test_precheck_fails_writes_nothing(
    env: dict[str, Mock], capfd: pytest.CaptureFixture[str]
) -> None:
    env["probe"].return_value = False
    assert command_rotate_key(MockArgs(), CORE.config) == 1
    assert CORE.config_path.read_text() == API_YAML
    env["compile"].assert_not_called()
    assert "did not complete an encrypted handshake" in capfd.readouterr().out


@pytest.mark.parametrize("step", ["compile", "interrupt"])
def test_restores_before_the_upload(
    env: dict[str, Mock], capfd: pytest.CaptureFixture[str], step: str
) -> None:
    if step == "compile":
        env["compile"].return_value = 1
    else:
        env["compile"].side_effect = KeyboardInterrupt
    image = CORE.firmware_bin
    image.parent.mkdir(parents=True)
    factory = image.with_name("firmware.factory.bin")
    for built in (image, factory):
        built.write_bytes(b"built with the new key")
    assert command_rotate_key(MockArgs(), CORE.config) == 1
    assert CORE.config_path.read_text() == API_YAML
    assert env["probe"].call_count == 1
    out = capfd.readouterr().out
    assert ("Interrupted before the upload" in out) is (step == "interrupt")
    # The images may hold the new key, so no upload can install them
    assert not image.exists() and not factory.exists()
    assert "compile again before the next upload" in out
    assert NEW_KEY not in out


def test_a_build_that_cannot_be_removed_shows_the_new_key(
    env: dict[str, Mock], capfd: pytest.CaptureFixture[str]
) -> None:
    """Every image is tried; the ones that stay are all named."""
    env["compile"].return_value = 1
    image = CORE.firmware_bin
    image.mkdir(parents=True)  # unlink fails on a directory
    stuck = image.with_name("firmware.factory.bin")
    stuck.mkdir()
    removable = image.with_name("firmware.ota.bin")
    removable.write_bytes(b"")
    assert command_rotate_key(MockArgs(), CORE.config) == 1
    out = capfd.readouterr().out
    assert str(image) in out and str(stuck) in out
    assert not removable.exists()
    assert OLD_KEY in out and NEW_KEY in out


def test_an_interrupt_during_the_image_cleanup_shows_the_keys(
    env: dict[str, Mock], capfd: pytest.CaptureFixture[str]
) -> None:
    env["compile"].return_value = 1
    CORE.firmware_bin.parent.mkdir(parents=True)
    CORE.firmware_bin.write_bytes(b"")
    with patch("pathlib.Path.glob", side_effect=KeyboardInterrupt):
        assert command_rotate_key(MockArgs(), CORE.config) == 1
    out = capfd.readouterr().out
    assert f"{CORE.firmware_bin.parent}: interrupted" in out
    assert OLD_KEY in out and NEW_KEY in out


def test_a_build_from_before_the_rotation_is_kept(
    env: dict[str, Mock], capfd: pytest.CaptureFixture[str]
) -> None:
    env["probe"].return_value = False
    image = CORE.firmware_bin
    image.parent.mkdir(parents=True)
    image.write_bytes(b"built with the current key")
    assert command_rotate_key(MockArgs(), CORE.config) == 1
    assert image.exists()
    assert "compile again" not in capfd.readouterr().out


@pytest.mark.parametrize("rollback", ["succeeded", "failed"])
def test_an_interrupt_while_writing_reports_a_failed_rollback(
    env: dict[str, Mock], capfd: pytest.CaptureFixture[str], rollback: str
) -> None:
    """apply_line_edits keeps an interrupt an interrupt and notes a rollback
    that failed too; then a file still holds the new key, so both are shown."""
    interrupt = KeyboardInterrupt()
    if rollback == "failed":
        interrupt.add_note("Could not restore test.yaml: disk")
    with patch("esphome.cli.rotate_key.apply_line_edits", side_effect=interrupt):
        assert command_rotate_key(MockArgs(), CORE.config) == 1
    out = capfd.readouterr().out
    assert ("Could not restore test.yaml: disk" in out) is (rollback == "failed")
    assert (NEW_KEY in out) is (rollback == "failed")
    assert ("Interrupted before the upload" in out) is (rollback == "succeeded")


def test_a_second_interrupt_during_the_restore_shows_the_keys(
    env: dict[str, Mock], capfd: pytest.CaptureFixture[str]
) -> None:
    env["compile"].return_value = 1
    with patch("esphome.yaml_edit.write_keeping_mode", side_effect=_interrupt_after(1)):
        assert command_rotate_key(MockArgs(), CORE.config) == 1
    out = capfd.readouterr().out
    assert f"Could not restore {CORE.config_path}: interrupted" in out
    assert OLD_KEY in out and NEW_KEY in out


def test_restores_when_the_device_was_never_reached(
    env: dict[str, Mock], capfd: pytest.CaptureFixture[str]
) -> None:
    """An upload that fails before a connection opens cannot have committed."""
    env["upload"].side_effect = None
    env["upload"].return_value = (1, None)
    assert command_rotate_key(MockArgs(), CORE.config) == 1
    assert CORE.config_path.read_text() == API_YAML
    out = capfd.readouterr().out
    assert "did not reach the device" in out
    assert "put back" not in out
    assert "Restored the previous key" in out


def test_keeps_the_edit_after_a_failed_upload(
    env: dict[str, Mock], capfd: pytest.CaptureFixture[str]
) -> None:
    """An attempted upload may have committed, so the config keeps both keys
    and the new one is printed."""
    env["upload"].return_value = (1, None)
    assert command_rotate_key(MockArgs(), CORE.config) == 1
    text = CORE.config_path.read_text()
    assert NEW_KEY in text and f'old_key: "{OLD_KEY}"' in text
    assert NEW_KEY in capfd.readouterr().out
    assert env["probe"].call_count == 1


def test_shows_both_keys_only_when_a_file_stayed_rewritten(
    env: dict[str, Mock], capfd: pytest.CaptureFixture[str]
) -> None:
    with patch(
        "esphome.cli.rotate_key.apply_line_edits", side_effect=RestoreError("stuck")
    ):
        assert command_rotate_key(MockArgs(), CORE.config) == 1
    out = capfd.readouterr().out
    assert "stuck" in out and OLD_KEY in out and NEW_KEY in out


def test_reports_a_failed_restore(
    env: dict[str, Mock], capfd: pytest.CaptureFixture[str]
) -> None:
    env["compile"].return_value = 1
    with patch(
        "esphome.cli.rotate_key.restore_files",
        side_effect=RestoreError("Could not restore x"),
    ):
        assert command_rotate_key(MockArgs(), CORE.config) == 1
    out = capfd.readouterr().out
    assert "Could not restore x" in out
    assert OLD_KEY in out and NEW_KEY in out


@pytest.mark.parametrize("outcome", ["unconfirmed", "interrupted"])
def test_keeps_the_new_key_after_the_upload(
    env: dict[str, Mock], capfd: pytest.CaptureFixture[str], outcome: str
) -> None:
    """Once the upload went through the device most likely runs the new key
    and old_key covers the other case, so the files stay and the key is
    printed."""
    env["probe"].side_effect = [
        True,
        KeyboardInterrupt if outcome == "interrupted" else False,
    ]
    assert command_rotate_key(MockArgs(), CORE.config) == 1
    assert NEW_KEY in CORE.config_path.read_text()
    assert f'old_key: "{OLD_KEY}"' in CORE.config_path.read_text()
    out = capfd.readouterr().out
    assert NEW_KEY in out
    assert ("Interrupted" in out) is (outcome == "interrupted")


def test_child_compile_gets_global_options_first(
    env: dict[str, Mock],
) -> None:
    """The compile parser is strict, so -s and --toolchain precede it."""
    args = MockArgs(substitution=[["name", "kitchen"]])
    CORE.dashboard = True
    args.toolchain = Toolchain.PLATFORMIO
    assert command_rotate_key(args, CORE.config) == 0
    assert env["compile"].call_args.args[len(ESPHOME_COMMAND) :] == (
        "--dashboard",
        "--toolchain",
        "platformio",
        "-s",
        "name",
        "kitchen",
        "compile",
        str(CORE.config_path),
    )


@pytest.mark.parametrize(
    ("config", "port_type", "match"),
    [
        (
            {CONF_OTA: [{CONF_PLATFORM: CONF_ESPHOME, CONF_PORT: 3232}]},
            "NETWORK",
            "needs 'encryption:'",
        ),
        ({CONF_OTA: [{CONF_PLATFORM: CONF_WEB_SERVER}]}, "NETWORK", "esphome OTA"),
        ({CONF_OTA: [OTA_CONF]}, PortType.SERIAL, "over the air"),
    ],
    ids=["no_encryption", "web_server_only", "serial"],
)
def test_refuses(
    env: dict[str, Mock],
    capfd: pytest.CaptureFixture[str],
    config: dict[str, Any],
    port_type: str,
    match: str,
) -> None:
    with patch("esphome.cli.rotate_key.get_port_type", return_value=port_type):
        assert command_rotate_key(MockArgs(), config) == 1
    assert match in capfd.readouterr().out
    env["probe"].assert_not_called()


def test_refuses_host(env: dict[str, Mock]) -> None:
    CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] = "host"
    assert command_rotate_key(MockArgs(), CORE.config) == 1
    env["probe"].assert_not_called()


@pytest.mark.parametrize("helper", ["locate_key_edits", "apply_line_edits"])
def test_reports_edit_errors(
    env: dict[str, Mock], capfd: pytest.CaptureFixture[str], helper: str
) -> None:
    with patch(f"esphome.cli.rotate_key.{helper}", side_effect=EsphomeError("nope")):
        assert command_rotate_key(MockArgs(), CORE.config) == 1
    out = capfd.readouterr().out
    assert "nope" in out
    # A refusal, or an edit rolled back cleanly, shows no key
    assert NEW_KEY not in out
    assert CORE.config_path.read_text() == API_YAML
