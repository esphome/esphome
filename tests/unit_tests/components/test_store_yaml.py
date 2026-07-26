"""Tests for the store_yaml component's file gathering, secret redaction, and
envelope packing."""

from __future__ import annotations

from pathlib import Path

import pytest

from esphome import yaml_util
from esphome.components import packages
from esphome.components.store_yaml import (
    CONF_ALLOW_UNENCRYPTED,
    SECRETS_SKELETON_HEADER,
    UNCAPTURED_NOTE_PATH,
    _final_validate,
    _gather_files,
    _generate_redacted_files,
    _pack_envelope,
    _read_files_verbatim,
    _remote_package_descriptions,
    _uncaptured_note,
    unpack_envelope,
)
import esphome.config_validation as cv
from esphome.core import CORE, EsphomeError
import esphome.final_validate as fv
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


def _sources(
    project_dir: Path, *names: str, secrets: tuple[str, ...] = ()
) -> DiscoveredYamlFiles:
    CORE.config_path = project_dir / "entry.yaml"
    files = [project_dir / name for name in names]
    secret_paths = {(project_dir / name).resolve() for name in secrets}
    return DiscoveredYamlFiles(files, secret_paths)


def _gather_redacted(discovered: DiscoveredYamlFiles) -> dict[str, bytes]:
    entries, secret_rels = _gather_files(discovered)
    return dict(_generate_redacted_files(entries, secret_rels))


# ---------------------------------------------------------------------------
# _gather_files
# ---------------------------------------------------------------------------


def test_gather_maps_rel_paths_and_flags_secrets(project: Path) -> None:
    discovered = _sources(
        project, "entry.yaml", "secrets.yaml", secrets=("secrets.yaml",)
    )
    entries, secret_rels = _gather_files(discovered)
    assert dict(entries) == {
        "entry.yaml": project / "entry.yaml",
        "secrets.yaml": project / "secrets.yaml",
    }
    assert secret_rels == {"secrets.yaml"}


def test_read_files_verbatim_returns_exact_bytes(project: Path) -> None:
    """`include_secrets: true` embeds the on-disk bytes untouched."""
    discovered = _sources(
        project, "entry.yaml", "secrets.yaml", secrets=("secrets.yaml",)
    )
    entries, _ = _gather_files(discovered)
    contents = dict(_read_files_verbatim(entries))
    assert contents["secrets.yaml"] == (project / "secrets.yaml").read_bytes()
    assert contents["entry.yaml"] == (project / "entry.yaml").read_bytes()


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


