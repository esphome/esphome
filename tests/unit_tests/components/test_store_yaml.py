"""Tests for store_yaml file gathering, redaction and envelope packing."""

from __future__ import annotations

from pathlib import Path
import struct

import pytest
import yaml

from esphome import yaml_util
from esphome.components import packages
from esphome.components.store_yaml import (
    CONF_ALLOW_UNENCRYPTED,
    ENVELOPE_MAGIC,
    SECRETS_SKELETON_HEADER,
    UNCAPTURED_NOTE_PATH,
    _build_secrets_skeleton,
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
from esphome.yaml_util import (
    DiscoveredYamlFiles,
    SensitiveHexInt,
    SensitiveInt,
    SensitiveStr,
    discover_user_yaml_files,
)


@pytest.fixture
def project(tmp_path: Path) -> Path:
    """Lay out an entry yaml, an include and a secrets file."""
    project_dir = tmp_path / "project"
    project_dir.mkdir()
    (project_dir / "entry.yaml").write_text(
        "esphome:\n  name: test\napi:\n  encryption:\n    key: !secret api_key\n"
    )
    (project_dir / "wifi.yaml").write_text("ssid: my_ssid\npassword: my_password\n")
    (project_dir / "secrets.yaml").write_text("api_key: SUPER_SECRET\n")
    return project_dir


def _sources(
    project_dir: Path,
    *names: str,
    secrets: tuple[str, ...] = (),
    unresolved: list[str] | None = None,
    load_errors: list[str] | None = None,
) -> DiscoveredYamlFiles:
    CORE.config_path = project_dir / "entry.yaml"
    return DiscoveredYamlFiles(
        [project_dir / name for name in names],
        {(project_dir / name).resolve() for name in secrets},
        unresolved or [],
        load_errors or [],
    )


def _gather_redacted(discovered: DiscoveredYamlFiles) -> dict[str, bytes]:
    entries, secret_rels = _gather_files(discovered)
    return dict(
        _generate_redacted_files(entries, secret_rels, _remote_package_descriptions())
    )


def _run_final_validate(full_config: dict, config: dict) -> dict:
    token = fv.full_config.set(full_config)
    try:
        return _final_validate(config)
    finally:
        fv.full_config.reset(token)


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


def test_gather_uses_relative_path_for_external_files(
    project: Path, tmp_path: Path
) -> None:
    """Files outside the project root keep a ``..`` path."""
    sibling = tmp_path / "outside.yaml"
    sibling.write_text("foo: bar\n")
    discovered = _sources(project, "entry.yaml")
    discovered.files.append(sibling)
    files, _ = _gather_files(discovered)
    assert "../outside.yaml" in dict(files)


def test_gather_raises_when_no_sources(project: Path) -> None:
    with pytest.raises(EsphomeError):
        _gather_files(_sources(project))


def test_gather_raises_on_load_errors(project: Path) -> None:
    """A failed include load fails the build."""
    discovered = _sources(project, "entry.yaml", load_errors=["oops.yaml: boom"])
    with pytest.raises(EsphomeError, match="oops.yaml"):
        _gather_files(discovered)


def test_read_files_verbatim_raises_on_unreadable_file(
    project: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """An unreadable file fails the build."""
    entries, _ = _gather_files(_sources(project, "entry.yaml", "wifi.yaml"))
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
    """Substitution-pathed includes are named in a warning."""
    discovered = _sources(project, "entry.yaml", unresolved=["${board}.yaml"])
    with caplog.at_level("WARNING", logger="esphome.components.store_yaml"):
        files, _ = _gather_files(discovered)
    assert len(files) == 1
    assert any(
        "${board}.yaml" in r.message and "not contain" in r.message
        for r in caplog.records
    )


def test_gather_raises_esphome_error_on_cross_anchor_path(
    project: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """A path with no common anchor raises EsphomeError, not ValueError."""

    def fake_relative_to(self: Path, other: Path, walk_up: bool = False) -> Path:
        raise ValueError("paths have different anchors")

    monkeypatch.setattr(Path, "relative_to", fake_relative_to)
    with pytest.raises(EsphomeError, match="does not share a root"):
        _gather_files(_sources(project, "entry.yaml"))


def test_redacted_secrets_file_becomes_skeleton(project: Path) -> None:
    """The secrets file becomes a skeleton of the referenced keys."""
    discovered = _sources(
        project, "entry.yaml", "secrets.yaml", secrets=("secrets.yaml",)
    )
    files = _gather_redacted(discovered)
    skeleton = files["secrets.yaml"].decode()
    assert skeleton.startswith(SECRETS_SKELETON_HEADER)
    assert "api_key: ''" in skeleton
    assert b"SUPER_SECRET" not in files["secrets.yaml"]
    assert "key: !secret 'api_key'" in files["entry.yaml"].decode()


def test_redacted_symlinked_secrets_outside_root(project: Path, tmp_path: Path) -> None:
    """A secrets.yaml linked to another name outside the root is flagged and gets a root skeleton."""
    target = tmp_path / "actual_creds.yaml"
    target.write_text("api_key: SUPER_SECRET\n")
    link = project / "secrets.yaml"
    link.unlink()
    link.symlink_to(target)
    # Discovery flags the unresolved name but records the resolved path.
    resolved = link.resolve()
    CORE.config_path = project / "entry.yaml"
    files = _gather_redacted(
        DiscoveredYamlFiles([project / "entry.yaml", resolved], {resolved})
    )
    assert "api_key: ''" in files["secrets.yaml"].decode()
    assert b"SUPER_SECRET" not in b"".join(files.values())


@pytest.mark.parametrize(
    ("source", "value"),
    [
        ("password: my_password\n", "my_password"),
        ('password: "my_password"\n', "my_password"),
        ("password: 'my_password'\n", "my_password"),
        # A value that is a substring of its key name.
        ("password: word\n", "word"),
    ],
)
def test_redacted_inline_value_becomes_secret_ref(
    project: Path, source: str, value: str
) -> None:
    """An inline sensitive value becomes a path-named !secret listed in the skeleton."""
    (project / "wifi.yaml").write_text(source)
    CORE.config = {"wifi": [{"password": SensitiveStr(value)}]}
    files = _gather_redacted(_sources(project, "wifi.yaml"))
    assert files["wifi.yaml"] == b"password: !secret 'wifi_password'\n"
    assert "wifi_password: ''" in files["secrets.yaml"].decode()


@pytest.mark.parametrize(
    ("source", "value"),
    [
        (
            "network_key: 0x00112233445566778899aabbccddeeff\n",
            SensitiveHexInt(0x00112233445566778899AABBCCDDEEFF),
        ),
        ("network_key: 12345\n", SensitiveInt(12345)),
    ],
    ids=["hex", "decimal"],
)
def test_redacted_inline_int_value_becomes_secret_ref(
    project: Path, source: str, value: int
) -> None:
    """Inline sensitive integers are redacted like strings."""
    (project / "thread.yaml").write_text(source + "name: node 12345\n")
    CORE.config = {"openthread": {"network_key": value}}
    files = _gather_redacted(_sources(project, "thread.yaml"))
    assert files["thread.yaml"] == (
        b"network_key: !secret 'openthread_network_key'\nname: node 12345\n"
    )
    assert "openthread_network_key: ''" in files["secrets.yaml"].decode()


def test_redacted_quoted_int_value_fails_build(project: Path) -> None:
    """A sensitive integer written as a string is not matched, so the build fails."""
    (project / "thread.yaml").write_text('network_key: "0x1234"\n')
    CORE.config = {"openthread": {"network_key": SensitiveHexInt(0x1234)}}
    with pytest.raises(EsphomeError, match="network_key in thread.yaml"):
        _gather_redacted(_sources(project, "thread.yaml"))


def test_redacted_include_reference_round_trips(project: Path) -> None:
    """A nested `!include` stays a reference."""
    (project / "entry.yaml").write_text(
        "esphome:\n  name: test\nwifi: !include wifi.yaml\n"
    )
    files = _gather_redacted(_sources(project, "entry.yaml", "wifi.yaml"))
    assert "wifi: !include 'wifi.yaml'" in files["entry.yaml"].decode()


def test_redacted_reuses_existing_secret_name_for_duplicated_value(
    project: Path,
) -> None:
    """An inline value equal to a !secret value reuses that name."""
    (project / "wifi.yaml").write_text("password: SUPER_SECRET\n")
    CORE.config = {"wifi": [{"password": SensitiveStr("SUPER_SECRET")}]}
    yaml_util._SECRET_VALUES["SUPER_SECRET"] = "api_key"
    discovered = _sources(
        project, "wifi.yaml", "secrets.yaml", secrets=("secrets.yaml",)
    )
    files = _gather_redacted(discovered)
    assert files["wifi.yaml"] == b"password: !secret 'api_key'\n"


def test_redacted_raises_when_value_not_locatable(project: Path) -> None:
    """A value not found as a whole scalar fails the build without echoing it."""
    (project / "wifi.yaml").write_text("ssid: my_ssid\n")
    CORE.config = {"wifi": [{"password": SensitiveStr("not_in_any_file")}]}
    with pytest.raises(EsphomeError, match="wifi.password") as err:
        _gather_redacted(_sources(project, "wifi.yaml"))
    assert "not_in_any_file" not in str(err.value)


def test_redacted_accepts_secret_only_values(project: Path) -> None:
    """A value only set through `!secret` never needs to appear inline."""
    CORE.config = {"api": {"encryption": {"key": SensitiveStr("SUPER_SECRET")}}}
    yaml_util._SECRET_VALUES["SUPER_SECRET"] = "api_key"
    discovered = _sources(
        project, "entry.yaml", "secrets.yaml", secrets=("secrets.yaml",)
    )
    files = _gather_redacted(discovered)
    assert "api_key: ''" in files["secrets.yaml"].decode()


def test_redacted_skips_empty_sensitive_values(project: Path) -> None:
    """Empty defaults (e.g. mqtt password) are never swapped."""
    (project / "wifi.yaml").write_text("ssid: my_ssid\n")
    CORE.config = {"mqtt": {"password": SensitiveStr("")}}
    files = _gather_redacted(_sources(project, "wifi.yaml"))
    assert files["wifi.yaml"] == b"ssid: my_ssid\n"


def test_redacted_generates_unique_names_on_collision(project: Path) -> None:
    """Two inline values with colliding paths get distinct names."""
    (project / "wifi.yaml").write_text("password: first_pw\n")
    (project / "wifi2.yaml").write_text("password: second_pw\n")
    CORE.config = {
        "wifi": [
            {"password": SensitiveStr("first_pw")},
            {"password": SensitiveStr("second_pw")},
        ]
    }
    files = _gather_redacted(_sources(project, "wifi.yaml", "wifi2.yaml"))
    assert files["wifi.yaml"] == b"password: !secret 'wifi_password'\n"
    assert files["wifi2.yaml"] == b"password: !secret 'wifi_password_2'\n"


@pytest.mark.parametrize(
    ("files", "sources", "reason"),
    [
        (
            {"wifi.yaml": "password: my_password\nurl: http://user:my_password@host\n"},
            ("wifi.yaml",),
            "URL with credentials",
        ),
        (
            {
                "wifi.yaml": "password: my_password\nx: !lambda 'return \"my_password\";'\n"
            },
            ("wifi.yaml",),
            "embedded",
        ),
        (
            {
                "entry.yaml": "password: my_password\n"
                "pkg: !include {file: wifi.yaml, vars: {url: 'http://user:my_password@host'}}\n"
            },
            ("entry.yaml", "wifi.yaml"),
            "URL with credentials",
        ),
        (
            {
                "entry.yaml": "password: my_password\n"
                "pkg: !include {file: wifi.yaml, condition: \"${ x == 'my_password' }\"}\n"
            },
            ("entry.yaml", "wifi.yaml"),
            "embedded",
        ),
    ],
    ids=["url", "lambda", "include_vars", "include_condition"],
)
def test_redacted_embedded_sensitive_value_fails_build(
    project: Path, files: dict[str, str], sources: tuple[str, ...], reason: str
) -> None:
    """A sensitive value inside a larger scalar fails the build."""
    for name, text in files.items():
        (project / name).write_text(text)
    CORE.config = {"wifi": [{"password": SensitiveStr("my_password")}]}
    with pytest.raises(EsphomeError, match=reason):
        _gather_redacted(_sources(project, *sources))


def test_redacted_private_package_password_fails_build(project: Path) -> None:
    """A git package password never reaches the validated config, so it fails closed."""
    (project / "entry.yaml").write_text(
        "packages:\n  - url: https://host/repo\n    password: hunter2\n"
    )
    CORE.config = {}
    with pytest.raises(EsphomeError, match="packages.password in entry.yaml"):
        _gather_redacted(_sources(project, "entry.yaml"))


def test_redacted_overridden_package_password_fails_build(project: Path) -> None:
    """A password a later package overrides is not in the validated config, so it fails closed."""
    (project / "pkg").mkdir()
    (project / "pkg" / "common.yaml").write_text("wifi:\n  password: old_pw\n")
    (project / "entry.yaml").write_text(
        "packages:\n  common: !include pkg/common.yaml\nwifi:\n  password: new_pw\n"
    )
    CORE.config = {"wifi": {"password": SensitiveStr("new_pw")}}
    with pytest.raises(EsphomeError, match="wifi.password in pkg/common.yaml"):
        _gather_redacted(_sources(project, "entry.yaml", "pkg/common.yaml"))


def test_redacted_secret_url_round_trips(project: Path) -> None:
    """A package URL loaded through `!secret` is written back as the reference."""
    (project / "entry.yaml").write_text("packages:\n  - url: !secret pkg_url\n")
    (project / "secrets.yaml").write_text("pkg_url: https://user:pw@host/repo\n")
    CORE.config = {}
    files = _gather_redacted(
        _sources(project, "entry.yaml", "secrets.yaml", secrets=("secrets.yaml",))
    )
    assert "url: !secret 'pkg_url'" in files["entry.yaml"].decode()
    assert b"user:pw" not in b"".join(files.values())


def test_redacted_dir_include_rebases_nested_include(project: Path) -> None:
    """An `!include` inside a directory-included file is rebased to the file it is dumped in."""
    (project / "packages").mkdir()
    (project / "packages" / "base.yaml").write_text("wifi: !include ../wifi.yaml\n")
    (project / "entry.yaml").write_text("packages: !include_dir_named packages\n")
    CORE.config = {}
    files = _gather_redacted(
        _sources(project, "entry.yaml", "packages/base.yaml", "wifi.yaml")
    )
    assert "wifi: !include 'wifi.yaml'" in files["entry.yaml"].decode()
    assert "wifi: !include '../wifi.yaml'" in files["packages/base.yaml"].decode()


def test_redacted_document_include_keeps_vars(project: Path) -> None:
    """A file that is a single `!include` with vars keeps the directive."""
    (project / "entry.yaml").write_text(
        "!include\n  file: base.yaml\n  vars:\n    name: kitchen\n"
    )
    (project / "base.yaml").write_text("esphome:\n  name: ${name}\n")
    CORE.config = {}
    files = _gather_redacted(_sources(project, "entry.yaml", "base.yaml"))
    assert files["entry.yaml"] == b"!include\nfile: base.yaml\nvars:\n  name: kitchen\n"


def test_redacted_literal_round_trips(project: Path) -> None:
    """`!literal` survives the re-dump, so the recovered config is not substituted."""
    (project / "wifi.yaml").write_text("ssid: !literal '${ssid}'\n")
    CORE.config = {}
    files = _gather_redacted(_sources(project, "wifi.yaml"))
    assert files["wifi.yaml"] == b"ssid: !literal '${ssid}'\n"


def test_gather_places_symlinked_include_under_its_alias(
    project: Path, tmp_path: Path
) -> None:
    """A file included through a symlink is laid out under the name the YAML uses."""
    shared = tmp_path / "shared"
    shared.mkdir()
    (shared / "network.yaml").write_text("ssid: my_ssid\n")
    (project / "wifi.yaml").unlink()
    (project / "wifi.yaml").symlink_to(shared / "network.yaml")
    (project / "entry.yaml").write_text("wifi: !include wifi.yaml\n")
    CORE.config_path = project / "entry.yaml"
    CORE.config = {}
    discovered = discover_user_yaml_files(project / "entry.yaml")
    entries, _ = _gather_files(discovered)
    assert [rel for rel, _ in entries] == ["entry.yaml", "wifi.yaml"]
    files = dict(_read_files_verbatim(entries))
    assert files["wifi.yaml"] == b"ssid: my_ssid\n"


def test_redacted_warns_on_value_collision(
    project: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """An unrelated equal scalar is swapped with a warning; the value's own key is not warned about."""
    (project / "wifi.yaml").write_text("password: esp32\nplatform: esp32\n")
    CORE.config = {"wifi": [{"password": SensitiveStr("esp32")}]}
    files = _gather_redacted(_sources(project, "wifi.yaml"))
    assert files["wifi.yaml"] == (
        b"password: !secret 'wifi_password'\nplatform: !secret 'wifi_password'\n"
    )
    assert "also matches the scalar at platform in wifi.yaml" in caplog.text
    assert "at password in wifi.yaml" not in caplog.text


def test_redacted_no_warning_for_substitution_definition(
    project: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A swapped substitutions definition does not warn."""
    (project / "wifi.yaml").write_text(
        "substitutions:\n  wifi_password: hunter2\nwifi:\n  password: ${wifi_password}\n"
    )
    CORE.config = {"wifi": [{"password": SensitiveStr("hunter2")}]}
    _gather_redacted(_sources(project, "wifi.yaml"))
    assert "also matches" not in caplog.text


def test_redacted_sensitive_value_as_mapping_key_fails_build(project: Path) -> None:
    """A sensitive value used as a mapping key fails the build."""
    (project / "wifi.yaml").write_text("password: password\n")
    CORE.config = {"ota": [{"password": SensitiveStr("password")}]}
    with pytest.raises(EsphomeError, match="mapping key"):
        _gather_redacted(_sources(project, "wifi.yaml"))


def test_redacted_secret_sourced_overlap_warns_not_fails(
    project: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A !secret value inside another scalar warns instead of failing."""
    yaml_util._SECRET_VALUES["Kitchen"] = "wifi_ssid"
    (project / "wifi.yaml").write_text("ssid: Kitchen\nname: Kitchen Temperature\n")
    CORE.config = {"wifi": [{"ssid": SensitiveStr("Kitchen")}]}
    files = _gather_redacted(_sources(project, "wifi.yaml"))
    assert b"!secret 'wifi_ssid'" in files["wifi.yaml"]
    assert "appears inside the value at name in wifi.yaml" in caplog.text


@pytest.mark.parametrize(
    ("unresolved", "remote", "expected"),
    [
        (["${board}.yaml"], [], ["#   ${board}.yaml"]),
        (
            [],
            ["https://github.com/org/repo@main", "https://github.com/org/other"],
            [
                "#   https://github.com/org/repo@main",
                "#   https://github.com/org/other",
            ],
        ),
    ],
    ids=["includes", "remote_packages"],
)
def test_uncaptured_note_lists_content(
    unresolved: list[str], remote: list[str], expected: list[str]
) -> None:
    """Uncaptured includes and remote packages are listed in the note entry."""
    rel, content = _uncaptured_note(unresolved, remote)
    assert rel == UNCAPTURED_NOTE_PATH
    text = content.decode()
    assert text.startswith("# store_yaml:")
    for line in expected:
        assert line in text


def test_uncaptured_note_none_when_complete() -> None:
    assert _uncaptured_note([], []) is None


def test_remote_package_descriptions_read_packages_record() -> None:
    """Remote sources format as url@ref, or url alone."""
    data = packages._get_data()
    data.remote_sources[
        packages.RemotePackageSource("https://github.com/org/repo", "main")
    ] = None
    data.remote_sources[
        packages.RemotePackageSource("https://github.com/org/other", None)
    ] = None
    assert _remote_package_descriptions() == [
        "https://github.com/org/repo@main",
        "https://github.com/org/other",
    ]


def test_remote_package_descriptions_empty_without_packages() -> None:
    assert _remote_package_descriptions() == []


def test_remote_package_descriptions_hide_credentials() -> None:
    """URL userinfo is masked, and a `!secret` URL is named, not printed."""
    data = packages._get_data()
    data.remote_sources[
        packages.RemotePackageSource("https://user:pw@host/repo", "main")
    ] = None
    data.remote_sources[
        packages.RemotePackageSource("https://user:pw@host/private", None)
    ] = None
    yaml_util._SECRET_VALUES["https://user:pw@host/private"] = "pkg_url"
    assert _remote_package_descriptions() == [
        "https://***@host/repo@main",
        "!secret pkg_url",
    ]


def test_secrets_skeleton_quotes_awkward_keys() -> None:
    """Keys that YAML would read as other types load back as the same strings."""
    keys = {"true", "123", "a: b", "api_key"}
    skeleton = _build_secrets_skeleton(keys).decode()
    assert skeleton.startswith(SECRETS_SKELETON_HEADER)
    assert "'true': ''" in skeleton
    assert yaml.safe_load(skeleton) == dict.fromkeys(keys, "")
    assert _build_secrets_skeleton(set()) == SECRETS_SKELETON_HEADER.encode()


def test_pack_envelope_roundtrip() -> None:
    files = [
        ("entry.yaml", b"esphome:\n  name: test\n"),
        ("dossiers/élan.yaml", b"ssid: a\n"),
    ]
    assert unpack_envelope(_pack_envelope(files)) == dict(files)


def test_pack_envelope_rejects_overlong_path() -> None:
    with pytest.raises(EsphomeError):
        _pack_envelope([("a" * (0xFFFF + 1), b"")])


def test_pack_envelope_rejects_duplicate_paths() -> None:
    with pytest.raises(EsphomeError, match="duplicate"):
        _pack_envelope([("entry.yaml", b"a: 1\n"), ("entry.yaml", b"b: 2\n")])


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
    """A corrupt path raises EsphomeError, not UnicodeDecodeError."""
    entry = struct.pack("<H", 2) + b"\xff\xfe" + struct.pack("<I", 0)
    with pytest.raises(EsphomeError, match="UTF-8"):
        unpack_envelope(ENVELOPE_MAGIC + struct.pack("<I", 1) + entry)


def test_unpack_envelope_rejects_duplicate_paths() -> None:
    entry = struct.pack("<H", 6) + b"a.yaml" + struct.pack("<I", 1) + b"x"
    with pytest.raises(EsphomeError, match="duplicate"):
        unpack_envelope(ENVELOPE_MAGIC + struct.pack("<I", 2) + entry + entry)


@pytest.mark.parametrize("path", ["/etc/passwd", "\\evil.yaml", "C:/evil.yaml"])
def test_unpack_envelope_rejects_non_relative_paths(path: str) -> None:
    """Absolute or drive-qualified paths are rejected."""
    with pytest.raises(EsphomeError, match="non-relative"):
        unpack_envelope(_pack_envelope([(path, b"boom\n")]))


def test_final_validate_accepts_encrypted_api() -> None:
    config = {CONF_ALLOW_UNENCRYPTED: False}
    full = {
        "api": {"encryption": {"key": "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8="}}
    }
    assert _run_final_validate(full, config) is config


@pytest.mark.parametrize(
    "api", [{}, {"encryption": {}}], ids=["no_encryption", "keyless_encryption"]
)
def test_final_validate_rejects_unencrypted_api(api: dict) -> None:
    """Keyless encryption accepts the all-zeros key, so it does not count."""
    with pytest.raises(cv.Invalid, match="requires API encryption"):
        _run_final_validate({"api": api}, {CONF_ALLOW_UNENCRYPTED: False})


@pytest.mark.parametrize(
    ("include_secrets", "expected"),
    [
        (False, "without API encryption"),
        (True, "verbatim contents of secrets.yaml"),
    ],
)
def test_final_validate_allows_unencrypted_with_escape_hatch(
    caplog: pytest.LogCaptureFixture, include_secrets: bool, expected: str
) -> None:
    config = {CONF_ALLOW_UNENCRYPTED: True, "include_secrets": include_secrets}
    assert _run_final_validate({"api": {}}, config) is config
    assert expected in caplog.text
