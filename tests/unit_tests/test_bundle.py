"""Tests for esphome.bundle module."""

from __future__ import annotations

import io
import json
from pathlib import Path
import shutil
import tarfile
from typing import Any
from unittest.mock import patch

import pytest

from esphome.bundle import (
    BUNDLE_EXTENSION,
    CURRENT_MANIFEST_VERSION,
    MANIFEST_FILENAME,
    BundleManifest,
    ConfigBundleCreator,
    ManifestKey,
    _add_bytes_to_tar,
    _default_target_dir,
    _find_used_secret_keys,
    _force_load_include_files,
    extract_bundle,
    is_bundle_path,
    prepare_bundle_for_compile,
    read_bundle_manifest,
)
from esphome.core import CORE, EsphomeError

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _make_bundle(
    tmp_path: Path,
    config_filename: str = "test.yaml",
    config_content: str = "esphome:\n  name: test\n",
    manifest_overrides: dict[str, Any] | None = None,
    extra_files: dict[str, bytes] | None = None,
    *,
    include_manifest: bool = True,
    raw_members: list[tarfile.TarInfo] | None = None,
) -> Path:
    """Create a minimal bundle tar.gz for testing."""
    bundle_path = tmp_path / f"device{BUNDLE_EXTENSION}"
    buf = io.BytesIO()
    with tarfile.open(fileobj=buf, mode="w:gz") as tar:
        if include_manifest:
            manifest: dict[str, Any] = {
                ManifestKey.MANIFEST_VERSION: CURRENT_MANIFEST_VERSION,
                ManifestKey.ESPHOME_VERSION: "2026.2.0-test",
                ManifestKey.CONFIG_FILENAME: config_filename,
                ManifestKey.FILES: [config_filename],
                ManifestKey.HAS_SECRETS: False,
            }
            if manifest_overrides:
                manifest.update(manifest_overrides)
            _add_bytes_to_tar(tar, MANIFEST_FILENAME, json.dumps(manifest).encode())

        _add_bytes_to_tar(tar, config_filename, config_content.encode())

        if extra_files:
            for name, data in extra_files.items():
                _add_bytes_to_tar(tar, name, data)

        if raw_members:
            for info in raw_members:
                tar.addfile(info, io.BytesIO(b""))

    bundle_path.write_bytes(buf.getvalue())
    return bundle_path


def _setup_config_dir(
    tmp_path: Path,
    files: dict[str, str] | None = None,
) -> Path:
    """Set up a fake config directory with files and configure CORE."""
    config_dir = tmp_path / "config"
    config_dir.mkdir()

    config_yaml = "esphome:\n  name: test\n"
    (config_dir / "test.yaml").write_text(config_yaml)

    if files:
        for rel_path, content in files.items():
            p = config_dir / rel_path
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text(content)

    CORE.config_path = config_dir / "test.yaml"
    return config_dir


# ---------------------------------------------------------------------------
# is_bundle_path
# ---------------------------------------------------------------------------


@pytest.mark.parametrize(
    ("filename", "expected"),
    [
        (f"my_device{BUNDLE_EXTENSION}", True),
        (f"MY_DEVICE{BUNDLE_EXTENSION.upper()}", True),
        ("my_device.yaml", False),
        ("my_device.tar.gz", False),
        ("my_device.zip", False),
        ("", False),
    ],
)
def test_is_bundle_path(filename: str, expected: bool) -> None:
    assert is_bundle_path(Path(filename)) is expected


# ---------------------------------------------------------------------------
# _default_target_dir
# ---------------------------------------------------------------------------


def test_default_target_dir_strips_extension() -> None:
    p = Path(f"/builds/device{BUNDLE_EXTENSION}")
    result = _default_target_dir(p)
    assert result == Path("/builds/device")


def test_default_target_dir_no_extension() -> None:
    p = Path("/builds/device.other")
    result = _default_target_dir(p)
    assert result == Path("/builds/device.other")


# ---------------------------------------------------------------------------
# _find_used_secret_keys
# ---------------------------------------------------------------------------


def test_find_used_secret_keys(tmp_path: Path) -> None:
    yaml1 = tmp_path / "a.yaml"
    yaml1.write_text("wifi:\n  ssid: !secret wifi_ssid\n  password: !secret wifi_pw\n")
    yaml2 = tmp_path / "b.yaml"
    yaml2.write_text("api:\n  key: !secret api_key\n")

    keys = _find_used_secret_keys([yaml1, yaml2])
    assert keys == {"wifi_ssid", "wifi_pw", "api_key"}


def test_find_used_secret_keys_no_secrets(tmp_path: Path) -> None:
    yaml1 = tmp_path / "a.yaml"
    yaml1.write_text("esphome:\n  name: test\n")

    keys = _find_used_secret_keys([yaml1])
    assert keys == set()


def test_find_used_secret_keys_missing_file(tmp_path: Path) -> None:
    missing = tmp_path / "does_not_exist.yaml"
    keys = _find_used_secret_keys([missing])
    assert keys == set()


def test_find_used_secret_keys_deduplicates(tmp_path: Path) -> None:
    yaml1 = tmp_path / "a.yaml"
    yaml1.write_text("a: !secret key1\nb: !secret key1\n")

    keys = _find_used_secret_keys([yaml1])
    assert keys == {"key1"}


def test_find_used_secret_keys_quoted(tmp_path: Path) -> None:
    """Quoted !secret keys should resolve to the same key as unquoted form.

    YAML strips surrounding quotes during parsing, so the secrets.yaml
    lookup uses the unquoted key. The bundle scan must do the same.
    """
    yaml1 = tmp_path / "a.yaml"
    yaml1.write_text(
        "single: !secret 'wifi_ssid'\n"
        'double: !secret "wifi_pw"\n'
        "bare: !secret api_key\n"
    )

    keys = _find_used_secret_keys([yaml1])
    assert keys == {"wifi_ssid", "wifi_pw", "api_key"}


# ---------------------------------------------------------------------------
# _add_bytes_to_tar
# ---------------------------------------------------------------------------