def test_read_files_verbatim_raises_on_unreadable_file(
    project: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """An unreadable tracked file fails the build instead of producing a
    silently partial recovery blob."""
    discovered = _sources(project, "entry.yaml", "wifi.yaml")
    entries, _ = _gather_files(discovered)
    orig_read_bytes = Path.read_bytes

    def fake_read_bytes(self: Path) -> bytes:
        if self.name == "wifi.yaml":
            raise OSError("permission denied")
        return orig_read_bytes(self)

    monkeypatch.setattr(Path, "read_bytes", fake_read_bytes)
    with pytest.raises(EsphomeError, match="wifi.yaml"):
        _read_files_verbatim(entries)


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
    like `!secret` itself). The recovered config stays semantically identical
    once the secret is filled."""
    (project / "wifi.yaml").write_text("platform: esp32\npassword: esp32\n")
    CORE.config = {"wifi": [{"password": SensitiveStr("esp32")}]}
    discovered = _sources(project, "wifi.yaml")
    files = _gather_redacted(discovered)
    text = files["wifi.yaml"].decode()
    assert "password: !secret 'wifi_password'" in text
    assert "platform: !secret 'wifi_password'" in text


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


def test_redacted_raises_when_value_not_locatable(project: Path) -> None:
    """A sensitive value that never appears as a whole scalar (e.g. composed
    via substitutions) would ship verbatim — fail the build, naming the config
    path but never the value."""
    CORE.config = {"wifi": [{"password": SensitiveStr("not_in_any_file")}]}
    discovered = _sources(project, "wifi.yaml")
    with pytest.raises(EsphomeError, match="wifi.password") as err:
        _gather_redacted(discovered)
    assert "not_in_any_file" not in str(err.value)


def test_redacted_accepts_secret_only_values(project: Path) -> None:
    """A value that only exists via `!secret` legitimately never appears inline."""
    CORE.config = {"api": {"encryption": {"key": SensitiveStr("SUPER_SECRET")}}}
    yaml_util._SECRET_VALUES["SUPER_SECRET"] = "api_key"
    discovered = _sources(
        project, "entry.yaml", "secrets.yaml", secrets=("secrets.yaml",)
    )
    files = _gather_redacted(discovered)
    assert 'api_key: ""' in files["secrets.yaml"].decode()


def test_uncaptured_note_lists_missing_includes() -> None:
    """Substitution-pathed includes that can't be captured are recorded in a
    dedicated envelope entry (both modes), not just a compile-time log line."""
    rel, content = _uncaptured_note(["${board}.yaml"], [])
    assert rel == UNCAPTURED_NOTE_PATH
    text = content.decode()
    assert text.startswith("# store_yaml:")
    assert "#   ${board}.yaml" in text


def test_uncaptured_note_lists_remote_packages() -> None:
    """Remote packages that can't be captured are recorded with their source
    so the user knows to re-fetch them."""
    rel, content = _uncaptured_note(
        [], ["https://github.com/org/repo@main", "https://github.com/org/other"]
    )
    assert rel == UNCAPTURED_NOTE_PATH
    text = content.decode()
    assert "#   https://github.com/org/repo@main" in text
    assert "#   https://github.com/org/other" in text


def test_remote_package_descriptions_read_packages_record(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Remote sources recorded by the packages component during config
    processing are formatted as url@ref (url alone when ref is absent)."""
    monkeypatch.delitem(CORE.data, packages.DOMAIN, raising=False)
    data = packages._get_data()
    data.remote_sources.append(
        packages.RemotePackageSource("https://github.com/org/repo", "main")
    )
    data.remote_sources.append(
        packages.RemotePackageSource("https://github.com/org/other", None)
    )
    assert _remote_package_descriptions() == [
        "https://github.com/org/repo@main",
        "https://github.com/org/other",
    ]


def test_remote_package_descriptions_empty_without_packages() -> None:
    CORE.data.pop(packages.DOMAIN, None)
    assert _remote_package_descriptions() == []


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


def test_unpack_envelope_rejects_invalid_utf8_path() -> None:
    """A corrupted envelope raises EsphomeError, never a bare UnicodeDecodeError."""
    import struct

    blob = (
        b"EHY1"
        + struct.pack("<I", 1)
        + struct.pack("<H", 2)
        + b"\xff\xfe"
        + struct.pack("<I", 0)
    )
    with pytest.raises(EsphomeError, match="UTF-8"):
        unpack_envelope(blob)


def test_unpack_envelope_rejects_duplicate_paths() -> None:
    """A tampered envelope with duplicate paths must not silently drop data."""
    import struct

    entry = struct.pack("<H", 6) + b"a.yaml" + struct.pack("<I", 1) + b"x"
    blob = b"EHY1" + struct.pack("<I", 2) + entry + entry
    with pytest.raises(EsphomeError, match="duplicate"):
        unpack_envelope(blob)


def test_pack_envelope_rejects_duplicate_paths() -> None:
    """A duplicate path would silently clobber the earlier entry on unpack."""
    with pytest.raises(EsphomeError, match="duplicate"):
        _pack_envelope([("entry.yaml", b"a: 1\n"), ("entry.yaml", b"b: 2\n")])


@pytest.mark.parametrize("path", ["/etc/passwd", "\\evil.yaml", "C:/evil.yaml"])
def test_unpack_envelope_rejects_non_relative_paths(path: str) -> None:
    """The packer never emits absolute or drive-qualified paths, so their
    presence means a malformed or hostile envelope."""
    blob = _pack_envelope([(path, b"boom\n")])
    with pytest.raises(EsphomeError, match="non-relative"):
        unpack_envelope(blob)


# ---------------------------------------------------------------------------
# embedded-value leak scan and collision warning
# ---------------------------------------------------------------------------


