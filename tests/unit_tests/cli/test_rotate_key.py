"""Tests for ``esphome rotate-key``."""

from __future__ import annotations

from collections.abc import Generator
from dataclasses import dataclass
from pathlib import Path
from typing import Any
from unittest.mock import ANY, Mock, patch

import pytest

from esphome import yaml_util
from esphome.cli.rotate_key import PRECHECK_ADVICE, command_rotate_key
from esphome.config import do_substitution_pass
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
from esphome.espota2 import ProbeResult
from esphome.upload_targets import PortType
from esphome.util import ESPHOME_COMMAND
from esphome.yaml_edit import RestoreError
from tests.unit_tests.test_ota_key_edit import API_YAML, NEW_KEY, OLD_KEY, SECRET_YAML
from tests.unit_tests.test_yaml_edit import _setup


@dataclass
class MockArgs:
    device: list[str] | None = None
    prompt_new_key: bool = False
    yes: bool = False
    substitution: list[list[str]] | None = None
    toolchain: Toolchain | None = None
    dashboard: bool = False


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


OTA_CONF = {
    CONF_PLATFORM: CONF_ESPHOME,
    CONF_PORT: 3232,
    CONF_ENCRYPTION: {CONF_KEY: OLD_KEY},
}


@pytest.fixture
def env(tmp_path: Path) -> Generator[dict[str, Mock]]:
    """A config with an inline api key, loaded the way read_config does so
    the key node carries its source range, and every network step mocked."""
    _setup(tmp_path, API_YAML)
    CORE.name = "test"
    CORE.build_path = tmp_path / ".esphome" / "build" / "test"
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: PLATFORM_ESP32}
    CORE.config = {
        CONF_API: {CONF_ENCRYPTION: {CONF_KEY: OLD_KEY}},
        CONF_OTA: [OTA_CONF],
    }
    with (
        patch(
            "esphome.__main__.choose_upload_log_host", return_value=["dev.local"]
        ) as choose,
        patch("esphome.__main__._resolve_network_devices", return_value=["dev.local"]),
        patch("esphome.cli.rotate_key.get_port_type", return_value=PortType.NETWORK),
        patch(
            "esphome.espota2.probe_ota_key", return_value=ProbeResult.ACCEPTED
        ) as probe,
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
            kwargs["on_upload_start"]()
            return upload.return_value

        upload.side_effect = upload_after_connecting
        yield {
            "probe": probe,
            "compile": compile_,
            "upload": upload,
            "confirm": confirm,
            "stdin": stdin,
            "choose": choose,
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
        on_upload_start=ANY,
    )
    out = capfd.readouterr().out
    assert NEW_KEY in out
    assert "SUCCESS" in out


def test_uploads_the_platform_ota_image(env: dict[str, Mock]) -> None:
    """The upload sends what the platform's ota_image hook picks, as installs do."""
    image = Path("firmware.ota.uf2")
    with patch("esphome.__main__._app_ota_image", return_value=image) as pick:
        assert command_rotate_key(MockArgs(), CORE.config) == 0
    pick.assert_called_once()
    assert env["upload"].call_args.args[3] == image


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


@pytest.mark.parametrize(
    ("yaml_text", "files", "message"),
    [
        (
            API_YAML,
            {"other.yaml": "packages:\n  base: !include test.yaml\n"},
            "test.yaml is also used by other.yaml",
        ),
        (
            # Without a secrets.yaml beside it the device may fall back to this one
            SECRET_YAML,
            {
                "secrets.yaml": f"device_key: {OLD_KEY}\n",
                "sub/device.yaml": "api:\n  encryption:\n    key: !secret device_key\n",
            },
            "secret 'device_key' is also used by sub/device.yaml",
        ),
        (
            API_YAML,
            {"broken.yaml": "key: !secret device_key\nbad: @x\n"},
            "Could not check whether other configurations use this key: Could not scan",
        ),
        (
            f'api:\n  encryption:\n    key: "{OLD_KEY}"\n\nota:\n  - !include ota.yaml\n',
            {
                "ota.yaml": "platform: esphome\nencryption:\n",
                "other.yaml": "ota:\n  - !include ota.yaml\n",
            },
            "ota.yaml is also used by other.yaml",
        ),
        (
            SECRET_YAML,
            {
                "secrets.yaml": f"device_key: {OLD_KEY}\ndevice_key_old: x\n",
                "other.yaml": "wifi:\n  password: !secret device_key_old\n",
            },
            "secret 'device_key_old' is also used by other.yaml",
        ),
    ],
    ids=["includer", "secrets_fallback", "unchecked", "old_key_file", "old_secret"],
)
def test_refuses_a_key_that_is_not_unique(
    env: dict[str, Mock],
    capfd: pytest.CaptureFixture[str],
    yaml_text: str,
    files: dict[str, str],
    message: str,
) -> None:
    """Anything another configuration uses or may use stops the rotation
    before any question, device choice, probe or write."""
    CORE.config_path.write_text(yaml_text, encoding="utf-8")
    for name, text in files.items():
        (CORE.config_dir / name).parent.mkdir(exist_ok=True)
        (CORE.config_dir / name).write_text(text, encoding="utf-8")
    CORE.raw_config = do_substitution_pass(yaml_util.load_yaml(CORE.config_path), None)
    before = {p: p.read_bytes() for p in CORE.config_dir.rglob("*.yaml")}
    assert command_rotate_key(MockArgs(yes=True), CORE.config) == 1
    out = capfd.readouterr().out
    assert message in out
    assert "only a key unique to this device can be rotated, change it by hand" in out
    assert {p: p.read_bytes() for p in CORE.config_dir.rglob("*.yaml")} == before
    env["confirm"].assert_not_called()
    env["choose"].assert_not_called()
    env["probe"].assert_not_called()


def test_uses_the_snapshot_taken_before_validation(env: dict[str, Mock]) -> None:
    """Validation rewrites the raw config; the snapshot keeps the source lines."""

    class Validated(dict):
        user_config = CORE.raw_config

    CORE.raw_config = {}
    assert command_rotate_key(MockArgs(yes=True), Validated(CORE.config)) == 0
    assert NEW_KEY in CORE.config_path.read_text()


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


@pytest.mark.parametrize(
    "key",
    # The last one spells OLD_KEY's bytes with other padding bits
    ["not-base64", OLD_KEY, OLD_KEY[:-2] + "9="],
    ids=["invalid", "same", "same_bytes"],
)
def test_rejects_bad_new_key(env: dict[str, Mock], key: str) -> None:
    with patch("esphome.cli.rotate_key.read_secret_line", return_value=key):
        assert command_rotate_key(MockArgs(prompt_new_key=True), CORE.config) == 1
    assert CORE.config_path.read_text() == API_YAML
    env["probe"].assert_not_called()


def test_precheck_fails_writes_nothing(
    env: dict[str, Mock], capfd: pytest.CaptureFixture[str]
) -> None:
    env["probe"].return_value = ProbeResult.REJECTED
    assert command_rotate_key(MockArgs(), CORE.config) == 1
    assert CORE.config_path.read_text() == API_YAML
    env["compile"].assert_not_called()
    assert "runs a different key than the yaml" in capfd.readouterr().out
    for found in (
        ProbeResult.UNREACHABLE,
        ProbeResult.NOT_OFFERED,
        ProbeResult.DEVICE_ERROR,
    ):
        env["probe"].return_value = found
        assert command_rotate_key(MockArgs(), CORE.config) == 1
        assert PRECHECK_ADVICE[found] in capfd.readouterr().out


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
    built = [
        image,
        image.with_name("firmware.factory.bin"),
        image.with_name("firmware.elf"),
        image.with_name("firmware.bin.signed"),
    ]
    for path in built:
        path.write_bytes(b"built with the new key")
    assert command_rotate_key(MockArgs(), CORE.config) == 1
    assert CORE.config_path.read_text() == API_YAML
    assert env["probe"].call_count == 1
    out = capfd.readouterr().out
    assert ("Interrupted before the upload" in out) is (step == "interrupt")
    # The images may hold the new key, so no upload can install them
    assert not any(path.exists() for path in built)
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


def test_images_are_dropped_when_the_restore_raises_anything(
    env: dict[str, Mock], capfd: pytest.CaptureFixture[str]
) -> None:
    env["compile"].return_value = 1
    CORE.firmware_bin.parent.mkdir(parents=True)
    CORE.firmware_bin.write_bytes(b"")
    with (
        patch("esphome.cli.rotate_key.restore_files", side_effect=SystemExit(3)),
        pytest.raises(SystemExit),
    ):
        command_rotate_key(MockArgs(), CORE.config)
    assert not CORE.firmware_bin.exists()
    out = capfd.readouterr().out
    assert OLD_KEY in out and NEW_KEY in out


def test_a_restore_exit_shows_its_report(
    env: dict[str, Mock], capfd: pytest.CaptureFixture[str]
) -> None:
    env["compile"].return_value = 1
    err = SystemExit(3)
    err.add_note("Could not restore x")
    with (
        patch("esphome.cli.rotate_key.restore_files", side_effect=err),
        pytest.raises(SystemExit),
    ):
        command_rotate_key(MockArgs(), CORE.config)
    out = capfd.readouterr().out
    assert "Could not restore x" in out
    assert OLD_KEY in out and NEW_KEY in out


@pytest.mark.parametrize("failed_rollback", [True, False])
def test_an_exit_while_writing_shows_the_keys_only_when_the_rollback_failed(
    env: dict[str, Mock], capfd: pytest.CaptureFixture[str], failed_rollback: bool
) -> None:
    err = SystemExit(3)
    if failed_rollback:
        err.add_note("Could not restore x")
    with (
        patch("esphome.cli.rotate_key.apply_line_edits", side_effect=err),
        pytest.raises(SystemExit),
    ):
        command_rotate_key(MockArgs(), CORE.config)
    out = capfd.readouterr().out
    assert ("Could not restore x" in out) is failed_rollback
    assert (NEW_KEY in out) is failed_rollback


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
    env["probe"].return_value = ProbeResult.UNREACHABLE
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
    """An upload that fails before the image is sent cannot have committed."""
    env["upload"].side_effect = None
    env["upload"].return_value = (1, None)
    assert command_rotate_key(MockArgs(), CORE.config) == 1
    assert CORE.config_path.read_text() == API_YAML
    out = capfd.readouterr().out
    assert "failed before the image was sent" in out
    assert "put back" not in out
    assert "Restored the previous key" in out


def test_a_successful_upload_without_the_callback_keeps_the_new_key(
    env: dict[str, Mock], capfd: pytest.CaptureFixture[str]
) -> None:
    """A finished upload is never rolled back, even if the callback did not run."""
    env["upload"].side_effect = None
    assert command_rotate_key(MockArgs(), CORE.config) == 0
    assert NEW_KEY in CORE.config_path.read_text()
    assert "SUCCESS" in capfd.readouterr().out


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


@pytest.mark.parametrize(
    "outcome", ["unreachable", "rejected", "not_offered", "device_error", "interrupted"]
)
def test_keeps_the_new_key_after_the_upload(
    env: dict[str, Mock], capfd: pytest.CaptureFixture[str], outcome: str
) -> None:
    """Once the upload went through the device most likely runs the new key
    and old_key covers the other case, so the files stay and the key is
    printed."""
    env["probe"].side_effect = [
        ProbeResult.ACCEPTED,
        {
            "interrupted": KeyboardInterrupt,
            "unreachable": ProbeResult.UNREACHABLE,
            "rejected": ProbeResult.REJECTED,
            "not_offered": ProbeResult.NOT_OFFERED,
            "device_error": ProbeResult.DEVICE_ERROR,
        }[outcome],
    ]
    assert command_rotate_key(MockArgs(), CORE.config) == 1
    assert NEW_KEY in CORE.config_path.read_text()
    assert f'old_key: "{OLD_KEY}"' in CORE.config_path.read_text()
    out = capfd.readouterr().out
    assert NEW_KEY in out
    assert ("Interrupted" in out) is (outcome == "interrupted")
    assert ("still answers with the previous key" in out) is (outcome == "rejected")
    assert ("did not come back" in out) is (outcome == "unreachable")
    assert ("came back without encryption" in out) is (outcome == "not_offered")
    assert ("answered with an error" in out) is (outcome == "device_error")


def test_child_compile_gets_global_options_first(
    env: dict[str, Mock],
) -> None:
    """The compile parser is strict, so -s and --toolchain precede it."""
    args = MockArgs(
        substitution=[["name", "kitchen"]],
        toolchain=Toolchain.PLATFORMIO,
        dashboard=True,
    )
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


@pytest.mark.parametrize("helper", ["rotation_edits", "apply_line_edits"])
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
