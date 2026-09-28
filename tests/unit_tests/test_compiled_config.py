"""Tests for the validated-config cache used by upload/logs."""

from __future__ import annotations

from contextlib import contextmanager
from ipaddress import IPv4Address, IPv4Network
import json
import os
from pathlib import Path
from typing import Any
from unittest.mock import patch
from uuid import UUID

import pytest

from esphome import const, yaml_util
from esphome.__main__ import run_esphome
from esphome.compiled_config import (
    _LAMBDA_KEY,
    compiled_config_path,
    load_compiled_config,
    save_compiled_config,
    save_compiled_config_and_sidecar,
)
from esphome.const import (
    CONF_API,
    CONF_ESPHOME,
    CONF_NAME,
    KEY_CORE,
    KEY_ESP32,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    KEY_VARIANT,
    Toolchain,
)
from esphome.core import (
    CORE,
    ID,
    EsphomeError,
    HexInt,
    Lambda,
    MACAddress,
    TimePeriodMilliseconds,
)
from esphome.storage_json import StorageJSON
from esphome.util import OrderedDict

_VALIDATED_CONFIG = {
    "esphome": {"name": "lite_test", "friendly_name": "Lite Test Device"},
    "esp32": {"board": "nodemcu-32s"},
    "logger": {"baud_rate": 115200},
    "api": {"port": 6053, "encryption": {"key": "6dGhpcyBpcyBhIHRlc3Q="}},
    "ota": [{"platform": "esphome", "port": 3232, "password": "secret"}],
    "wifi": {"ssid": "ssid", "use_address": "192.168.1.42"},
}


def _cache_body(config: dict | None = None) -> str:
    """Render the JSON envelope the production save writes."""
    return json.dumps(
        {"v": 1, "esphome": const.__version__, "config": config or _VALIDATED_CONFIG}
    )


def _write_storage(
    storage_path: Path,
    *,
    esp_platform: str | None = "ESP32",
    core_platform: str | None = "esp32",
    build_path: str | None = "/build/lite_test",
    toolchain: str | None = None,
) -> None:
    """Write a vanilla StorageJSON sidecar for the cache tests."""
    storage_path.parent.mkdir(parents=True, exist_ok=True)
    data = {
        "storage_version": 1,
        "name": "lite_test",
        "friendly_name": "Lite Test Device",
        "comment": None,
        "esphome_version": "2026.1.0",
        "src_version": 1,
        "address": "192.168.1.42",
        "web_port": None,
        "esp_platform": esp_platform,
        "build_path": build_path,
        "firmware_bin_path": "/build/lite_test/firmware.bin",
        "loaded_integrations": ["api", "logger", "ota", "wifi"],
        "loaded_platforms": [],
        "no_mdns": False,
        "framework": "arduino",
        "core_platform": core_platform,
        "toolchain": toolchain,
    }
    storage_path.write_text(json.dumps(data), encoding="utf-8")


def _write_cache(cache_path: Path, body: str | None = None) -> Path:
    """Write the cache file and return it."""
    cache_path.parent.mkdir(parents=True, exist_ok=True)
    cache_path.write_text(body if body is not None else _cache_body(), encoding="utf-8")
    return cache_path


def _set_cache_mtime(cache_path: Path, yaml_path: Path, *, offset: int) -> None:
    """Force the cache file's mtime relative to the source YAML.

    Positive offset → cache is fresh. Negative → cache is stale.
    """
    yaml_stat = yaml_path.stat()
    os.utime(cache_path, (yaml_stat.st_atime, yaml_stat.st_mtime + offset))


@pytest.fixture
def primed_storage(tmp_path: Path) -> Path:
    """YAML + StorageJSON sidecar, no cache yet."""
    yaml_path = tmp_path / "lite_test.yaml"
    yaml_path.write_text("esphome:\n  name: lite_test\n")
    CORE.config_path = yaml_path
    _write_storage(tmp_path / ".esphome" / "storage" / "lite_test.yaml.json")
    return yaml_path


@pytest.fixture
def fresh_cache_files(primed_storage: Path) -> Path:
    """YAML + StorageJSON + cache, all consistent and fresh."""
    storage_dir = primed_storage.parent / ".esphome" / "storage"
    cache = _write_cache(storage_dir / "lite_test.yaml.validated.json")
    _set_cache_mtime(cache, primed_storage, offset=5)
    return primed_storage


def test_compiled_config_path_lives_alongside_sidecar(setup_core: Path) -> None:
    """The cache file shape is predictable from the YAML filename."""
    path = compiled_config_path("device.yaml")
    assert path.name == "device.yaml.validated.json"
    assert path.parent.name == "storage"


