"""Tests for the store_yaml component's file gathering, secret redaction, and
envelope packing."""

from __future__ import annotations

from pathlib import Path

import pytest

from esphome import yaml_util
from esphome.components.store_yaml import (
    SECRETS_SKELETON_HEADER,
    _gather_files,
    _generate_redacted_files,
    _pack_envelope,
    unpack_envelope,
)
from esphome.core import CORE, EsphomeError
from esphome.yaml_util import DiscoveredYamlFiles, SensitiveStr


@pytest.fixture
def project(tmp_path: Path) -> Path:
    """Lay out a tiny ESPHome-like project: entry yaml, an include, and a secrets file."""
    project_dir = tmp_path / "project"
    project_dir.mkdir()
    (project_dir / "entry.yaml").write_text(
        "esphome:\n  name: test\napi:\n  encryption:\n    key: !secret api_key\n"
    )
    (project_dir / "wifi.yaml").write_text("ssid: my_ssid\npassword: my_password\n")
    (project_dir / "secrets.yaml").write_text("api_key: SUPER_SECRET\n")
    return project_dir


@pytest.fixture(autouse=True)
def _clear_config() -> None:
    CORE.config = {}
    yield
    yaml_util._SECRET_VALUES.clear()


def _sources(
    project_dir: Path, *names: str, secrets: tuple[str, ...] = ()
) -> DiscoveredYamlFiles:
    CORE.config_path = project_dir / "entry.yaml"
    files = [project_dir / name for name in names]
    secret_paths = {(project_dir / name).resolve() for name in secrets}
    return DiscoveredYamlFiles(files, secret_paths)


def _gather_redacted(discovered: DiscoveredYamlFiles) -> dict[str, bytes]:
    files, secret_rels = _gather_files(discovered)
    return dict(_generate_redacted_files(files, secret_rels))


# ---------------------------------------------------------------------------
# _gather_files
# ---------------------------------------------------------------------------


def test_gather_returns_verbatim_content_and_flags_secrets(project: Path) -> None:
    discovered = _sources(
        project, "entry.yaml", "secrets.yaml", secrets=("secrets.yaml",)
    )
    files, secret_rels = _gather_files(discovered)
    contents = dict(files)
    assert contents["secrets.yaml"] == b"api_key: SUPER_SECRET\n"
    assert secret_rels == {"secrets.yaml"}


def test_gather_flags_secret_symlinked_to_other_name(
    project: Path, tmp_path: Path
) -> None:
    """A `secrets.yaml` symlinked to a non-secrets-named target is still flagged
    because the un-resolved basename was captured upstream."""
    target = tmp_path / "actual_creds.yaml"
    target.write_text("api_key: FROM_SYMLINK\n")
    link = project / "secrets.yaml"
    link.unlink()  # remove the regular file laid down by the fixture
    link.symlink_to(target)
    # Discovery records the un-resolved listener fname under SECRETS_FILES
    # but stores the resolved path; mimic that here.
    resolved = link.resolve()
    CORE.config_path = project / "entry.yaml"
    files = _gather_redacted(DiscoveredYamlFiles([resolved], {resolved}))
    assert b"FROM_SYMLINK" not in b"".join(files.values())


def test_gather_uses_relative_path_for_external_files(
    project: Path, tmp_path: Path
) -> None:
    """Files outside the project root use a ``..``-style relative path so they don't collide."""
    sibling = tmp_path / "outside.yaml"
    sibling.write_text("foo: bar\n")
    CORE.config_path = project / "entry.yaml"
    discovered = DiscoveredYamlFiles([project / "entry.yaml", sibling], set())
    files, _ = _gather_files(discovered)
    # project root is `tmp_path/project`, sibling is in `tmp_path` so it
    # resolves to `../outside.yaml`.
    assert "../outside.yaml" in dict(files)


def test_gather_raises_when_no_sources(project: Path) -> None:
    CORE.config_path = project / "entry.yaml"
    with pytest.raises(EsphomeError):
        _gather_files(DiscoveredYamlFiles())


def test_gather_raises_on_load_errors(project: Path) -> None:
    """A failed include load during discovery fails the build instead of
    embedding an incomplete recovery bundle."""
    CORE.config_path = project / "entry.yaml"
    discovered = DiscoveredYamlFiles(
        [project / "entry.yaml"], set(), load_errors=["oops.yaml: boom"]
    )
    with pytest.raises(EsphomeError, match="oops.yaml"):
        _gather_files(discovered)


