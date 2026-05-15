"""Tests for the store_yaml component's file gathering and envelope packing."""

from __future__ import annotations

from pathlib import Path
import struct

import pytest

from esphome.components.store_yaml import (
    ENVELOPE_MAGIC,
    REDACTED_PLACEHOLDER,
    _gather_files,
    _pack_envelope,
)
from esphome.core import CORE, EsphomeError


def _unpack_envelope(blob: bytes) -> dict[str, bytes]:
    """Inverse of `_pack_envelope` for assertions in tests."""
    assert blob[:4] == ENVELOPE_MAGIC, "envelope must start with EHY1 magic"
    pos = 4
    (count,) = struct.unpack_from("<I", blob, pos)
    pos += 4
    files: dict[str, bytes] = {}
    for _ in range(count):
        (path_len,) = struct.unpack_from("<H", blob, pos)
        pos += 2
        path = blob[pos : pos + path_len].decode("utf-8")
        pos += path_len
        (content_len,) = struct.unpack_from("<I", blob, pos)
        pos += 4
        content = blob[pos : pos + content_len]
        pos += content_len
        files[path] = content
    assert pos == len(blob), "envelope must consume all bytes"
    return files


@pytest.fixture
def project(tmp_path: Path) -> Path:
    """Lay out a tiny ESPHome-like project: entry yaml, an include, and a secrets file."""
    project_dir = tmp_path / "project"
    project_dir.mkdir()
    (project_dir / "entry.yaml").write_text("esphome:\n  name: test\n")
    (project_dir / "wifi.yaml").write_text("ssid: my_ssid\npassword: my_password\n")
    (project_dir / "secrets.yaml").write_text("api_key: SUPER_SECRET\n")
    return project_dir


@pytest.fixture(autouse=True)
def _reset_core() -> None:
    CORE.data.pop("yaml_sources", None)
    CORE.config_path = None
    yield
    CORE.data.pop("yaml_sources", None)
    CORE.config_path = None


def _set_sources(project_dir: Path, *names: str) -> None:
    CORE.config_path = project_dir / "entry.yaml"
    CORE.data["yaml_sources"] = [project_dir / name for name in names]


def test_gather_redacts_secrets_by_default(project: Path) -> None:
    _set_sources(project, "entry.yaml", "wifi.yaml", "secrets.yaml")
    files = dict(_gather_files(include_secrets=False))
    assert files["secrets.yaml"] == REDACTED_PLACEHOLDER
    assert b"SUPER_SECRET" not in files["secrets.yaml"]
    assert files["wifi.yaml"] == (project / "wifi.yaml").read_bytes()


def test_gather_redacts_yml_extension(project: Path) -> None:
    yml = project / "secrets.yml"
    yml.write_text("api_key: OTHER_SECRET\n")
    _set_sources(project, "entry.yaml", "secrets.yml")
    files = dict(_gather_files(include_secrets=False))
    assert files["secrets.yml"] == REDACTED_PLACEHOLDER


def test_gather_embeds_secrets_when_opted_in(project: Path) -> None:
    _set_sources(project, "entry.yaml", "secrets.yaml")
    files = dict(_gather_files(include_secrets=True))
    assert b"SUPER_SECRET" in files["secrets.yaml"]


def test_gather_uses_relative_path_for_external_files(
    project: Path, tmp_path: Path
) -> None:
    """Files outside the project root use a ``..``-style relative path so they don't collide."""
    sibling = tmp_path / "outside.yaml"
    sibling.write_text("foo: bar\n")
    _set_sources(project, "entry.yaml")
    CORE.data["yaml_sources"].append(sibling)
    files = dict(_gather_files(include_secrets=False))
    # project root is `tmp_path/project`, sibling is in `tmp_path` so it
    # resolves to `../outside.yaml`.
    assert "../outside.yaml" in files


def test_gather_deduplicates(project: Path) -> None:
    _set_sources(project, "entry.yaml", "wifi.yaml", "wifi.yaml")
    files = _gather_files(include_secrets=False)
    paths = [p for p, _ in files]
    assert paths.count("wifi.yaml") == 1


def test_gather_raises_when_no_sources(project: Path) -> None:
    CORE.config_path = project / "entry.yaml"
    with pytest.raises(EsphomeError):
        _gather_files(include_secrets=False)


def test_pack_envelope_roundtrip() -> None:
    files = [
        ("entry.yaml", b"esphome:\n  name: test\n"),
        ("wifi.yaml", b"ssid: a\n"),
    ]
    blob = _pack_envelope(files)
    assert _unpack_envelope(blob) == dict(files)


def test_pack_envelope_handles_utf8_paths() -> None:
    files = [("dossiers/maison.yaml", b"foo: bar\n")]
    blob = _pack_envelope(files)
    assert _unpack_envelope(blob) == dict(files)


def test_pack_envelope_rejects_overlong_path() -> None:
    long_path = "a" * (0xFFFF + 1)
    with pytest.raises(EsphomeError):
        _pack_envelope([(long_path, b"")])