def test_load_compiled_config_happy_path(fresh_cache_files: Path) -> None:
    """Fresh cache + sidecar → returns config and populates CORE."""
    config = load_compiled_config(fresh_cache_files)

    assert config is not None
    assert config[CONF_ESPHOME][CONF_NAME] == "lite_test"
    assert config[CONF_API]["encryption"]["key"] == "6dGhpcyBpcyBhIHRlc3Q="
    assert config["ota"][0]["password"] == "secret"

    # The fast path loads plain scalars; no per-node source ranges exist.
    assert type(config[CONF_ESPHOME][CONF_NAME]) is str

    # apply_to_core populated exactly what upload/logs read off CORE.
    assert CORE.name == "lite_test"
    assert CORE.build_path == Path("/build/lite_test")
    assert CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] == "esp32"
    assert CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] == "arduino"
    # upload_using_esptool reads get_esp32_variant() off CORE.data[KEY_ESP32].
    assert CORE.data[KEY_ESP32][KEY_VARIANT] == "ESP32"


def test_load_compiled_config_populates_esp32_variant(tmp_path: Path) -> None:
    """ESP32 variants survive the cache fast path so esptool gets the right --chip."""
    yaml_path = tmp_path / "lite_test.yaml"
    yaml_path.write_text("esphome:\n  name: lite_test\n")
    CORE.config_path = yaml_path

    storage_dir = tmp_path / ".esphome" / "storage"
    _write_storage(storage_dir / "lite_test.yaml.json", esp_platform="ESP32S3")
    cache = _write_cache(storage_dir / "lite_test.yaml.validated.json")
    _set_cache_mtime(cache, yaml_path, offset=5)

    assert load_compiled_config(yaml_path) is not None
    assert CORE.data[KEY_ESP32][KEY_VARIANT] == "ESP32S3"


def test_load_compiled_config_skips_esp32_block_for_other_platforms(
    tmp_path: Path,
) -> None:
    """Non-esp32 targets shouldn't fabricate an esp32 data block."""
    yaml_path = tmp_path / "lite_test.yaml"
    yaml_path.write_text("esphome:\n  name: lite_test\n")
    CORE.config_path = yaml_path

    storage_dir = tmp_path / ".esphome" / "storage"
    _write_storage(
        storage_dir / "lite_test.yaml.json",
        esp_platform="ESP8266",
        core_platform="esp8266",
    )
    cache = _write_cache(storage_dir / "lite_test.yaml.validated.json")
    _set_cache_mtime(cache, yaml_path, offset=5)

    assert load_compiled_config(yaml_path) is not None
    assert KEY_ESP32 not in CORE.data


@pytest.mark.parametrize(
    "scenario",
    ["missing_cache", "stale_cache", "corrupt_cache", "missing_sidecar"],
)
def test_load_compiled_config_falls_back(tmp_path: Path, scenario: str) -> None:
    """All non-happy cases return None so the caller falls back."""
    yaml_path = tmp_path / "lite_test.yaml"
    yaml_path.write_text("esphome:\n  name: lite_test\n")
    CORE.config_path = yaml_path
    storage_dir = tmp_path / ".esphome" / "storage"
    cache_path = storage_dir / "lite_test.yaml.validated.json"
    sidecar_path = storage_dir / "lite_test.yaml.json"

    if scenario == "missing_cache":
        pass  # no cache, no sidecar
    elif scenario == "stale_cache":
        _write_storage(sidecar_path)
        _set_cache_mtime(_write_cache(cache_path), yaml_path, offset=-60)
    elif scenario == "corrupt_cache":
        _write_storage(sidecar_path)
        _set_cache_mtime(
            _write_cache(cache_path, '{"v": 1, "config": {'), yaml_path, offset=5
        )
    elif scenario == "missing_sidecar":
        # Cache fresh + parseable, but no StorageJSON → can't populate CORE.
        _set_cache_mtime(_write_cache(cache_path), yaml_path, offset=5)

    assert load_compiled_config(yaml_path) is None