def test_redacted_embedded_sensitive_value_fails_build(project: Path) -> None:
    """A sensitive value inside a larger scalar (URL, lambda body) is not
    swapped by the whole-scalar redaction; the substring scan fails closed."""
    (project / "wifi.yaml").write_text(
        "password: my_password\nurl: http://user:my_password@host\n"
    )
    CORE.config = {"wifi": [{"password": SensitiveStr("my_password")}]}
    discovered = _sources(project, "wifi.yaml")
    with pytest.raises(EsphomeError, match="embedded"):
        _gather_redacted(discovered)


def test_redacted_warns_on_value_collision(
    project: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """An unrelated scalar equal to a sensitive value gets rewritten by the
    value-keyed swap; a warning documents the trap. The value's own
    occurrence (under its sensitive key) does not warn."""
    (project / "wifi.yaml").write_text("password: esp32\nplatform: esp32\n")
    CORE.config = {"wifi": [{"password": SensitiveStr("esp32")}]}
    discovered = _sources(project, "wifi.yaml")
    files = _gather_redacted(discovered)
    assert files["wifi.yaml"] == (
        b"password: !secret 'wifi_password'\nplatform: !secret 'wifi_password'\n"
    )
    assert "also matches the scalar at platform in wifi.yaml" in caplog.text
    assert "at password in wifi.yaml" not in caplog.text


def test_redacted_no_warning_for_substitution_definition(
    project: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A swapped `substitutions:` definition keeps `${...}` working in the
    recovered config, so it is expected and does not warn."""
    (project / "wifi.yaml").write_text(
        "substitutions:\n  wifi_password: hunter2\nwifi:\n  password: ${wifi_password}\n"
    )
    CORE.config = {"wifi": [{"password": SensitiveStr("hunter2")}]}
    discovered = _sources(project, "wifi.yaml")
    _gather_redacted(discovered)
    assert "also matches" not in caplog.text


def test_redacted_sensitive_value_as_mapping_key_fails_build(project: Path) -> None:
    """A sensitive value equal to a mapping key would be swapped in key
    position and corrupt the recovered structure; the build fails instead."""
    (project / "wifi.yaml").write_text("password: password\n")
    CORE.config = {"ota": [{"password": SensitiveStr("password")}]}
    discovered = _sources(project, "wifi.yaml")
    with pytest.raises(EsphomeError, match="mapping key"):
        _gather_redacted(discovered)


def test_redacted_key_names_do_not_false_positive_embedded_scan(
    project: Path,
) -> None:
    """The embedded scan runs on tree scalars, not serialized text, so a
    sensitive value that is a substring of a key name (or of the generated
    `!secret` reference text) does not fail the build."""
    (project / "wifi.yaml").write_text("password: word\n")
    CORE.config = {"wifi": [{"password": SensitiveStr("word")}]}
    discovered = _sources(project, "wifi.yaml")
    files = _gather_redacted(discovered)
    assert files["wifi.yaml"] == b"password: !secret 'wifi_password'\n"


# ---------------------------------------------------------------------------
# _final_validate encryption gate
# ---------------------------------------------------------------------------


def _run_final_validate(full_config: dict, config: dict) -> dict:
    token = fv.full_config.set(full_config)
    try:
        return _final_validate(config)
    finally:
        fv.full_config.reset(token)


def test_final_validate_accepts_encrypted_api() -> None:
    config = {CONF_ALLOW_UNENCRYPTED: False}
    full = {
        "api": {"encryption": {"key": "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8="}}
    }
    assert _run_final_validate(full, config) is config


def test_final_validate_rejects_unencrypted_api() -> None:
    with pytest.raises(cv.Invalid, match="requires API encryption"):
        _run_final_validate({"api": {}}, {CONF_ALLOW_UNENCRYPTED: False})


def test_final_validate_allows_unencrypted_with_escape_hatch(
    caplog: pytest.LogCaptureFixture,
) -> None:
    config = {CONF_ALLOW_UNENCRYPTED: True}
    assert _run_final_validate({"api": {}}, config) is config
    assert "without API encryption" in caplog.text