def test_add_bytes_to_tar_deterministic_metadata() -> None:
    buf = io.BytesIO()
    with tarfile.open(fileobj=buf, mode="w:gz") as tar:
        _add_bytes_to_tar(tar, "hello.txt", b"world")

    buf.seek(0)
    with tarfile.open(fileobj=buf, mode="r:gz") as tar:
        member = tar.getmember("hello.txt")
        assert member.size == 5
        assert member.mtime == 0
        assert member.uid == 0
        assert member.gid == 0
        assert member.mode == 0o644
        assert tar.extractfile(member).read() == b"world"


# ---------------------------------------------------------------------------
# ManifestKey
# ---------------------------------------------------------------------------


def test_manifest_key_values() -> None:
    assert ManifestKey.MANIFEST_VERSION == "manifest_version"
    assert ManifestKey.ESPHOME_VERSION == "esphome_version"
    assert ManifestKey.CONFIG_FILENAME == "config_filename"
    assert ManifestKey.FILES == "files"
    assert ManifestKey.HAS_SECRETS == "has_secrets"


def test_manifest_key_is_str() -> None:
    """Verify ManifestKey values work as dict keys and JSON keys."""
    d: dict[str, int] = {ManifestKey.MANIFEST_VERSION: 1}
    assert d["manifest_version"] == 1


# ---------------------------------------------------------------------------
# extract_bundle
# ---------------------------------------------------------------------------


def test_extract_bundle_basic(tmp_path: Path) -> None:
    bundle_path = _make_bundle(tmp_path)
    target = tmp_path / "output"

    config_path = extract_bundle(bundle_path, target)

    assert config_path.is_file()
    assert config_path.name == "test.yaml"
    assert config_path.read_text().startswith("esphome:")
    assert (target / MANIFEST_FILENAME).is_file()


def test_extract_bundle_default_target_dir(tmp_path: Path) -> None:
    bundle_path = _make_bundle(tmp_path)

    config_path = extract_bundle(bundle_path)

    expected_dir = tmp_path / "device"
    assert config_path.parent == expected_dir


def test_extract_bundle_missing_file(tmp_path: Path) -> None:
    missing = tmp_path / f"missing{BUNDLE_EXTENSION}"
    with pytest.raises(EsphomeError, match="Bundle file not found"):
        extract_bundle(missing)


def test_extract_bundle_missing_manifest(tmp_path: Path) -> None:
    bundle_path = _make_bundle(tmp_path, include_manifest=False)
    with pytest.raises(EsphomeError, match="missing manifest.json"):
        extract_bundle(bundle_path, tmp_path / "out")


def test_extract_bundle_future_manifest_version(tmp_path: Path) -> None:
    bundle_path = _make_bundle(
        tmp_path,
        manifest_overrides={ManifestKey.MANIFEST_VERSION: 999},
    )
    with pytest.raises(EsphomeError, match="newer than this ESPHome"):
        extract_bundle(bundle_path, tmp_path / "out")


def test_extract_bundle_missing_config_filename_in_manifest(tmp_path: Path) -> None:
    """Manifest exists but is missing config_filename key."""
    bundle_path = tmp_path / f"bad{BUNDLE_EXTENSION}"
    buf = io.BytesIO()
    with tarfile.open(fileobj=buf, mode="w:gz") as tar:
        manifest = {ManifestKey.MANIFEST_VERSION: 1}
        _add_bytes_to_tar(tar, MANIFEST_FILENAME, json.dumps(manifest).encode())
        _add_bytes_to_tar(tar, "test.yaml", b"esphome:\n  name: test\n")
    bundle_path.write_bytes(buf.getvalue())

    with pytest.raises(EsphomeError, match="missing 'config_filename'"):
        extract_bundle(bundle_path, tmp_path / "out")


def test_extract_bundle_config_not_in_archive(tmp_path: Path) -> None:
    """Manifest references a config file that isn't in the archive."""
    bundle_path = _make_bundle(
        tmp_path,
        config_filename="test.yaml",
        manifest_overrides={ManifestKey.CONFIG_FILENAME: "missing.yaml"},
    )
    with pytest.raises(EsphomeError, match="was not found in the archive"):
        extract_bundle(bundle_path, tmp_path / "out")


def test_extract_bundle_with_extra_files(tmp_path: Path) -> None:
    bundle_path = _make_bundle(
        tmp_path,
        extra_files={
            "common/base.yaml": b"level: DEBUG\n",
            "includes/sensor.h": b"#pragma once\n",
        },
    )
    target = tmp_path / "out"
    extract_bundle(bundle_path, target)

    assert (target / "common" / "base.yaml").read_text() == "level: DEBUG\n"
    assert (target / "includes" / "sensor.h").read_text() == "#pragma once\n"


# ---------------------------------------------------------------------------
# extract_bundle - security validation
# ---------------------------------------------------------------------------


def test_extract_bundle_rejects_absolute_path(tmp_path: Path) -> None:
    info = tarfile.TarInfo(name="/etc/passwd")
    info.size = 0
    bundle_path = _make_bundle(tmp_path, raw_members=[info])

    with pytest.raises(EsphomeError, match="absolute path"):
        extract_bundle(bundle_path, tmp_path / "out")


def test_extract_bundle_rejects_path_traversal(tmp_path: Path) -> None:
    info = tarfile.TarInfo(name="../../../etc/passwd")
    info.size = 0
    bundle_path = _make_bundle(tmp_path, raw_members=[info])

    with pytest.raises(EsphomeError, match="path traversal"):
        extract_bundle(bundle_path, tmp_path / "out")


def test_extract_bundle_rejects_backslash_path_traversal(tmp_path: Path) -> None:
    info = tarfile.TarInfo(name="foo\\..\\..\\etc\\passwd")
    info.size = 0
    bundle_path = _make_bundle(tmp_path, raw_members=[info])

    with pytest.raises(EsphomeError, match="path traversal"):
        extract_bundle(bundle_path, tmp_path / "out")