@pytest.mark.parametrize(
    "body",
    [
        pytest.param(
            json.dumps(
                {"v": 999, "esphome": const.__version__, "config": {"esphome": {}}}
            ),
            id="wrong_version",
        ),
        pytest.param(
            json.dumps({"esphome": const.__version__, "config": {"esphome": {}}}),
            id="missing_version",
        ),
        pytest.param(
            json.dumps({"v": 1, "esphome": "2020.1.0", "config": {"esphome": {}}}),
            id="other_esphome_version",
        ),
        pytest.param(
            json.dumps({"v": 1, "config": {"esphome": {}}}),
            id="missing_esphome_version",
        ),
        pytest.param(
            json.dumps(
                {
                    "v": 1,
                    "esphome": const.__version__,
                    "config": ["not", "a", "dict"],
                }
            ),
            id="non_dict_config",
        ),
        pytest.param(
            json.dumps({"v": 1, "esphome": const.__version__}), id="missing_config"
        ),
        pytest.param(json.dumps(["not", "an", "envelope"]), id="non_dict_envelope"),
    ],
)
def test_load_compiled_config_rejects_bad_envelope(
    primed_storage: Path, body: str
) -> None:
    """A foreign or future cache shape falls back instead of half-loading."""
    storage_dir = primed_storage.parent / ".esphome" / "storage"
    cache = _write_cache(storage_dir / "lite_test.yaml.validated.json", body)
    _set_cache_mtime(cache, primed_storage, offset=5)

    assert load_compiled_config(primed_storage) is None


def test_load_ignores_legacy_yaml_cache(primed_storage: Path) -> None:
    """A fresh pre-JSON ``.validated.yaml`` alone can't drive the fast path."""
    storage_dir = primed_storage.parent / ".esphome" / "storage"
    legacy = _write_cache(
        storage_dir / "lite_test.yaml.validated.yaml", "esphome:\n  name: lite_test\n"
    )
    _set_cache_mtime(legacy, primed_storage, offset=5)

    assert load_compiled_config(primed_storage) is None


def test_save_removes_stale_legacy_yaml_cache(tmp_path: Path) -> None:
    """A successful save leaves only the JSON cache behind."""
    CORE.config_path = tmp_path / "lite_test.yaml"
    legacy = tmp_path / ".esphome" / "storage" / "lite_test.yaml.validated.yaml"
    legacy.parent.mkdir(parents=True, exist_ok=True)
    legacy.write_text("esphome:\n  name: lite_test\n")

    save_compiled_config({"esphome": {"name": "lite_test"}})

    assert compiled_config_path("lite_test.yaml").is_file()
    assert not legacy.exists()


def test_save_removes_legacy_yaml_even_when_write_fails(tmp_path: Path) -> None:
    """The secret-bearing legacy cache goes away regardless of write outcome."""
    CORE.config_path = tmp_path / "lite_test.yaml"
    legacy = tmp_path / ".esphome" / "storage" / "lite_test.yaml.validated.yaml"
    legacy.parent.mkdir(parents=True, exist_ok=True)
    legacy.write_text("esphome:\n  name: lite_test\n")

    with patch("esphome.compiled_config.write_file", side_effect=RuntimeError("boom")):
        save_compiled_config({"esphome": {"name": "lite_test"}})

    assert not legacy.exists()
    assert not compiled_config_path("lite_test.yaml").exists()