def test_gather_raises_on_unreadable_file(
    project: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """An unreadable tracked file fails the build instead of producing a
    silently partial recovery blob."""
    discovered = _sources(project, "entry.yaml", "wifi.yaml")
    orig_read_bytes = Path.read_bytes

    def fake_read_bytes(self: Path) -> bytes:
        if self.name == "wifi.yaml":
            raise OSError("permission denied")
        return orig_read_bytes(self)

    monkeypatch.setattr(Path, "read_bytes", fake_read_bytes)
    with pytest.raises(EsphomeError, match="wifi.yaml"):
        _gather_files(discovered)


def test_gather_warns_on_unresolved_includes(
    project: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """Substitution-pathed includes that discovery could not capture produce a
    warning naming them, so the user knows the blob is incomplete."""
    CORE.config_path = project / "entry.yaml"
    discovered = DiscoveredYamlFiles([project / "entry.yaml"], set(), ["${board}.yaml"])
    with caplog.at_level("WARNING", logger="esphome.components.store_yaml"):
        files, _ = _gather_files(discovered)
    assert len(files) == 1
    assert any(
        "${board}.yaml" in r.message and "not contain" in r.message
        for r in caplog.records
    )


# ---------------------------------------------------------------------------
# _generate_redacted_files
# ---------------------------------------------------------------------------


def test_redacted_secrets_file_becomes_skeleton(project: Path) -> None:
    """The secrets file is replaced by a fill-in skeleton listing every
    referenced `!secret` key, so the recovered config is flashable."""
    discovered = _sources(
        project, "entry.yaml", "secrets.yaml", secrets=("secrets.yaml",)
    )
    files = _gather_redacted(discovered)
    skeleton = files["secrets.yaml"].decode()
    assert skeleton.startswith(SECRETS_SKELETON_HEADER)
    assert 'api_key: ""' in skeleton
    assert b"SUPER_SECRET" not in files["secrets.yaml"]
    # The entry's own `!secret` reference is re-emitted as a reference.
    assert "key: !secret 'api_key'" in files["entry.yaml"].decode()


def test_redacted_inline_sensitive_value_becomes_secret_ref(project: Path) -> None:
    """An inline cv.sensitive value is generated as `!secret <path-derived
    name>` and lands in the skeleton."""
    CORE.config = {"wifi": [{"password": SensitiveStr("my_password")}]}
    discovered = _sources(
        project, "wifi.yaml", "secrets.yaml", secrets=("secrets.yaml",)
    )
    files = _gather_redacted(discovered)
    text = files["wifi.yaml"].decode()
    assert "my_password" not in text
    assert "password: !secret 'wifi_password'" in text
    assert 'wifi_password: ""' in files["secrets.yaml"].decode()


@pytest.mark.parametrize("quote", ['"', "'"])
def test_redacted_quoted_inline_value(project: Path, quote: str) -> None:
    """Quoting in the source doesn't matter — the swap happens on the parsed
    scalar, not the text."""
    (project / "wifi.yaml").write_text(f"password: {quote}my_password{quote}\n")
    CORE.config = {"wifi": [{"password": SensitiveStr("my_password")}]}
    discovered = _sources(project, "wifi.yaml")
    files = _gather_redacted(discovered)
    assert files["wifi.yaml"] == b"password: !secret 'wifi_password'\n"


def test_redacted_swap_is_whole_scalar_and_value_keyed(project: Path) -> None:
    """Every whole scalar equal to the sensitive value is swapped (value-keyed,
    like `!secret` itself); substrings inside other scalars are never touched.
    The recovered config stays semantically identical once the secret is filled."""
    (project / "wifi.yaml").write_text(
        "platform: esp32\nnote: esp32 is great\npassword: esp32\n"
    )
    CORE.config = {"wifi": [{"password": SensitiveStr("esp32")}]}
    discovered = _sources(project, "wifi.yaml")
    files = _gather_redacted(discovered)
    text = files["wifi.yaml"].decode()
    assert "password: !secret 'wifi_password'" in text
    assert "platform: !secret 'wifi_password'" in text
    assert "note: esp32 is great" in text


def test_redacted_include_reference_round_trips(project: Path) -> None:
    """A nested `!include` stays a reference in the generated file."""
    (project / "entry.yaml").write_text(
        "esphome:\n  name: test\nwifi: !include wifi.yaml\n"
    )
    discovered = _sources(project, "entry.yaml", "wifi.yaml")
    files = _gather_redacted(discovered)
    assert "wifi: !include 'wifi.yaml'" in files["entry.yaml"].decode()


def test_redacted_reuses_existing_secret_name_for_duplicated_value(
    project: Path,
) -> None:
    """A value that comes from `!secret` somewhere but is ALSO written inline
    elsewhere is generated with the existing secret name."""
    (project / "wifi.yaml").write_text("password: SUPER_SECRET\n")
    CORE.config = {"wifi": [{"password": SensitiveStr("SUPER_SECRET")}]}
    yaml_util._SECRET_VALUES["SUPER_SECRET"] = "api_key"
    discovered = _sources(
        project, "wifi.yaml", "secrets.yaml", secrets=("secrets.yaml",)
    )
    files = _gather_redacted(discovered)
    assert files["wifi.yaml"] == b"password: !secret 'api_key'\n"


def test_redacted_warns_when_value_not_locatable(
    project: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A sensitive value that never appears as a whole scalar (e.g. composed
    via substitutions) produces a warning naming the config path, not the value."""
    CORE.config = {"wifi": [{"password": SensitiveStr("not_in_any_file")}]}
    discovered = _sources(project, "wifi.yaml")
    with caplog.at_level("WARNING", logger="esphome.components.store_yaml"):
        _gather_redacted(discovered)
    assert any(
        "wifi.password" in r.message and "could not locate" in r.message
        for r in caplog.records
    )
    assert not any("not_in_any_file" in r.message for r in caplog.records)


def test_redacted_does_not_warn_for_secret_only_values(
    project: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A value that only exists via `!secret` legitimately never appears inline."""
    CORE.config = {"api": {"encryption": {"key": SensitiveStr("SUPER_SECRET")}}}
    yaml_util._SECRET_VALUES["SUPER_SECRET"] = "api_key"
    discovered = _sources(
        project, "entry.yaml", "secrets.yaml", secrets=("secrets.yaml",)
    )
    with caplog.at_level("WARNING", logger="esphome.components.store_yaml"):
        _gather_redacted(discovered)
    assert not any("could not locate" in r.message for r in caplog.records)


def test_redacted_skips_empty_sensitive_values(project: Path) -> None:
    """Empty defaults (e.g. mqtt password) are never swapped."""
    (project / "wifi.yaml").write_text("ssid: my_ssid\n")
    CORE.config = {"mqtt": {"password": SensitiveStr("")}}
    discovered = _sources(project, "wifi.yaml")
    files = _gather_redacted(discovered)
    assert files["wifi.yaml"] == b"ssid: my_ssid\n"


def test_redacted_adds_synthetic_secrets_file_when_none_captured(
    project: Path,
) -> None:
    """Inline secrets in a project without a secrets.yaml still produce a
    skeleton so the recovered config is complete."""
    CORE.config = {"wifi": [{"password": SensitiveStr("my_password")}]}
    discovered = _sources(project, "wifi.yaml")
    files = _gather_redacted(discovered)
    assert 'wifi_password: ""' in files["secrets.yaml"].decode()


def test_redacted_generates_unique_names_on_collision(project: Path) -> None:
    """Two different inline values whose paths collide get distinct names."""
    (project / "wifi.yaml").write_text("password: first_pw\n")
    (project / "wifi2.yaml").write_text("password: second_pw\n")
    CORE.config = {
        "wifi": [
            {"password": SensitiveStr("first_pw")},
            {"password": SensitiveStr("second_pw")},
        ]
    }
    discovered = _sources(project, "wifi.yaml", "wifi2.yaml")
    files = _gather_redacted(discovered)
    assert files["wifi.yaml"] == b"password: !secret 'wifi_password'\n"
    assert files["wifi2.yaml"] == b"password: !secret 'wifi_password_2'\n"


# ---------------------------------------------------------------------------
# envelope pack/unpack
# ---------------------------------------------------------------------------


def test_pack_envelope_roundtrip() -> None:
    files = [
        ("entry.yaml", b"esphome:\n  name: test\n"),
        ("wifi.yaml", b"ssid: a\n"),
    ]
    blob = _pack_envelope(files)
    assert unpack_envelope(blob) == dict(files)


def test_pack_envelope_handles_utf8_paths() -> None:
    files = [("dossiers/maison.yaml", b"foo: bar\n")]
    blob = _pack_envelope(files)
    assert unpack_envelope(blob) == dict(files)


def test_pack_envelope_rejects_overlong_path() -> None:
    long_path = "a" * (0xFFFF + 1)
    with pytest.raises(EsphomeError):
        _pack_envelope([(long_path, b"")])


def test_unpack_envelope_rejects_bad_magic() -> None:
    with pytest.raises(EsphomeError):
        unpack_envelope(b"NOPE" + b"\x00" * 4)


@pytest.mark.parametrize("cut", [5, 9, 12, -1])
def test_unpack_envelope_rejects_truncated_input(cut: int) -> None:
    blob = _pack_envelope([("entry.yaml", b"esphome:\n")])
    with pytest.raises(EsphomeError, match="truncated"):
        unpack_envelope(blob[:cut])


def test_unpack_envelope_rejects_trailing_bytes() -> None:
    blob = _pack_envelope([("entry.yaml", b"esphome:\n")])
    with pytest.raises(EsphomeError, match="trailing"):
        unpack_envelope(blob + b"\x00")