def test_extract_bundle_rejects_symlink(tmp_path: Path) -> None:
    info = tarfile.TarInfo(name="evil_link")
    info.type = tarfile.SYMTYPE
    info.linkname = "/etc/passwd"
    info.size = 0
    bundle_path = _make_bundle(tmp_path, raw_members=[info])

    with pytest.raises(EsphomeError, match="symlink"):
        extract_bundle(bundle_path, tmp_path / "out")


def test_extract_bundle_rejects_oversized(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Archive whose total decompressed size exceeds the limit is rejected."""
    # Lower the limit so we don't need huge test data
    monkeypatch.setattr("esphome.bundle.MAX_DECOMPRESSED_SIZE", 100)

    bundle_path = _make_bundle(
        tmp_path,
        extra_files={"big.bin": b"\x00" * 200},
    )

    with pytest.raises(EsphomeError, match="decompressed size exceeds"):
        extract_bundle(bundle_path, tmp_path / "out")


def test_extract_bundle_corrupted_tar(tmp_path: Path) -> None:
    """Corrupted tar file raises EsphomeError."""
    bundle_path = tmp_path / f"bad{BUNDLE_EXTENSION}"
    bundle_path.write_bytes(b"not a tar file at all")

    with pytest.raises(EsphomeError, match="Failed to extract bundle"):
        extract_bundle(bundle_path, tmp_path / "out")


def test_extract_bundle_malformed_manifest_json(tmp_path: Path) -> None:
    """Invalid JSON in manifest.json raises EsphomeError."""
    bundle_path = tmp_path / f"badjson{BUNDLE_EXTENSION}"
    buf = io.BytesIO()
    with tarfile.open(fileobj=buf, mode="w:gz") as tar:
        _add_bytes_to_tar(tar, MANIFEST_FILENAME, b"{invalid json")
        _add_bytes_to_tar(tar, "test.yaml", b"esphome:\n  name: test\n")
    bundle_path.write_bytes(buf.getvalue())

    with pytest.raises(EsphomeError, match="malformed manifest.json"):
        extract_bundle(bundle_path, tmp_path / "out")


def test_extract_bundle_missing_manifest_version(tmp_path: Path) -> None:
    """Manifest without manifest_version raises EsphomeError."""
    bundle_path = tmp_path / f"nover{BUNDLE_EXTENSION}"
    buf = io.BytesIO()
    with tarfile.open(fileobj=buf, mode="w:gz") as tar:
        manifest = {ManifestKey.CONFIG_FILENAME: "test.yaml"}
        _add_bytes_to_tar(tar, MANIFEST_FILENAME, json.dumps(manifest).encode())
        _add_bytes_to_tar(tar, "test.yaml", b"esphome:\n  name: test\n")
    bundle_path.write_bytes(buf.getvalue())

    with pytest.raises(EsphomeError, match="missing 'manifest_version'"):
        extract_bundle(bundle_path, tmp_path / "out")


def test_extract_bundle_invalid_manifest_version_type(tmp_path: Path) -> None:
    """Non-integer manifest_version raises EsphomeError."""
    bundle_path = _make_bundle(
        tmp_path,
        manifest_overrides={ManifestKey.MANIFEST_VERSION: "not_an_int"},
    )

    with pytest.raises(EsphomeError, match="must be a positive integer"):
        extract_bundle(bundle_path, tmp_path / "out")


def test_extract_bundle_manifest_version_zero(tmp_path: Path) -> None:
    """manifest_version of 0 is rejected."""
    bundle_path = _make_bundle(
        tmp_path,
        manifest_overrides={ManifestKey.MANIFEST_VERSION: 0},
    )

    with pytest.raises(EsphomeError, match="must be a positive integer"):
        extract_bundle(bundle_path, tmp_path / "out")


def test_extract_bundle_manifest_too_large(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Oversized manifest.json is rejected."""
    monkeypatch.setattr("esphome.bundle.MAX_MANIFEST_SIZE", 50)

    bundle_path = _make_bundle(tmp_path)

    with pytest.raises(EsphomeError, match="manifest.json too large"):
        extract_bundle(bundle_path, tmp_path / "out")


def test_extract_bundle_manifest_not_regular_file(tmp_path: Path) -> None:
    """manifest.json that is a directory entry raises EsphomeError."""
    bundle_path = tmp_path / f"dirmanifest{BUNDLE_EXTENSION}"
    buf = io.BytesIO()
    with tarfile.open(fileobj=buf, mode="w:gz") as tar:
        # Add manifest.json as a directory instead of a file
        dir_info = tarfile.TarInfo(name=MANIFEST_FILENAME)
        dir_info.type = tarfile.DIRTYPE
        dir_info.size = 0
        tar.addfile(dir_info)
        _add_bytes_to_tar(tar, "test.yaml", b"esphome:\n  name: test\n")
    bundle_path.write_bytes(buf.getvalue())

    with pytest.raises(EsphomeError, match="not a regular file"):
        extract_bundle(bundle_path, tmp_path / "out")


# ---------------------------------------------------------------------------
# read_bundle_manifest
# ---------------------------------------------------------------------------


def test_read_bundle_manifest_corrupted_tar(tmp_path: Path) -> None:
    """Corrupted tar file raises EsphomeError via read_bundle_manifest."""
    bundle_path = tmp_path / f"bad{BUNDLE_EXTENSION}"
    bundle_path.write_bytes(b"not a tar file")

    with pytest.raises(EsphomeError, match="Failed to read bundle"):
        read_bundle_manifest(bundle_path)


def test_read_bundle_manifest(tmp_path: Path) -> None:
    bundle_path = _make_bundle(
        tmp_path,
        manifest_overrides={ManifestKey.HAS_SECRETS: True},
        extra_files={"secrets.yaml": b"wifi: test\n"},
    )

    manifest = read_bundle_manifest(bundle_path)

    assert isinstance(manifest, BundleManifest)
    assert manifest.manifest_version == CURRENT_MANIFEST_VERSION
    assert manifest.esphome_version == "2026.2.0-test"
    assert manifest.config_filename == "test.yaml"
    assert manifest.has_secrets is True


def test_read_bundle_manifest_minimal(tmp_path: Path) -> None:
    """Manifest with only required fields."""
    bundle_path = tmp_path / f"min{BUNDLE_EXTENSION}"
    buf = io.BytesIO()
    with tarfile.open(fileobj=buf, mode="w:gz") as tar:
        manifest = {
            ManifestKey.MANIFEST_VERSION: 1,
            ManifestKey.CONFIG_FILENAME: "cfg.yaml",
        }
        _add_bytes_to_tar(tar, MANIFEST_FILENAME, json.dumps(manifest).encode())
        _add_bytes_to_tar(tar, "cfg.yaml", b"")
    bundle_path.write_bytes(buf.getvalue())

    result = read_bundle_manifest(bundle_path)
    assert result.esphome_version == "unknown"
    assert not result.files
    assert result.has_secrets is False


# ---------------------------------------------------------------------------
# prepare_bundle_for_compile
# ---------------------------------------------------------------------------


def test_prepare_bundle_preserves_build_cache(tmp_path: Path) -> None:
    bundle_path = _make_bundle(tmp_path)
    target = tmp_path / "work"
    target.mkdir()

    # Pre-existing build cache
    esphome_dir = target / ".esphome"
    esphome_dir.mkdir()
    (esphome_dir / "build_state.json").write_text('{"cached": true}')

    pio_dir = target / ".pioenvs"
    pio_dir.mkdir()
    (pio_dir / "firmware.bin").write_bytes(b"\x00" * 100)

    config_path = prepare_bundle_for_compile(bundle_path, target)

    assert config_path.is_file()
    # Build caches should be preserved
    assert (target / ".esphome" / "build_state.json").read_text() == '{"cached": true}'
    assert (target / ".pioenvs" / "firmware.bin").read_bytes() == b"\x00" * 100


def test_prepare_bundle_cleans_old_config(tmp_path: Path) -> None:
    bundle_path = _make_bundle(tmp_path)
    target = tmp_path / "work"
    target.mkdir()

    # Old config from previous extraction
    (target / "old_config.yaml").write_text("old: true")
    old_dir = target / "old_includes"
    old_dir.mkdir()
    (old_dir / "old.h").write_text("// old")

    prepare_bundle_for_compile(bundle_path, target)

    # Old files should be cleaned
    assert not (target / "old_config.yaml").exists()
    assert not (target / "old_includes").exists()
    # New config should exist
    assert (target / "test.yaml").is_file()


def test_prepare_bundle_missing_file(tmp_path: Path) -> None:
    missing = tmp_path / f"missing{BUNDLE_EXTENSION}"
    with pytest.raises(EsphomeError, match="Bundle file not found"):
        prepare_bundle_for_compile(missing)


def test_prepare_bundle_cache_wins_over_bundle_content(tmp_path: Path) -> None:
    """Pre-existing build cache is restored even if the bundle contains those dirs."""
    bundle_path = _make_bundle(
        tmp_path,
        extra_files={
            ".esphome/from_bundle.json": b'{"from": "bundle"}',
        },
    )
    target = tmp_path / "work"
    target.mkdir()

    # Pre-existing build cache
    esphome_dir = target / ".esphome"
    esphome_dir.mkdir()
    (esphome_dir / "local_cache.json").write_text('{"from": "local"}')

    prepare_bundle_for_compile(bundle_path, target)

    # Local cache should win over bundle content
    assert (target / ".esphome" / "local_cache.json").read_text() == '{"from": "local"}'
    assert not (target / ".esphome" / "from_bundle.json").exists()


def test_prepare_bundle_default_target_dir(tmp_path: Path) -> None:
    """prepare_bundle_for_compile uses default dir when target_dir is None."""
    bundle_path = _make_bundle(tmp_path)

    config_path = prepare_bundle_for_compile(bundle_path)

    expected_dir = tmp_path / "device"
    assert config_path.parent == expected_dir
    assert config_path.is_file()


# ---------------------------------------------------------------------------
# ConfigBundleCreator - file discovery
# ---------------------------------------------------------------------------


def test_discover_files_includes_config(tmp_path: Path) -> None:
    _setup_config_dir(tmp_path)

    creator = ConfigBundleCreator({})
    files = creator.discover_files()

    paths = [f.path for f in files]
    assert "test.yaml" in paths


def test_discover_files_finds_path_objects(tmp_path: Path) -> None:
    """Path objects in validated config are discovered."""
    config_dir = _setup_config_dir(
        tmp_path,
        files={"assets/font.ttf": "fake font data"},
    )

    config: dict[str, Any] = {"font": [{"file": config_dir / "assets" / "font.ttf"}]}
    creator = ConfigBundleCreator(config)
    files = creator.discover_files()

    paths = [f.path for f in files]
    assert "assets/font.ttf" in paths


def test_discover_files_finds_absolute_string_paths(tmp_path: Path) -> None:
    """Absolute string paths in validated config are discovered."""
    config_dir = _setup_config_dir(
        tmp_path,
        files={"assets/logo.png": "fake png data"},
    )

    abs_path = str(config_dir / "assets" / "logo.png")
    config: dict[str, Any] = {"image": [{"file": abs_path}]}
    creator = ConfigBundleCreator(config)
    files = creator.discover_files()

    paths = [f.path for f in files]
    assert "assets/logo.png" in paths


def test_discover_files_skips_non_path_prefixes(tmp_path: Path) -> None:
    """Remote URLs and special prefixes are not treated as file paths."""
    _setup_config_dir(tmp_path)

    config: dict[str, Any] = {
        "font": [
            {"file": "https://example.com/font.ttf"},
            {"file": "mdi:home"},
            {"file": "http://example.com/icon.png"},
        ]
    }
    creator = ConfigBundleCreator(config)
    files = creator.discover_files()

    # Only the config file itself
    assert len(files) == 1
    assert files[0].path == "test.yaml"


def test_discover_files_skips_multiline_strings(tmp_path: Path) -> None:
    """Lambda/template strings are not treated as file paths."""
    _setup_config_dir(tmp_path)

    config: dict[str, Any] = {
        "sensor": [{"lambda": "auto val = id(sensor1);\nreturn val;"}]
    }
    creator = ConfigBundleCreator(config)
    files = creator.discover_files()

    assert len(files) == 1


def test_discover_files_deduplicates(tmp_path: Path) -> None:
    """Same file referenced twice is only included once."""
    config_dir = _setup_config_dir(
        tmp_path,
        files={"cert.pem": "fake cert"},
    )

    abs_path = str(config_dir / "cert.pem")
    config: dict[str, Any] = {
        "a": {"cert": abs_path},
        "b": {"cert": abs_path},
    }
    creator = ConfigBundleCreator(config)
    files = creator.discover_files()

    cert_files = [f for f in files if f.path == "cert.pem"]
    assert len(cert_files) == 1


def test_discover_files_skips_outside_config_dir(tmp_path: Path) -> None:
    """Files outside the config directory are skipped."""
    _setup_config_dir(tmp_path)

    outside_file = tmp_path / "outside.pem"
    outside_file.write_text("outside cert")

    config: dict[str, Any] = {"cert": str(outside_file)}
    creator = ConfigBundleCreator(config)
    files = creator.discover_files()

    paths = [f.path for f in files]
    assert "outside.pem" not in paths


def test_discover_files_esphome_includes(tmp_path: Path) -> None:
    """Paths listed in esphome.includes are discovered."""
    _setup_config_dir(
        tmp_path,
        files={"my_sensor.h": "#pragma once\n"},
    )

    config: dict[str, Any] = {
        "esphome": {"includes": ["my_sensor.h"]},
    }
    creator = ConfigBundleCreator(config)
    files = creator.discover_files()

    paths = [f.path for f in files]
    assert "my_sensor.h" in paths


def test_discover_files_esphome_includes_directory(tmp_path: Path) -> None:
    """esphome.includes pointing to a directory adds all files."""
    _setup_config_dir(
        tmp_path,
        files={
            "my_lib/a.h": "// a",
            "my_lib/b.cpp": "// b",
        },
    )

    config: dict[str, Any] = {
        "esphome": {"includes": ["my_lib"]},
    }
    creator = ConfigBundleCreator(config)
    files = creator.discover_files()

    paths = [f.path for f in files]
    assert "my_lib/a.h" in paths
    assert "my_lib/b.cpp" in paths


def test_discover_files_esphome_includes_skips_system(tmp_path: Path) -> None:
    """System includes like <Arduino.h> are not added."""
    _setup_config_dir(tmp_path)

    config: dict[str, Any] = {
        "esphome": {"includes": ["<Arduino.h>"]},
    }
    creator = ConfigBundleCreator(config)
    files = creator.discover_files()

    paths = [f.path for f in files]
    assert len(paths) == 1  # Just test.yaml


def test_discover_files_external_components_local(tmp_path: Path) -> None:
    """external_components with type: local adds the directory."""
    _setup_config_dir(
        tmp_path,
        files={
            "components/my_comp/__init__.py": "# comp",
            "components/my_comp/sensor.py": "# sensor",
        },
    )

    config: dict[str, Any] = {
        "external_components": [{"source": {"type": "local", "path": "components"}}],
    }
    creator = ConfigBundleCreator(config)
    files = creator.discover_files()

    paths = [f.path for f in files]
    assert "components/my_comp/__init__.py" in paths
    assert "components/my_comp/sensor.py" in paths


def test_discover_files_external_components_skips_pycache(tmp_path: Path) -> None:
    """__pycache__ directories inside local external_components are excluded."""
    _setup_config_dir(
        tmp_path,
        files={
            "components/my_comp/__init__.py": "# comp",
            "components/my_comp/__pycache__/module.cpython-313.pyc": "bytecode",
        },
    )

    config: dict[str, Any] = {
        "external_components": [{"source": {"type": "local", "path": "components"}}],
    }
    creator = ConfigBundleCreator(config)
    files = creator.discover_files()

    paths = [f.path for f in files]
    assert "components/my_comp/__init__.py" in paths
    assert not any("__pycache__" in p for p in paths)


def test_discover_files_external_components_non_dict_source(tmp_path: Path) -> None:
    """external_components with string source (e.g. github shorthand) is skipped."""
    _setup_config_dir(tmp_path)

    config: dict[str, Any] = {
        "external_components": [{"source": "github://user/repo@main"}],
    }
    creator = ConfigBundleCreator(config)
    files = creator.discover_files()

    # Only the config file itself - no crash from non-dict source
    assert len(files) == 1
    assert files[0].path == "test.yaml"


def test_discover_files_nested_config_values(tmp_path: Path) -> None:
    """Deeply nested Path objects in lists/dicts are found."""
    config_dir = _setup_config_dir(
        tmp_path,
        files={"deep/file.pem": "cert data"},
    )

    config: dict[str, Any] = {
        "level1": {"level2": [{"level3": config_dir / "deep" / "file.pem"}]}
    }
    creator = ConfigBundleCreator(config)
    files = creator.discover_files()

    paths = [f.path for f in files]
    assert "deep/file.pem" in paths


def test_discover_files_idempotent_secrets(tmp_path: Path) -> None:
    """Calling discover_files twice does not accumulate secrets paths."""
    config_dir = _setup_config_dir(tmp_path)
    (config_dir / "secrets.yaml").write_text("k: v\n")
    (config_dir / "test.yaml").write_text("a: !secret k\n")

    creator = ConfigBundleCreator({})
    files1 = creator.discover_files()
    files2 = creator.discover_files()

    # Both calls should return the same result (secrets not accumulated)
    paths1 = [f.path for f in files1]
    paths2 = [f.path for f in files2]
    assert "secrets.yaml" in paths1
    assert paths1 == paths2


def test_discover_files_skips_missing_file(tmp_path: Path) -> None:
    """_add_file logs warning for non-existent files via includes."""
    _setup_config_dir(tmp_path)

    # Include references a file that doesn't exist on disk
    config: dict[str, Any] = {
        "esphome": {"includes": ["nonexistent.h"]},
    }
    creator = ConfigBundleCreator(config)
    files = creator.discover_files()

    paths = [f.path for f in files]
    assert "nonexistent.h" not in paths


def test_discover_files_skips_missing_directory(tmp_path: Path) -> None:
    """_add_directory logs warning for non-existent directories."""
    _setup_config_dir(tmp_path)

    config: dict[str, Any] = {
        "external_components": [
            {"source": {"type": "local", "path": "nonexistent_dir"}}
        ],
    }
    creator = ConfigBundleCreator(config)
    files = creator.discover_files()

    # Only the config file
    assert len(files) == 1


def test_discover_files_nested_include(tmp_path: Path) -> None:
    """Nested !include files (e.g. wifi: !include wifi.yaml) are bundled."""
    config_dir = _setup_config_dir(tmp_path)
    (config_dir / "test.yaml").write_text(
        "esphome:\n  name: test\nwifi: !include wifi.yaml\n"
    )
    (config_dir / "wifi.yaml").write_text('ssid: "a"\npassword: "b"\n')

    creator = ConfigBundleCreator({})
    files = creator.discover_files()

    paths = [f.path for f in files]
    assert "test.yaml" in paths
    assert "wifi.yaml" in paths


def test_discover_files_deeply_nested_include(tmp_path: Path) -> None:
    """Chains of !include (a includes b includes c) are fully resolved."""
    config_dir = _setup_config_dir(tmp_path)
    (config_dir / "test.yaml").write_text(
        "esphome:\n  name: test\nwifi: !include level1.yaml\n"
    )
    (config_dir / "level1.yaml").write_text("nested: !include level2.yaml\n")
    (config_dir / "level2.yaml").write_text('value: "leaf"\n')

    creator = ConfigBundleCreator({})
    files = creator.discover_files()

    paths = [f.path for f in files]
    assert "level1.yaml" in paths
    assert "level2.yaml" in paths


def test_discover_files_nested_include_unresolved_substitution(
    tmp_path: Path,
) -> None:
    """!include with substitution vars in path cannot be resolved; skipped gracefully."""
    config_dir = _setup_config_dir(tmp_path)
    (config_dir / "test.yaml").write_text(
        "esphome:\n  name: test\nwifi: !include ${platform}.yaml\n"
    )

    creator = ConfigBundleCreator({})
    # Should not raise
    files = creator.discover_files()

    paths = [f.path for f in files]
    assert "test.yaml" in paths


def test_discover_files_nested_include_load_failure(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A nested !include pointing at a missing file is logged and skipped."""
    config_dir = _setup_config_dir(tmp_path)
    (config_dir / "test.yaml").write_text(
        "esphome:\n  name: test\nwifi: !include missing.yaml\n"
    )

    creator = ConfigBundleCreator({})
    files = creator.discover_files()

    paths = [f.path for f in files]
    assert "test.yaml" in paths
    assert any(
        "failed to load !include" in r.message and "missing.yaml" in r.message
        for r in caplog.records
    )


def test_force_load_skips_duplicate_include_file() -> None:
    """The same IncludeFile referenced twice is only loaded once."""

    class _StubInclude:
        """Mimics yaml_util.IncludeFile minimally for _force_load testing."""

        def __init__(self) -> None:
            self.file = Path("dup.yaml")
            self.parent_file = Path("root.yaml")
            self.load_calls = 0

        def has_unresolved_expressions(self) -> bool:
            return False

        def load(self) -> dict[str, Any]:
            self.load_calls += 1
            return {}

    stub = _StubInclude()
    # Same instance appears twice — second visit must hit the _seen guard.
    tree = {"a": stub, "b": [stub]}

    with patch("esphome.bundle.yaml_util.IncludeFile", _StubInclude):
        _force_load_include_files(tree)

    assert stub.load_calls == 1


def test_force_load_handles_cyclic_containers() -> None:
    """Cyclic dict/list references don't cause infinite recursion."""
    cyclic_dict: dict[str, Any] = {}
    cyclic_dict["self"] = cyclic_dict

    cyclic_list: list[Any] = []
    cyclic_list.append(cyclic_list)

    # Should return without recursing forever
    _force_load_include_files(cyclic_dict)
    _force_load_include_files(cyclic_list)


def test_discover_files_yaml_reload_failure(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """YAML reload failure during include discovery is handled gracefully."""
    _setup_config_dir(tmp_path)

    def _raise_error(*args, **kwargs):
        raise EsphomeError("parse error")

    monkeypatch.setattr("esphome.yaml_util.load_yaml", _raise_error)

    creator = ConfigBundleCreator({})
    files = creator.discover_files()

    # Should still have the config file at minimum
    paths = [f.path for f in files]
    assert "test.yaml" in paths


def test_discover_files_esphome_includes_c(tmp_path: Path) -> None:
    """Paths listed in esphome.includes_c are discovered."""
    _setup_config_dir(
        tmp_path,
        files={"my_code.c": "// c code"},
    )

    config: dict[str, Any] = {
        "esphome": {"includes_c": ["my_code.c"]},
    }
    creator = ConfigBundleCreator(config)
    files = creator.discover_files()

    paths = [f.path for f in files]
    assert "my_code.c" in paths


def test_discover_files_external_components_non_local_type(tmp_path: Path) -> None:
    """external_components with type != 'local' are skipped."""
    _setup_config_dir(tmp_path)

    config: dict[str, Any] = {
        "external_components": [
            {"source": {"type": "git", "url": "https://github.com/user/repo"}}
        ],
    }
    creator = ConfigBundleCreator(config)
    files = creator.discover_files()

    assert len(files) == 1


def test_discover_files_external_components_no_path(tmp_path: Path) -> None:
    """external_components with local type but missing path are skipped."""
    _setup_config_dir(tmp_path)

    config: dict[str, Any] = {
        "external_components": [{"source": {"type": "local"}}],
    }
    creator = ConfigBundleCreator(config)
    files = creator.discover_files()

    assert len(files) == 1


def test_discover_files_external_components_absolute_path(tmp_path: Path) -> None:
    """external_components with absolute path are resolved correctly."""
    config_dir = _setup_config_dir(
        tmp_path,
        files={"ext/comp/__init__.py": "# comp"},
    )

    abs_path = str(config_dir / "ext")
    config: dict[str, Any] = {
        "external_components": [{"source": {"type": "local", "path": abs_path}}],
    }
    creator = ConfigBundleCreator(config)
    files = creator.discover_files()

    paths = [f.path for f in files]
    assert "ext/comp/__init__.py" in paths


def test_discover_files_relative_string_with_known_extension(tmp_path: Path) -> None:
    """Relative strings with known extensions are resolved and warned."""
    _setup_config_dir(
        tmp_path,
        files={"my_cert.pem": "cert data"},
    )

    config: dict[str, Any] = {
        "component": {"cert": "my_cert.pem"},
    }
    creator = ConfigBundleCreator(config)
    files = creator.discover_files()

    paths = [f.path for f in files]
    assert "my_cert.pem" in paths


def test_discover_files_relative_string_missing_file(tmp_path: Path) -> None:
    """Relative strings with known extensions that don't exist are skipped."""
    _setup_config_dir(tmp_path)

    config: dict[str, Any] = {
        "component": {"cert": "nonexistent.pem"},
    }
    creator = ConfigBundleCreator(config)
    files = creator.discover_files()

    assert len(files) == 1


def test_discover_files_esphome_includes_absolute_path(tmp_path: Path) -> None:
    """esphome.includes with absolute path is handled."""
    config_dir = _setup_config_dir(
        tmp_path,
        files={"my_code.h": "#pragma once"},
    )

    config: dict[str, Any] = {
        "esphome": {"includes": [str(config_dir / "my_code.h")]},
    }
    creator = ConfigBundleCreator(config)
    files = creator.discover_files()

    paths = [f.path for f in files]
    assert "my_code.h" in paths


def test_discover_files_walk_tuple_values(tmp_path: Path) -> None:
    """Tuples in config are walked like lists."""
    config_dir = _setup_config_dir(
        tmp_path,
        files={"a.pem": "cert"},
    )

    config: dict[str, Any] = {
        "items": (config_dir / "a.pem",),
    }
    creator = ConfigBundleCreator(config)
    files = creator.discover_files()

    paths = [f.path for f in files]
    assert "a.pem" in paths


# ---------------------------------------------------------------------------
# ConfigBundleCreator - fixture-based end-to-end
# ---------------------------------------------------------------------------


def test_discover_files_fixture_config(fixture_path: Path, tmp_path: Path) -> None:
    """Use the real ``fixtures/bundle/`` tree as an end-to-end reproducer.

    The fixture config uses ``wifi: !include common/wifi.yaml`` — a plain
    nested !include that is returned as a deferred ``IncludeFile`` and only
    resolved during the substitution pass. Before this fix, bundle discovery
    never ran substitutions, so ``common/wifi.yaml`` was silently missing
    from the bundle.
    """
    # Copy the fixture tree into a tmp dir so the test doesn't rely on the
    # source repo being writable and so we can set CORE.config_path freely.
    src = fixture_path / "bundle"
    dst = tmp_path / "bundle"
    shutil.copytree(src, dst)

    CORE.config_path = dst / "bundle_test.yaml"

    creator = ConfigBundleCreator({})
    files = creator.discover_files()
    paths = {f.path for f in files}

    # Root and top-level !secret-referenced files
    assert "bundle_test.yaml" in paths
    assert "secrets.yaml" in paths
    # The nested !include — this is what regressed when IncludeFile became
    # deferred (PR #12213).
    assert "common/wifi.yaml" in paths


# ---------------------------------------------------------------------------
# ConfigBundleCreator - create_bundle
# ---------------------------------------------------------------------------


def test_create_bundle_produces_valid_archive(tmp_path: Path) -> None:
    _setup_config_dir(tmp_path)

    creator = ConfigBundleCreator({})
    result = creator.create_bundle()

    assert isinstance(result.data, bytes)
    assert len(result.data) > 0

    # Verify it's a valid tar.gz
    buf = io.BytesIO(result.data)
    with tarfile.open(fileobj=buf, mode="r:gz") as tar:
        names = tar.getnames()
        assert MANIFEST_FILENAME in names
        assert "test.yaml" in names


def test_create_bundle_manifest_content(tmp_path: Path) -> None:
    _setup_config_dir(tmp_path)

    creator = ConfigBundleCreator({})
    result = creator.create_bundle()

    manifest = result.manifest
    assert manifest[ManifestKey.MANIFEST_VERSION] == CURRENT_MANIFEST_VERSION
    assert manifest[ManifestKey.CONFIG_FILENAME] == "test.yaml"
    assert "test.yaml" in manifest[ManifestKey.FILES]


def test_create_bundle_filters_secrets(tmp_path: Path) -> None:
    config_dir = _setup_config_dir(tmp_path)

    # Create secrets.yaml with multiple secrets
    secrets = config_dir / "secrets.yaml"
    secrets.write_text(
        "wifi_ssid: MyNetwork\nwifi_pw: secret123\nunused: should_not_appear\n"
    )

    # Config that references only some secrets
    config_yaml = "wifi:\n  ssid: !secret wifi_ssid\n  password: !secret wifi_pw\n"
    (config_dir / "test.yaml").write_text(config_yaml)

    creator = ConfigBundleCreator({})
    result = creator.create_bundle()

    # Extract and check secrets
    buf = io.BytesIO(result.data)
    with tarfile.open(fileobj=buf, mode="r:gz") as tar:
        secrets_data = tar.extractfile("secrets.yaml").read().decode()

    assert "wifi_ssid" in secrets_data
    assert "wifi_pw" in secrets_data
    assert "unused" not in secrets_data
    assert "should_not_appear" not in secrets_data


def test_create_bundle_filters_secrets_quoted(tmp_path: Path) -> None:
    """Bundling must include secrets.yaml when !secret keys are quoted.

    Regression test for issue 16259: quoted !secret references previously
    captured the quotes as part of the key, so no key matched secrets.yaml
    entries and the secrets file was dropped from the bundle entirely.
    """
    config_dir = _setup_config_dir(tmp_path)

    secrets = config_dir / "secrets.yaml"
    secrets.write_text("ota_password: hunter2\nunused: should_not_appear\n")

    config_yaml = "ota:\n  password: !secret 'ota_password'\n"
    (config_dir / "test.yaml").write_text(config_yaml)

    creator = ConfigBundleCreator({})
    result = creator.create_bundle()

    assert result.manifest[ManifestKey.HAS_SECRETS] is True

    buf = io.BytesIO(result.data)
    with tarfile.open(fileobj=buf, mode="r:gz") as tar:
        secrets_data = tar.extractfile("secrets.yaml").read().decode()

    assert "ota_password" in secrets_data
    assert "hunter2" in secrets_data
    assert "unused" not in secrets_data


def test_create_bundle_no_secrets(tmp_path: Path) -> None:
    _setup_config_dir(tmp_path)

    creator = ConfigBundleCreator({})
    result = creator.create_bundle()

    assert result.manifest[ManifestKey.HAS_SECRETS] is False


def test_create_bundle_secrets_load_failure(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Secrets file that fails to load during filtering is skipped gracefully."""
    config_dir = _setup_config_dir(tmp_path)
    (config_dir / "secrets.yaml").write_text("k: v\n")
    (config_dir / "test.yaml").write_text("a: !secret k\n")

    from esphome import yaml_util as yu

    original_load = yu.load_yaml

    def _failing_on_filter(fname, *args, clear_secrets=True, **kwargs):
        # Fail only when _build_filtered_secrets calls with clear_secrets=False
        if not clear_secrets and "secrets" in str(fname):
            raise EsphomeError("corrupt secrets")
        return original_load(fname, *args, clear_secrets=clear_secrets, **kwargs)

    monkeypatch.setattr(yu, "load_yaml", _failing_on_filter)

    creator = ConfigBundleCreator({})
    result = creator.create_bundle()

    # Should succeed without secrets since the filtered load failed
    assert result.manifest[ManifestKey.HAS_SECRETS] is False


def test_create_bundle_secrets_non_dict(tmp_path: Path) -> None:
    """Secrets file that parses to non-dict is skipped."""
    config_dir = _setup_config_dir(tmp_path)
    (config_dir / "secrets.yaml").write_text("- item1\n- item2\n")
    (config_dir / "test.yaml").write_text("a: !secret k\n")

    creator = ConfigBundleCreator({})
    result = creator.create_bundle()

    assert result.manifest[ManifestKey.HAS_SECRETS] is False


def test_create_bundle_secrets_no_matching_keys(tmp_path: Path) -> None:
    """Secrets with no matching keys produces empty filtered result."""
    config_dir = _setup_config_dir(tmp_path)
    (config_dir / "secrets.yaml").write_text("other_key: value\n")
    (config_dir / "test.yaml").write_text("a: !secret nonexistent\n")

    creator = ConfigBundleCreator({})
    result = creator.create_bundle()

    assert result.manifest[ManifestKey.HAS_SECRETS] is False


def test_create_bundle_deterministic_order(tmp_path: Path) -> None:
    """Files are added in sorted order for reproducibility."""
    _setup_config_dir(
        tmp_path,
        files={
            "z_last.h": "// z",
            "a_first.h": "// a",
            "m_middle.h": "// m",
        },
    )

    config: dict[str, Any] = {
        "esphome": {"includes": ["z_last.h", "a_first.h", "m_middle.h"]},
    }
    creator = ConfigBundleCreator(config)
    result = creator.create_bundle()

    buf = io.BytesIO(result.data)
    with tarfile.open(fileobj=buf, mode="r:gz") as tar:
        names = tar.getnames()

    # manifest.json is always first, then files in sorted order
    assert names[0] == MANIFEST_FILENAME
    file_names = [n for n in names if n != MANIFEST_FILENAME]
    assert file_names == sorted(file_names)


# ---------------------------------------------------------------------------
# Round-trip: create then extract
# ---------------------------------------------------------------------------


def test_bundle_round_trip(tmp_path: Path) -> None:
    """A bundle created by ConfigBundleCreator can be extracted."""
    _setup_config_dir(
        tmp_path,
        files={"include.h": "#pragma once\n"},
    )
    config: dict[str, Any] = {"esphome": {"includes": ["include.h"]}}

    creator = ConfigBundleCreator(config)
    result = creator.create_bundle()

    bundle_path = tmp_path / f"roundtrip{BUNDLE_EXTENSION}"
    bundle_path.write_bytes(result.data)

    target = tmp_path / "extracted"
    config_path = extract_bundle(bundle_path, target)

    assert config_path.is_file()
    assert (target / "include.h").read_text() == "#pragma once\n"

    manifest = read_bundle_manifest(bundle_path)
    assert manifest.config_filename == "test.yaml"
    assert "include.h" in manifest.files


def test_bundle_round_trip_with_secrets(tmp_path: Path) -> None:
    """Secrets survive round-trip with correct filtering."""
    config_dir = _setup_config_dir(tmp_path)
    (config_dir / "secrets.yaml").write_text("key1: val1\nkey2: val2\nunused: nope\n")
    (config_dir / "test.yaml").write_text("a: !secret key1\nb: !secret key2\n")

    creator = ConfigBundleCreator({})
    result = creator.create_bundle()

    bundle_path = tmp_path / f"secrets{BUNDLE_EXTENSION}"
    bundle_path.write_bytes(result.data)

    target = tmp_path / "extracted"
    extract_bundle(bundle_path, target)

    secrets_content = (target / "secrets.yaml").read_text()
    assert "key1" in secrets_content
    assert "key2" in secrets_content
    assert "unused" not in secrets_content

    manifest = read_bundle_manifest(bundle_path)
    assert manifest.has_secrets is True