def test_save_warns_when_legacy_cache_unremovable(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A secret-bearing legacy file that won't unlink warns; the write proceeds."""
    CORE.config_path = tmp_path / "lite_test.yaml"
    legacy = tmp_path / ".esphome" / "storage" / "lite_test.yaml.validated.yaml"
    legacy.parent.mkdir(parents=True, exist_ok=True)
    legacy.mkdir()  # unlink() on a directory raises OSError

    with caplog.at_level("WARNING", logger="esphome.compiled_config"):
        save_compiled_config({"esphome": {"name": "lite_test"}})

    assert "legacy validated-config cache" in caplog.text
    assert compiled_config_path("lite_test.yaml").is_file()


@pytest.mark.parametrize("command", ["upload", "logs"])
def test_run_esphome_upload_and_logs_use_cache_when_fresh(
    command: str,
    fresh_cache_files: Path,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """upload/logs skip read_config() when the cache is fresh."""
    captured: dict = {}

    def _stub(_args, config):
        captured["config"] = config
        return 0

    with (
        caplog.at_level("INFO", logger="esphome.__main__"),
        patch("esphome.config.read_config") as mock_read,
        patch.dict("esphome.__main__.POST_CONFIG_ACTIONS", {command: _stub}),
    ):
        assert run_esphome(["esphome", command, str(fresh_cache_files)]) == 0

    mock_read.assert_not_called()
    assert captured["config"][CONF_ESPHOME][CONF_NAME] == "lite_test"
    assert captured["config"][CONF_API]["encryption"]["key"] == "6dGhpcyBpcyBhIHRlc3Q="
    # The success-branch log line is part of the patch; assert on it so
    # branch coverage stays unambiguous in CI.
    assert "Loaded validated config cache" in caplog.text


@pytest.mark.parametrize("command", ["upload", "logs"])
def test_run_esphome_upload_and_logs_fall_back_when_no_cache(
    tmp_path: Path, command: str
) -> None:
    """Without a cache, the dispatcher falls back to read_config()."""
    yaml_path = tmp_path / "lite_test.yaml"
    yaml_path.write_text("esphome:\n  name: lite_test\n")

    with (
        patch("esphome.config.read_config", return_value=None) as mock_read,
        patch.dict(
            "esphome.__main__.POST_CONFIG_ACTIONS",
            {command: lambda args, config: 0},
        ),
    ):
        assert run_esphome(["esphome", command, str(yaml_path)]) == 2

    mock_read.assert_called_once()


def _storage_fixture(tmp_path: Path) -> StorageJSON:
    """A loaded StorageJSON instance matching _write_storage's contents."""
    fixture = tmp_path / "fixture_storage.json"
    _write_storage(fixture)
    return StorageJSON.load(fixture)


def _bare_yaml(tmp_path: Path) -> Path:
    """A minimal YAML with CORE.config_path pointed at it."""
    yaml_path = tmp_path / "lite_test.yaml"
    yaml_path.write_text("esphome:\n  name: lite_test\n")
    CORE.config_path = yaml_path
    return yaml_path


@contextmanager
def _fallback_run(command: str = "upload", **from_core_kwargs) -> Any:
    """Patch the fallback path's collaborators for a run_esphome call.

    Without kwargs, from_esphome_core stays real (yielded mock is None).
    """
    with (
        patch(
            "esphome.config.read_config",
            return_value={"esphome": {"name": "lite_test"}},
        ) as mock_read,
        patch.dict(
            "esphome.__main__.POST_CONFIG_ACTIONS",
            {command: lambda args, config: 0},
        ),
    ):
        if not from_core_kwargs:
            yield mock_read, None
            return
        with patch.object(
            StorageJSON, "from_esphome_core", **from_core_kwargs
        ) as mock_from_core:
            yield mock_read, mock_from_core


@pytest.mark.parametrize("command", ["upload", "logs"])
def test_run_esphome_fallback_writes_sidecar_and_cache_without_sidecar(
    tmp_path: Path, command: str
) -> None:
    """A never-compiled config caches on its first upload/logs run: the
    fallback writes the StorageJSON sidecar itself (load_compiled_config
    needs it), so the second run hits the fast path."""
    yaml_path = _bare_yaml(tmp_path)
    storage_dir = tmp_path / ".esphome" / "storage"

    with _fallback_run(command, return_value=_storage_fixture(tmp_path)) as (
        mock_read,
        mock_from_core,
    ):
        assert run_esphome(["esphome", command, str(yaml_path)]) == 0
        mock_from_core.assert_called_once()
        assert (storage_dir / "lite_test.yaml.validated.json").exists()
        storage = StorageJSON.load(storage_dir / "lite_test.yaml.json")
        assert storage is not None
        # No compile happened, so the sidecar must not claim one.
        assert mock_from_core.call_args.kwargs == {"claim_build": False}

        # The second run loads the cache instead of re-validating.
        assert run_esphome(["esphome", command, str(yaml_path)]) == 0
        mock_read.assert_called_once()


# as_dict serialized unset paths as str(None) until 2026.9; files
# written by those wizards are still on disk.
_WIZARD_SIDECAR_CASES = pytest.mark.parametrize(
    "wizard_kwargs",
    [
        {"esp_platform": None, "core_platform": None, "build_path": None},
        {"build_path": None},
        {"build_path": "None"},
    ],
    ids=["legacy_wizard", "modern_wizard", "none_string_wizard"],
)


def _prime_core(tmp_path: Path) -> None:
    """Set the post-validation CORE state from_esphome_core reads."""
    CORE.name = "lite_test"
    CORE.build_path = tmp_path / "build" / "lite_test"
    CORE.data[KEY_CORE] = {
        KEY_TARGET_PLATFORM: "esp8266",
        KEY_TARGET_FRAMEWORK: "arduino",
    }


@_WIZARD_SIDECAR_CASES
def test_run_esphome_fallback_completes_wizard_sidecar(
    tmp_path: Path, wizard_kwargs: dict[str, Any]
) -> None:
    """A wizard-written sidecar can't drive the fast path (no build_path;
    older wizards also no platform fields); the fallback rewrites it from
    CORE so the cache loads on the next run."""
    yaml_path = _bare_yaml(tmp_path)
    storage_dir = tmp_path / ".esphome" / "storage"
    _write_storage(storage_dir / "lite_test.yaml.json", **wizard_kwargs)

    with _fallback_run(return_value=_storage_fixture(tmp_path)) as (_, mock_from_core):
        assert run_esphome(["esphome", "upload", str(yaml_path)]) == 0

    mock_from_core.assert_called_once()
    storage = StorageJSON.load(storage_dir / "lite_test.yaml.json")
    assert storage is not None and storage.core_platform == "esp32"
    # What the wizard recorded about a build (nothing, or a real one)
    # carries through instead of being stamped with this run's values.
    assert storage.esphome_version == "2026.1.0"
    assert load_compiled_config(yaml_path) is not None


def test_run_esphome_fallback_skips_cache_when_sidecar_write_fails(
    tmp_path: Path,
) -> None:
    """A failed sidecar write is non-fatal and skips the cache save too:
    without the sidecar the cache could never be loaded back, so writing
    it would only leave resolved secrets on disk."""
    yaml_path = _bare_yaml(tmp_path)

    with (
        _fallback_run(side_effect=RuntimeError("boom")),
        patch("esphome.compiled_config.save_compiled_config") as mock_save,
    ):
        assert run_esphome(["esphome", "upload", str(yaml_path)]) == 0

    mock_save.assert_not_called()
    assert not (tmp_path / ".esphome" / "storage" / "lite_test.yaml.json").exists()


def test_run_esphome_fallback_write_failure_takes_io_branch(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """StorageJSON.save raises EsphomeError (write_file wraps OSError into
    it), which must land in the plain I/O warning, not the traceback
    branch for structural bugs."""
    yaml_path = _bare_yaml(tmp_path)

    with (
        _fallback_run(return_value=_storage_fixture(tmp_path)),
        patch.object(StorageJSON, "save", side_effect=EsphomeError("boom")),
        patch("esphome.compiled_config.save_compiled_config") as mock_save,
        caplog.at_level("WARNING", logger="esphome.compiled_config"),
    ):
        assert run_esphome(["esphome", "upload", str(yaml_path)]) == 0

    mock_save.assert_not_called()
    assert "Could not refresh the storage sidecar" in caplog.text
    assert "Unexpected error" not in caplog.text


def test_run_esphome_fallback_leaves_unreadable_sidecar_alone(tmp_path: Path) -> None:
    """A present-but-corrupt sidecar is not overwritten: it may hold a real
    build's metadata, and replacing it would suppress the next compile's
    clean of a possibly incoherent build tree. The cache save is skipped."""
    yaml_path = _bare_yaml(tmp_path)
    storage_dir = tmp_path / ".esphome" / "storage"
    sidecar = storage_dir / "lite_test.yaml.json"
    sidecar.parent.mkdir(parents=True, exist_ok=True)
    sidecar.write_text("{truncated", encoding="utf-8")

    with _fallback_run(return_value=None) as (_, mock_from_core):
        assert run_esphome(["esphome", "upload", str(yaml_path)]) == 0

    mock_from_core.assert_not_called()
    assert sidecar.read_text(encoding="utf-8") == "{truncated"
    assert not (storage_dir / "lite_test.yaml.validated.json").exists()


def test_run_esphome_fallback_skips_cache_when_rebuilt_sidecar_incomplete(
    tmp_path: Path,
) -> None:
    """If the rebuilt sidecar would still be incomplete, nothing is written:
    the cache could never be loaded back, so saving it would only rewrite
    resolved secrets on every run."""
    yaml_path = _bare_yaml(tmp_path)
    storage_dir = tmp_path / ".esphome" / "storage"

    incomplete = tmp_path / "incomplete_storage.json"
    _write_storage(incomplete, build_path=None)

    with _fallback_run(return_value=StorageJSON.load(incomplete)):
        assert run_esphome(["esphome", "upload", str(yaml_path)]) == 0

    assert not (storage_dir / "lite_test.yaml.json").exists()
    assert not (storage_dir / "lite_test.yaml.validated.json").exists()


def test_run_esphome_fallback_sidecar_records_platformio_toolchain(
    tmp_path: Path,
) -> None:
    """The toolchain fallback runs before the sidecar write, so platforms
    whose validators leave CORE.toolchain unset record the same
    "platformio" a compile writes, not null."""
    yaml_path = _bare_yaml(tmp_path)
    _prime_core(tmp_path)
    assert CORE.toolchain is None

    with _fallback_run():
        assert run_esphome(["esphome", "upload", str(yaml_path)]) == 0

    storage = StorageJSON.load(
        tmp_path / ".esphome" / "storage" / "lite_test.yaml.json"
    )
    assert storage is not None
    assert storage.toolchain == "platformio"


@pytest.mark.parametrize("existing_sidecar", [None, "wizard"])
def test_run_esphome_fallback_skips_sidecar_when_build_tree_exists(
    tmp_path: Path, existing_sidecar: str | None
) -> None:
    """An existing build tree with a missing or wizard-only sidecar keeps
    it that way: the mismatch is what makes the next compile wipe the
    unknown tree, so the fallback writes nothing and skips the cache."""
    yaml_path = _bare_yaml(tmp_path)
    _prime_core(tmp_path)
    CORE.build_path.mkdir(parents=True)
    storage_dir = tmp_path / ".esphome" / "storage"
    if existing_sidecar == "wizard":
        _write_storage(storage_dir / "lite_test.yaml.json", build_path=None)
        wizard_body = (storage_dir / "lite_test.yaml.json").read_text(encoding="utf-8")

    with _fallback_run(return_value=_storage_fixture(tmp_path)) as (_, mock_from_core):
        assert run_esphome(["esphome", "upload", str(yaml_path)]) == 0

    mock_from_core.assert_not_called()
    assert not (storage_dir / "lite_test.yaml.validated.json").exists()
    if existing_sidecar == "wizard":
        sidecar_body = (storage_dir / "lite_test.yaml.json").read_text(encoding="utf-8")
        assert sidecar_body == wizard_body
    else:
        assert not (storage_dir / "lite_test.yaml.json").exists()


def test_save_compiled_config_and_sidecar_builds_real_sidecar(tmp_path: Path) -> None:
    """Drive the real from_esphome_core on the fallback path: the
    post-validation CORE state yields a complete, loadable sidecar."""
    yaml_path = _bare_yaml(tmp_path)
    _prime_core(tmp_path)
    CORE.config = {CONF_ESPHOME: {CONF_NAME: "lite_test"}}
    CORE.toolchain = Toolchain.PLATFORMIO

    save_compiled_config_and_sidecar(CORE.config)

    storage = StorageJSON.load(
        tmp_path / ".esphome" / "storage" / "lite_test.yaml.json"
    )
    assert storage is not None
    assert storage.core_platform == "esp8266"
    assert storage.build_path is not None
    # No compile happened, so the sidecar must not claim one.
    assert storage.esphome_version is None
    assert storage.firmware_bin_path is None
    assert load_compiled_config(yaml_path) is not None


@pytest.mark.parametrize(
    ("sidecar_toolchain", "saved"),
    [
        ("esp-idf", False),
        ("platformio", True),
        (None, True),  # legacy sidecar without the field: guard is inert
    ],
)
def test_save_compiled_config_and_sidecar_toolchain_mismatch(
    tmp_path: Path, sidecar_toolchain: str | None, saved: bool
) -> None:
    """A config validated under a different toolchain than the compile's
    must not overwrite the cache."""
    yaml_path = _bare_yaml(tmp_path)
    _prime_core(tmp_path)
    CORE.config = {CONF_ESPHOME: {CONF_NAME: "lite_test"}}
    CORE.toolchain = Toolchain.PLATFORMIO
    _write_storage(
        tmp_path / ".esphome" / "storage" / "lite_test.yaml.json",
        toolchain=sidecar_toolchain,
    )

    save_compiled_config_and_sidecar(CORE.config)

    cache = tmp_path / ".esphome" / "storage" / "lite_test.yaml.validated.json"
    assert cache.exists() is saved
    assert (load_compiled_config(yaml_path) is not None) is saved


@pytest.mark.parametrize("command", ["upload", "logs"])
def test_run_esphome_upload_and_logs_refresh_cache_on_fallback(
    tmp_path: Path, command: str
) -> None:
    """A stale-cache fallback rewrites the cache so the next call hits
    the fast path. Without this, every upload/logs after a YAML edit
    pays for read_config() until the next compile rewrites the cache."""
    yaml_path = tmp_path / "lite_test.yaml"
    yaml_path.write_text("esphome:\n  name: lite_test\n")
    CORE.config_path = yaml_path

    storage_dir = tmp_path / ".esphome" / "storage"
    _write_storage(storage_dir / "lite_test.yaml.json")
    cache = _write_cache(storage_dir / "lite_test.yaml.validated.json")
    _set_cache_mtime(cache, yaml_path, offset=-60)  # stale

    fresh_config = {"esphome": {"name": "lite_test"}, "logger": {}}

    with (
        patch("esphome.config.read_config", return_value=fresh_config),
        patch(
            "esphome.compiled_config.save_compiled_config", wraps=save_compiled_config
        ) as mock_save,
        patch.object(StorageJSON, "from_esphome_core") as mock_from_core,
        patch.dict(
            "esphome.__main__.POST_CONFIG_ACTIONS",
            {command: lambda args, config: 0},
        ),
    ):
        assert run_esphome(["esphome", command, str(yaml_path)]) == 0

    mock_save.assert_called_once_with(fresh_config)
    # The compile-written sidecar is complete; the fallback leaves it alone.
    mock_from_core.assert_not_called()
    # mtime is now newer than the source YAML, so a follow-up call hits
    # the fast path instead of repeating read_config.
    assert cache.stat().st_mtime >= yaml_path.stat().st_mtime


def test_run_esphome_upload_with_substitution_does_not_refresh_cache(
    fresh_cache_files: Path,
) -> None:
    """`-s` substitutions skip the cache on both read and write -- saving
    here would clobber the cache with a substitution-specific config."""
    with (
        patch("esphome.config.read_config", return_value={"esphome": {}}),
        patch("esphome.compiled_config.save_compiled_config") as mock_save,
        patch.dict(
            "esphome.__main__.POST_CONFIG_ACTIONS",
            {"upload": lambda args, config: 0},
        ),
    ):
        run_esphome(["esphome", "-s", "var", "val", "upload", str(fresh_cache_files)])

    mock_save.assert_not_called()


def test_run_esphome_compile_does_not_refresh_cache_via_fallback(
    fresh_cache_files: Path,
) -> None:
    """Compile writes the cache through update_storage_json, not via the
    upload/logs fallback path -- the fallback save would skip the
    storage_should_clean check."""
    with (
        patch("esphome.config.read_config", return_value={"esphome": {}}),
        patch("esphome.compiled_config.save_compiled_config") as mock_save,
        patch.dict(
            "esphome.__main__.POST_CONFIG_ACTIONS",
            {"compile": lambda args, config: 0},
        ),
    ):
        run_esphome(["esphome", "compile", str(fresh_cache_files)])

    mock_save.assert_not_called()


def test_run_esphome_upload_with_substitution_skips_cache(
    fresh_cache_files: Path,
) -> None:
    """`-s key value` forces a fresh validation -- the cache was written
    against the prior substitution set, so reusing it would silently
    ignore the override."""
    with (
        patch("esphome.config.read_config", return_value=None) as mock_read,
        patch.dict(
            "esphome.__main__.POST_CONFIG_ACTIONS",
            {"upload": lambda args, config: 0},
        ),
    ):
        run_esphome(["esphome", "-s", "var", "val", "upload", str(fresh_cache_files)])

    mock_read.assert_called_once()


def test_run_esphome_compile_does_not_use_cache(fresh_cache_files: Path) -> None:
    """The compile subcommand always re-validates -- it's what writes the cache."""
    with (
        patch("esphome.config.read_config", return_value=None) as mock_read,
        patch.dict(
            "esphome.__main__.POST_CONFIG_ACTIONS",
            {"compile": lambda args, config: 0},
        ),
    ):
        run_esphome(["esphome", "compile", str(fresh_cache_files)])

    mock_read.assert_called_once()


def test_save_compiled_config_writes_cache(tmp_path: Path) -> None:
    """`save_compiled_config` writes the JSON envelope next to the sidecar."""
    CORE.config_path = tmp_path / "lite_test.yaml"
    save_compiled_config({"esphome": {"name": "lite_test"}, "logger": {}})

    cache_path = compiled_config_path("lite_test.yaml")
    assert cache_path.is_file()
    envelope = json.loads(cache_path.read_text())
    assert envelope["v"] == 1
    assert envelope["esphome"] == const.__version__
    assert envelope["config"] == {"esphome": {"name": "lite_test"}, "logger": {}}


def test_save_compiled_config_swallows_write_errors(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """Failures during the write are non-fatal -- a bad cache just means
    the next fast path falls back to read_config()."""
    CORE.config_path = tmp_path / "lite_test.yaml"
    with patch("esphome.compiled_config.write_file", side_effect=RuntimeError("boom")):
        save_compiled_config({"esphome": {"name": "lite_test"}})
    assert not compiled_config_path("lite_test.yaml").exists()


def test_save_stringifies_unknown_values(tmp_path: Path) -> None:
    """A type with no dedicated encoding stores its string form."""

    class Weird:
        def __str__(self) -> str:
            return "weird-str"

    CORE.config_path = tmp_path / "lite_test.yaml"
    save_compiled_config({"esphome": {"name": "lite_test", "weird": Weird()}})
    envelope = json.loads(compiled_config_path("lite_test.yaml").read_text())
    assert envelope["config"]["esphome"]["weird"] == "weird-str"


def test_save_skips_cache_on_unserializable_key(tmp_path: Path) -> None:
    """A non-basic dict key aborts the write; the fast path falls back."""
    CORE.config_path = tmp_path / "lite_test.yaml"
    save_compiled_config({"esphome": {("a", "b"): "lite_test"}})
    assert not compiled_config_path("lite_test.yaml").exists()


def _normalize(value: Any) -> Any:
    """Make Lambda comparable; everything else compares by value already."""
    if isinstance(value, Lambda):
        return ("__lambda__", value.value)
    if isinstance(value, dict):
        return {k: _normalize(v) for k, v in value.items()}
    if isinstance(value, (list, tuple)):
        return [_normalize(v) for v in value]
    return value


def _round_trip_config() -> OrderedDict:
    """A post-validation shaped config exercising every representer type."""
    return OrderedDict(
        {
            "esphome": OrderedDict(
                {
                    "name": "lite_test",
                    "build_path": Path("/build/lite_test"),
                    "on_boot": [
                        OrderedDict(
                            {
                                "trigger_id": ID("trigger_1", type="Trigger"),
                                "then": [{"lambda": Lambda('ESP_LOGD("t", "x");')}],
                            }
                        )
                    ],
                }
            ),
            "wifi": OrderedDict(
                {
                    "id": ID("wifi_id", type="WiFiComponent"),
                    "reboot_timeout": TimePeriodMilliseconds(milliseconds=900000),
                    "use_address": IPv4Address("192.168.1.42"),
                    "subnet": IPv4Network("192.168.1.0/24"),
                    "mac": MACAddress(0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01),
                }
            ),
            "misc": OrderedDict(
                {
                    "uuid": UUID("12345678-1234-5678-1234-567812345678"),
                    "toolchain": Toolchain.PLATFORMIO,
                    "hex": HexInt(0x1234),
                    "levels": (1, 2.5, True, None),
                    "empty": {},
                }
            ),
        }
    )


def test_cache_round_trip_matches_yaml_cache(primed_storage: Path) -> None:
    """The JSON cache loads the same tree the YAML cache used to."""
    config = _round_trip_config()
    save_compiled_config(config)
    from_json = load_compiled_config(primed_storage)
    assert from_json is not None

    yaml_cache = primed_storage.parent / "dumped.yaml"
    yaml_cache.write_text(yaml_util.dump(config, show_secrets=True))
    from_yaml = yaml_util.load_yaml(
        yaml_cache, clear_secrets=False, track_document_range=False
    )

    assert _normalize(from_json) == _normalize(from_yaml)


def test_lambda_sentinel_round_trips(primed_storage: Path) -> None:
    """A !lambda body comes back as a Lambda with the same source."""
    body = 'id(sensor_1).publish_state(42);\nreturn "multi\\nline";'
    save_compiled_config(
        {
            "esphome": {"name": "lite_test"},
            "script": [{"then": [{"lambda": Lambda(body)}]}],
        }
    )

    config = load_compiled_config(primed_storage)
    assert config is not None
    revived = config["script"][0]["then"][0]["lambda"]
    assert isinstance(revived, Lambda)
    assert revived.value == body


def test_object_hook_requires_exact_shape(primed_storage: Path) -> None:
    """Only the exact one-key string-valued sentinel revives a Lambda."""
    storage_dir = primed_storage.parent / ".esphome" / "storage"
    config = {
        "esphome": {"name": "lite_test"},
        "extra_key": {_LAMBDA_KEY: "x", "y": 1},
        "non_str": {_LAMBDA_KEY: 5},
    }
    cache = _write_cache(
        storage_dir / "lite_test.yaml.validated.json", _cache_body(config)
    )
    _set_cache_mtime(cache, primed_storage, offset=5)

    loaded = load_compiled_config(primed_storage)
    assert loaded is not None
    assert loaded["extra_key"] == {_LAMBDA_KEY: "x", "y": 1}
    assert loaded["non_str"] == {_LAMBDA_KEY: 5}


def test_int_keys_coerce_to_strings(primed_storage: Path) -> None:
    """Non-str basic keys stringify; validated configs only use string keys."""
    save_compiled_config({"esphome": {"name": "lite_test"}, "table": {1: "a", 2: "b"}})

    config = load_compiled_config(primed_storage)
    assert config is not None
    assert config["table"] == {"1": "a", "2": "b"}


@_WIZARD_SIDECAR_CASES
def test_load_compiled_config_rejects_wizard_only_sidecar(
    tmp_path: Path, wizard_kwargs: dict[str, Any]
) -> None:
    """A wizard-written sidecar (no build_path; older wizards also no
    platform fields) can't drive upload/logs, so the fast path falls back."""
    yaml_path = _bare_yaml(tmp_path)
    storage_dir = tmp_path / ".esphome" / "storage"
    _write_storage(storage_dir / "lite_test.yaml.json", **wizard_kwargs)
    cache_path = _write_cache(storage_dir / "lite_test.yaml.validated.json")
    _set_cache_mtime(cache_path, yaml_path, offset=5)

    assert load_compiled_config(yaml_path) is None
