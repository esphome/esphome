"""Tests for store_yaml file gathering, redaction and envelope packing."""

from __future__ import annotations

from pathlib import Path
import struct
import sys

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
    _iter_nodes,
    _pack_envelope,
    _read_files_verbatim,
    _remote_package_descriptions,
    _symlink_descriptions,
    _uncaptured_note,
    unpack_envelope,
)
import esphome.config_validation as cv
from esphome.core import CORE, EsphomeError, Lambda
import esphome.final_validate as fv
from esphome.helpers import add_class_to_obj
from esphome.yaml_util import (
    DiscoveredYamlFiles,
    SensitiveHexInt,
    SensitiveInt,
    SensitiveStr,
    discover_user_yaml_files,
)

WIFI_PW = {"wifi": {"networks": [{"password": SensitiveStr("hunter2")}]}}
API_KEY = {"api": {"encryption": {"key": SensitiveStr("SUPER_SECRET")}}}
_PKG_PW = "packages:\n  common: !include pkg/common.yaml\n"
_SECRET_VAR = "wifi: !include {file: wifi.yaml, vars: {pw: !secret wifi_password}}\n"
_OUTER_VAR = "packages:\n  outer: !include {file: outer.yaml, vars: {pw: !secret wifi_password}}\n"
_WIFI_SECRET = "wifi_password: hunter2\n"


@pytest.fixture
def project(tmp_path: Path) -> Path:
    """An entry yaml, an include and a secrets file as the validated config's root."""
    project_dir = tmp_path / "project"
    project_dir.mkdir()
    (project_dir / "entry.yaml").write_text(
        "esphome:\n  name: test\napi:\n  encryption:\n    key: !secret api_key\n"
    )
    (project_dir / "wifi.yaml").write_text("ssid: my_ssid\npassword: my_password\n")
    (project_dir / "secrets.yaml").write_text("api_key: SUPER_SECRET\n")
    CORE.config_path = project_dir / "entry.yaml"
    CORE.config = {}
    return project_dir


@pytest.fixture
def shared(tmp_path: Path) -> Path:
    shared = tmp_path / "shared"
    shared.mkdir()
    return shared


def _sources(
    project_dir: Path,
    *names: str,
    secrets: tuple[str, ...] = (),
    unresolved: list[str] | None = None,
    load_errors: list[str] | None = None,
) -> DiscoveredYamlFiles:
    files = [(project_dir / name).resolve() for name in names]
    return DiscoveredYamlFiles(
        files,
        {(project_dir / name).resolve() for name in secrets},
        unresolved or [],
        load_errors or [],
        {path: [path] for path in files},
    )


def _gather_redacted(discovered: DiscoveredYamlFiles) -> dict[str, bytes]:
    entries, secret_rels = _gather_files(discovered)
    return dict(
        _generate_redacted_files(entries, secret_rels, _remote_package_descriptions())
    )


def _redact(
    project: Path, files: dict[str, str], config: dict | None = None
) -> dict[str, bytes]:
    """Write `files`, register the secrets file as discovery would, and redact."""
    for name, text in files.items():
        (project / name).parent.mkdir(parents=True, exist_ok=True)
        (project / name).write_text(text)
    if config is not None:
        CORE.config = config
    secrets = yaml.safe_load((project / "secrets.yaml").read_text()) or {}
    for name, value in secrets.items():
        yaml_util._SECRET_VALUES[str(value)] = name
    return _gather_redacted(
        _sources(project, *files, "secrets.yaml", secrets=("secrets.yaml",))
    )


def _run_final_validate(full_config: dict, config: dict) -> dict:
    token = fv.full_config.set(full_config)
    try:
        return _final_validate(config)
    finally:
        fv.full_config.reset(token)


def test_gather_maps_rel_paths_and_reads_verbatim(project: Path) -> None:
    """Entries map to root-relative paths; `include_secrets: true` keeps the bytes."""
    discovered = _sources(
        project, "entry.yaml", "secrets.yaml", secrets=("secrets.yaml",)
    )
    entries, secret_rels = _gather_files(discovered)
    assert dict(entries) == {
        "entry.yaml": project / "entry.yaml",
        "secrets.yaml": project / "secrets.yaml",
    }
    assert secret_rels == {"secrets.yaml"}
    contents = dict(_read_files_verbatim(entries))
    assert contents["secrets.yaml"] == (project / "secrets.yaml").read_bytes()


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


def test_gather_raises_on_load_errors(project: Path) -> None:
    discovered = _sources(project, "entry.yaml", load_errors=["oops.yaml: boom"])
    with pytest.raises(EsphomeError, match="oops.yaml"):
        _gather_files(discovered)


def test_read_files_verbatim_raises_on_unreadable_file(
    project: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
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
    files = _redact(project, {"entry.yaml": (project / "entry.yaml").read_text()})
    skeleton = files["secrets.yaml"].decode()
    assert skeleton.startswith(SECRETS_SKELETON_HEADER)
    assert "api_key: ''" in skeleton
    assert b"SUPER_SECRET" not in b"".join(files.values())
    assert "key: !secret 'api_key'" in files["entry.yaml"].decode()


def test_redacted_symlinked_secrets_outside_root(project: Path, tmp_path: Path) -> None:
    """A secrets.yaml linked to another name outside the root gets a root skeleton."""
    target = tmp_path / "actual_creds.yaml"
    target.write_text("api_key: SUPER_SECRET\n")
    link = project / "secrets.yaml"
    link.unlink()
    link.symlink_to(target)
    resolved = link.resolve()
    files = _gather_redacted(
        DiscoveredYamlFiles([project / "entry.yaml", resolved], {resolved})
    )
    assert "api_key: ''" in files["secrets.yaml"].decode()
    assert b"SUPER_SECRET" not in b"".join(files.values())


@pytest.mark.parametrize(
    ("source", "config", "expected", "name"),
    [
        pytest.param(
            "password: 'my_password'\n",
            {"wifi": [{"password": SensitiveStr("my_password")}]},
            b"password: !secret 'wifi_password'\n",
            "wifi_password",
            id="string",
        ),
        pytest.param(
            "password: word\n",
            {"wifi": [{"password": SensitiveStr("word")}]},
            b"password: !secret 'wifi_password'\n",
            "wifi_password",
            id="value_inside_its_own_key_name",
        ),
        pytest.param(
            "network_key: 0x00112233445566778899aabbccddeeff\nname: node 12345\n",
            {
                "openthread": {
                    "network_key": SensitiveHexInt(0x00112233445566778899AABBCCDDEEFF)
                }
            },
            b"network_key: !secret 'openthread_network_key'\nname: node 12345\n",
            "openthread_network_key",
            id="hex_int",
        ),
        pytest.param(
            "network_key: 12345\nname: node 12345\n",
            {"openthread": {"network_key": SensitiveInt(12345)}},
            b"network_key: !secret 'openthread_network_key'\nname: node 12345\n",
            "openthread_network_key",
            id="int",
        ),
    ],
)
def test_redacted_inline_value_becomes_secret_ref(
    project: Path, source: str, config: dict, expected: bytes, name: str
) -> None:
    """An inline sensitive value becomes a path-named !secret listed in the skeleton."""
    files = _redact(project, {"wifi.yaml": source}, config)
    assert files["wifi.yaml"] == expected
    assert f"{name}: ''" in files["secrets.yaml"].decode()


@pytest.mark.parametrize(
    ("files", "config", "match"),
    [
        pytest.param(
            {"thread.yaml": 'network_key: "0x1234"\n'},
            {"openthread": {"network_key": SensitiveHexInt(0x1234)}},
            "network_key in thread.yaml",
            id="quoted_int",
        ),
        pytest.param(
            {"wifi.yaml": "ssid: my_ssid\n"},
            {"wifi": [{"password": SensitiveStr("not_in_any_file")}]},
            "wifi.password",
            id="not_locatable",
        ),
        pytest.param(
            {
                "wifi.yaml": "password: my_password\nx: !lambda 'return \"my_password\";'\n"
            },
            {"wifi": [{"password": SensitiveStr("my_password")}]},
            "embedded",
            id="lambda",
        ),
        pytest.param(
            {
                "entry.yaml": "password: my_password\n"
                "pkg: !include {file: wifi.yaml, vars: {url: 'http://user:my_password@host'}}\n"
            },
            {"wifi": [{"password": SensitiveStr("my_password")}]},
            "embedded|URL with credentials",
            id="include_vars",
        ),
        pytest.param(
            {
                "entry.yaml": "password: my_password\n"
                "pkg: !include {file: wifi.yaml, condition: \"${ x == 'my_password' }\"}\n"
            },
            {"wifi": [{"password": SensitiveStr("my_password")}]},
            "embedded",
            id="include_condition",
        ),
        pytest.param(
            {
                "entry.yaml": "packages:\n  - url: https://host/repo\n    password: hunter2\n"
            },
            {},
            "packages.password in entry.yaml",
            id="private_package_password",
        ),
        pytest.param(
            {
                "pkg/common.yaml": "wifi:\n  password: old_pw\n",
                "entry.yaml": _PKG_PW + "wifi:\n  password: new_pw\n",
            },
            {"wifi": {"networks": [{"password": SensitiveStr("new_pw")}]}},
            "wifi.password in pkg/common.yaml",
            id="overridden_package_password_regrouped_by_validation",
        ),
        pytest.param(
            {
                "pkg/common.yaml": "mqtt:\n  password: old_pw\n",
                "entry.yaml": _PKG_PW
                + "wifi:\n  password: hunter2\nmqtt:\n  password: hunter2\n",
            },
            {**WIFI_PW, "mqtt": {"password": SensitiveStr("hunter2")}},
            "mqtt.password in pkg/common.yaml",
            id="shared_password_keeps_every_path",
        ),
        pytest.param(
            {
                "pkg/common.yaml": "mqtt:\n  password: old_pw\n",
                "entry.yaml": _PKG_PW + 'mqtt:\n  password: ""\n',
            },
            {"mqtt": {"password": SensitiveStr("")}},
            "mqtt.password in pkg/common.yaml",
            id="overridden_by_empty_password",
        ),
        pytest.param(
            {"wifi.yaml": "substitutions:\n  prefix_hunter2: x\npassword: hunter2\n"},
            WIFI_PW,
            "inside a key under substitutions",
            id="key_embeds_password",
        ),
        pytest.param(
            {
                "legacy.yaml": "mqtt:\n  broker: b\n  password: hunter2\n",
                "entry.yaml": "packages:\n  legacy: !include legacy.yaml\n"
                "mqtt: !remove\nwifi:\n  password: pw2\n",
            },
            {"wifi": {"networks": [{"password": SensitiveStr("pw2")}]}},
            "mqtt.password in legacy.yaml",
            id="removed_component_keeps_credential_keys",
        ),
        pytest.param(
            {"wifi.yaml": "wifi:\n  hunter2:\n    child: x\n"},
            {"ota": [{"password": SensitiveStr("hunter2")}]},
            "mapping key under wifi",
            id="password_as_mapping_key",
        ),
        pytest.param(
            {
                "ld.yaml": "ld2410:\n  bluetooth_password: !lambda 'return \"hunter2\";'\n"
            },
            {
                "ld2410": {
                    "bluetooth_password": add_class_to_obj(
                        Lambda('return "hunter2";'), yaml_util.Sensitive
                    )
                }
            },
            "set by a lambda",
            id="credential_from_a_lambda",
        ),
        pytest.param(
            {
                "ota.yaml": "password: !secret ota_pw\nurl: http://host/?pw=hunter2\n",
                "secrets.yaml": "ota_pw: hunter2\n",
            },
            {"ota": [{"password": SensitiveStr("hunter2")}]},
            "inside url in ota.yaml",
            id="secret_inside_a_larger_value",
        ),
        pytest.param(
            {"fp.yaml": "fingerprint_grow:\n  password: !lambda 'return 12345678;'\n"},
            {
                "fingerprint_grow": {
                    "password": add_class_to_obj(
                        Lambda("return 12345678;"), yaml_util.Sensitive
                    )
                }
            },
            "set by a lambda",
            id="credential_from_a_lambda_number",
        ),
        pytest.param(
            {
                "entry.yaml": "substitutions:\n  a: ${b}\n  b: ${a}\nwifi:\n  password: ${a}\n"
            },
            WIFI_PW,
            "cannot be confirmed",
            id="alias_cycle",
        ),
        pytest.param(
            {
                "entry.yaml": "packages:\n  base:\n    api:\n      encryption:\n        key: old_key\n"
                "api:\n  encryption:\n    key: !secret api_key\n"
            },
            API_KEY,
            "packages.base.api.encryption.key",
            id="inline_package_override",
        ),
        pytest.param(
            {
                "pkg/common.yaml": "substitutions:\n  pkg_pw: !secret pw\n",
                "entry.yaml": "substitutions:\n  pkg_pw: hunter2\n"
                + _PKG_PW
                + "  private:\n    url: https://host/repo\n    password: ${pkg_pw}\n",
                "secrets.yaml": "pw: other\n",
            },
            {},
            "cannot be confirmed",
            id="substitution_defined_twice",
        ),
        pytest.param(
            {
                "entry.yaml": "substitutions:\n  u: !secret u\n  p: !secret p\n"
                'x: !lambda \'use("https://${u}:${p}@host", "https://bob:hunter2@other");\'\n',
                "secrets.yaml": "u: bob\np: pw\n",
            },
            {},
            "URL with credentials",
            id="second_url_in_a_scalar",
        ),
        pytest.param(
            {
                "entry.yaml": "substitutions:\n  pkg_password: hunter2\n"
                "packages:\n  private:\n    url: https://host/repo\n    password: ${pkg_password}\n"
            },
            {},
            "cannot be confirmed",
            id="literal_substitution",
        ),
        pytest.param(
            {
                "entry.yaml": "packages:\n  private:\n    url: https://host/repo\n"
                "    password: ${ pkg_password }x\n"
            },
            {},
            "cannot be confirmed",
            id="expression",
        ),
        pytest.param(
            {
                "entry.yaml": "substitutions:\n  git_host: host\n"
                "packages:\n  - url: https://user:hunter2@${git_host}/repo\n"
            },
            {},
            "URL with credentials",
            id="mixed_url",
        ),
        pytest.param(
            {
                "entry.yaml": "substitutions:\n  user: bob\n  pw: hunter2\n"
                "packages:\n  - url: https://${user}:${pw}@host/repo\n"
            },
            {},
            "URL with credentials",
            id="literal_url_parts",
        ),
        pytest.param(
            {
                "entry.yaml": "wifi: !include {file: wifi.yaml, vars: {pw: other_pw}}\n",
                "wifi.yaml": "password: ${pw}\n",
            },
            WIFI_PW,
            "cannot be confirmed",
            id="literal_include_var",
        ),
        pytest.param(
            {
                "entry.yaml": _OUTER_VAR,
                "outer.yaml": "packages:\n  inner: !include inner.yaml\n",
                "inner.yaml": "defaults:\n  pw: realpw\nmqtt:\n  password: ${pw}\n",
                "secrets.yaml": _WIFI_SECRET,
            },
            {"mqtt": {"password": SensitiveStr("other_pw")}},
            "cannot be confirmed",
            id="nested_include_callee_default_wins",
        ),
        pytest.param(
            {
                "entry.yaml": "cfg: !include {file: base.yaml, vars: !secret common_vars}\n",
                "base.yaml": "password: ${pw}\n",
                "secrets.yaml": "common_vars:\n  pw: hunter2\n",
            },
            WIFI_PW,
            "could not redact",
            id="credential_only_inside_a_secret_mapping",
        ),
        pytest.param(
            {
                "entry.yaml": _SECRET_VAR + "wifi2: !include wifi.yaml\n",
                "wifi.yaml": "defaults:\n  pw: placeholder\npassword: ${pw}\n",
                "secrets.yaml": _WIFI_SECRET,
            },
            WIFI_PW,
            "cannot be confirmed",
            id="second_include_uses_literal_default",
        ),
    ],
)
def test_redacted_fails_closed(
    project: Path, files: dict[str, str], config: dict, match: str
) -> None:
    """An unprovable credential fails the build, and the message never echoes one."""
    with pytest.raises(EsphomeError, match=match) as err:
        _redact(project, files, config)
    assert not any(
        str(node) in str(err.value)
        for _, node, is_key in _iter_nodes(config)
        if not is_key and isinstance(node, yaml_util.Sensitive) and node
    )


@pytest.mark.parametrize(
    ("files", "config", "expect"),
    [
        pytest.param(
            {"wifi.yaml": "password: SUPER_SECRET\n"},
            {"wifi": [{"password": SensitiveStr("SUPER_SECRET")}]},
            {"wifi.yaml": "password: !secret 'api_key'\n"},
            id="inline_value_reuses_the_secret_name",
        ),
        pytest.param(
            {
                "wifi.yaml": "password: first_pw\n",
                "wifi2.yaml": "password: second_pw\n",
            },
            {
                "wifi": [
                    {"password": SensitiveStr("first_pw")},
                    {"password": SensitiveStr("second_pw")},
                ]
            },
            {
                "wifi.yaml": "password: !secret 'wifi_password'\n",
                "wifi2.yaml": "password: !secret 'wifi_password_2'\n",
            },
            id="colliding_names_stay_distinct",
        ),
        pytest.param(
            {"wifi.yaml": "ssid: my_ssid\n"},
            {"mqtt": {"password": SensitiveStr("")}},
            {"wifi.yaml": "ssid: my_ssid\n"},
            id="empty_sensitive_value_is_not_swapped",
        ),
        pytest.param(
            {
                "entry.yaml": "api:\n  encryption:\n    key: !secret api_key\n"
                "  actions:\n    - action: press\n      variables:\n        key: string\n"
            },
            API_KEY,
            {"entry.yaml": "key: string"},
            id="action_variable_named_key",
        ),
        pytest.param(
            {
                "entry.yaml": "api:\n  encryption:\n    key: !secret api_key\n"
                "binary_sensor: !include buttons.yaml\n",
                "buttons.yaml": "- platform: matrix_keypad\n  key: A\n",
            },
            API_KEY,
            {"buttons.yaml": "key: A"},
            id="list_file_key_is_not_the_encryption_key",
        ),
        pytest.param(
            {
                "entry.yaml": _SECRET_VAR,
                "wifi.yaml": "password: ${pw}\n",
                "secrets.yaml": _WIFI_SECRET,
            },
            WIFI_PW,
            {},
            id="secret_include_var",
        ),
        pytest.param(
            {
                "entry.yaml": "wifi: !include wifi.yaml\n",
                "wifi.yaml": "defaults:\n  pw: !secret wifi_password\npassword: ${pw}\n",
                "secrets.yaml": _WIFI_SECRET,
            },
            WIFI_PW,
            {},
            id="secret_default",
        ),
        pytest.param(
            {
                "entry.yaml": "substitutions:\n  pw: !secret wifi_password\nwifi: !include wifi.yaml\n",
                "wifi.yaml": "password: ${pw}\n",
                "secrets.yaml": _WIFI_SECRET,
            },
            WIFI_PW,
            {},
            id="secret_substitution",
        ),
        pytest.param(
            {
                "entry.yaml": "substitutions:\n  pw: hunter2\n" + _SECRET_VAR,
                "wifi.yaml": "password: ${pw}\n",
                "secrets.yaml": _WIFI_SECRET,
            },
            WIFI_PW,
            {},
            id="var_shadows_literal_substitution",
        ),
        pytest.param(
            {
                "entry.yaml": _SECRET_VAR,
                "wifi.yaml": "defaults:\n  pw: placeholder\npassword: ${pw}\n",
                "secrets.yaml": _WIFI_SECRET,
            },
            WIFI_PW,
            {},
            id="var_shadows_literal_default",
        ),
        pytest.param(
            {
                "entry.yaml": _OUTER_VAR,
                "outer.yaml": "wifi: !include wifi.yaml\n",
                "wifi.yaml": "password: ${pw}\n",
                "secrets.yaml": _WIFI_SECRET,
            },
            WIFI_PW,
            {},
            id="nested_include_inherits_caller_vars",
        ),
        pytest.param(
            {
                "entry.yaml": "substitutions:\n  pkg_password: !secret pw\n"
                "packages:\n  private:\n    url: https://host/repo\n    password: ${pkg_password}\n",
                "secrets.yaml": "pw: hunter2\n",
            },
            {},
            {},
            id="git_password_from_secret_substitution",
        ),
        pytest.param(
            {
                "entry.yaml": "substitutions:\n  user: !secret user\n  pw: !secret pw\n"
                "packages:\n  - url: https://${user}:${pw}@host/repo\n",
                "secrets.yaml": "user: bob\npw: hunter2\n",
            },
            {},
            {},
            id="url_parts_from_secrets",
        ),
        pytest.param(
            {
                "entry.yaml": "http_request:\n  request_headers: !secret headers\n"
                "  more: !secret list_headers\n",
                "secrets.yaml": "headers:\n  Authorization: Bearer hunter2\n"
                "list_headers:\n  - Bearer hunter2\n",
            },
            {},
            {
                "entry.yaml": "request_headers: !secret 'headers'\n  more: !secret 'list_headers'",
                "secrets.yaml": "headers: ''",
            },
            id="container_secrets",
        ),
        pytest.param(
            {
                "entry.yaml": "http: !include {file: h.yaml, vars: {headers: !secret headers}}\n",
                "h.yaml": "request_headers: ${headers}\n",
                "secrets.yaml": "headers:\n  Authorization: Bearer hunter2\n",
            },
            {},
            {"entry.yaml": "headers: !secret 'headers'"},
            id="container_secret_in_include_vars",
        ),
        pytest.param(
            {
                "entry.yaml": "cfg: !include {file: base.yaml, vars: !secret common_vars}\n",
                "base.yaml": "name: ${name}\n",
                "secrets.yaml": "common_vars:\n  name: hunter2\n",
            },
            {},
            {"entry.yaml": "vars: !secret 'common_vars'"},
            id="secret_vars_mapping",
        ),
        pytest.param(
            {
                "entry.yaml": "packages:\n  - url: !secret pkg_url\n",
                "secrets.yaml": "pkg_url: https://user:hunter2@host/repo\n",
            },
            {},
            {"entry.yaml": "url: !secret 'pkg_url'"},
            id="secret_url",
        ),
        pytest.param(
            {
                "packages/base.yaml": "wifi: !include ../wifi.yaml\n",
                "entry.yaml": "packages: !include_dir_named packages\n",
            },
            {},
            {
                "entry.yaml": "wifi: !include 'wifi.yaml'",
                "packages/base.yaml": "wifi: !include '../wifi.yaml'",
            },
            id="dir_include_rebases_nested_include",
        ),
        pytest.param(
            {
                "entry.yaml": "!include\n  file: base.yaml\n  vars:\n    name: kitchen\n",
                "base.yaml": "esphome:\n  name: ${name}\n",
            },
            {},
            {"entry.yaml": "!include\nfile: base.yaml\nvars:\n  name: kitchen\n"},
            id="document_include_keeps_vars",
        ),
        pytest.param(
            {"wifi.yaml": "ssid: !literal '${ssid}'\n"},
            {},
            {"wifi.yaml": "ssid: !literal '${ssid}'\n"},
            id="literal",
        ),
        pytest.param(
            {"fp.yaml": "password: 0\naccuracy_decimals: 0\n"},
            {"fingerprint_grow": {"password": SensitiveInt(0)}},
            {
                "fp.yaml": "password: !secret 'fingerprint_grow_password'\naccuracy_decimals: 0\n"
            },
            id="zero_credential_swapped_only_at_its_key",
        ),
        pytest.param(
            {
                "fp.yaml": "substitutions:\n  fp_password: 12345678\n"
                "fingerprint_grow:\n  - password: 12345678\n  - password: ${fp_password}\n"
                "sensor:\n  accuracy_decimals: 0\n"
            },
            {"fingerprint_grow": [{"password": SensitiveInt(12345678)}] * 2},
            {
                "fp.yaml": "fp_password: !secret 'fingerprint_grow_password'\n"
                "fingerprint_grow:\n  - password: !secret 'fingerprint_grow_password'\n"
                "  - password: ${fp_password}\nsensor:\n  accuracy_decimals: 0\n"
            },
            id="int_credential_reused_through_a_substitution",
        ),
        pytest.param(
            {
                "fp.yaml": "fingerprint_grow:\n  - password: 12345678\n  - password: '12345678'\n"
            },
            {"fingerprint_grow": [{"password": SensitiveInt(12345678)}] * 2},
            {
                "fp.yaml": "fingerprint_grow:\n  - password: !secret 'fingerprint_grow_password'\n"
                "  - password: !secret 'fingerprint_grow_password'\n"
            },
            id="int_credential_written_quoted",
        ),
        pytest.param(
            {
                "entry.yaml": "substitutions:\n  base_pw: !secret wifi_password\n  pw: ${base_pw}\n"
                "wifi:\n  password: ${pw}\n",
                "secrets.yaml": _WIFI_SECRET,
            },
            WIFI_PW,
            {"entry.yaml": "base_pw: !secret 'wifi_password'"},
            id="substitution_alias_of_a_secret",
        ),
        pytest.param(
            {
                "entry.yaml": "packages:\n  base: !include base.yaml\n",
                "base.yaml": "packages:\n  loop: !include {file: base.yaml, condition: false}\n"
                "wifi:\n  password: !secret wifi_password\n",
                "secrets.yaml": _WIFI_SECRET,
            },
            WIFI_PW,
            {"base.yaml": "password: !secret 'wifi_password'"},
            id="disabled_self_include_is_skipped",
        ),
        pytest.param(
            {
                "entry.yaml": "packages:\n  base:\n    substitutions:\n"
                "      pw: !secret wifi_password\nwifi:\n  password: ${pw}\n",
                "secrets.yaml": _WIFI_SECRET,
            },
            WIFI_PW,
            {"entry.yaml": "pw: !secret 'wifi_password'"},
            id="substitution_defined_in_an_inline_package",
        ),
        pytest.param(
            {
                "fp.yaml": "packages:\n  base:\n    substitutions:\n"
                "      fp_password: 12345678\n"
                "fingerprint_grow:\n  password: ${fp_password}\n"
            },
            {"fingerprint_grow": {"password": SensitiveInt(12345678)}},
            {"fp.yaml": "fp_password: !secret 'fingerprint_grow_password'"},
            id="int_substitution_defined_in_an_inline_package",
        ),
        pytest.param(
            {
                "wifi.yaml": "button:\n  - on_press:\n      wifi.configure:\n"
                "        ssid: !lambda 'return id(ssid_text).state;'\n"
                "        password: !lambda 'return id(pw_text).state;'\n"
            },
            {
                "button": [
                    {
                        "on_press": [
                            {
                                "then": [
                                    {
                                        "wifi.configure": {
                                            key: add_class_to_obj(
                                                Lambda(f"return id({key}_text).state;"),
                                                yaml_util.Sensitive,
                                            )
                                            for key in ("ssid", "password")
                                        }
                                    }
                                ]
                            }
                        ]
                    }
                ]
            },
            {
                "wifi.yaml": "password: !lambda |-\n          return id(pw_text).state;\n"
            },
            id="lambda_reading_an_entity_is_kept",
        ),
    ],
)
def test_redacted_hides_credentials(
    project: Path, files: dict[str, str], config: dict, expect: dict[str, str]
) -> None:
    """The redacted files contain the expected text and never `hunter2`."""
    out = _redact(project, files, config)
    assert b"hunter2" not in b"".join(out.values())
    for rel, text in expect.items():
        assert text in out[rel].decode()


@pytest.mark.parametrize("int_first", [True, False])
def test_redacted_shared_text_keeps_substring_scan(
    project: Path, int_first: bool
) -> None:
    """A text that is an integer credential in one place and a string in another
    is still scanned as a substring, whichever is collected first."""
    thread = ("openthread", {"network_key": SensitiveInt(12345678)})
    mqtt = ("mqtt", {"password": SensitiveStr("12345678")})
    config = dict([thread, mqtt] if int_first else [mqtt, thread])
    files = {
        "node.yaml": "network_key: 12345678\npassword: '12345678'\nname: node12345678\n"
    }
    with pytest.raises(EsphomeError, match="embedded"):
        _redact(project, files, config)


def test_redacted_symlinked_package_loads_relative_to_its_name(
    project: Path, shared: Path
) -> None:
    """A directory include inside a symlinked file resolves beside the link."""
    (shared / "pkg.yaml").write_text("sensor: !include_dir_merge_list sensors\n")
    (project / "pkg.yaml").symlink_to(shared / "pkg.yaml")
    (project / "sensors").mkdir()
    (project / "sensors" / "a.yaml").write_text("- platform: template\n  name: a\n")
    (project / "entry.yaml").write_text("packages:\n  p: !include pkg.yaml\n")
    files = _gather_redacted(discover_user_yaml_files(project / "entry.yaml"))
    assert "platform: template" in files["pkg.yaml"].decode()


def test_redacted_symlinked_dir_include_rebases_where_the_loader_reads(
    project: Path, shared: Path
) -> None:
    """`..` after a symlinked directory goes where the loader reads: through the link
    on POSIX, lexically on Windows."""
    (shared / "pkgs").mkdir()
    (shared / "pkgs" / "base.yaml").write_text("wifi: !include ../wifi.yaml\n")
    (shared / "wifi.yaml").write_bytes(b"ssid: shared\n")
    (project / "wifi.yaml").write_bytes(b"ssid: project\n")
    (project / "pkgs").symlink_to(shared / "pkgs")
    (project / "entry.yaml").write_text("packages: !include_dir_named pkgs\n")
    files = _gather_redacted(discover_user_yaml_files(project / "entry.yaml"))
    # The file itself is captured at its real location, so its own include stays put.
    assert "!include '../wifi.yaml'" in files["../shared/pkgs/base.yaml"].decode()
    if sys.platform == "win32":
        assert "wifi: !include 'wifi.yaml'" in files["entry.yaml"].decode()
        assert files["wifi.yaml"] == b"ssid: project\n"
    else:
        assert "wifi: !include '../shared/wifi.yaml'" in files["entry.yaml"].decode()
        assert files["../shared/wifi.yaml"] == b"ssid: shared\n"


def test_include_through_a_directory_symlink(
    project: Path, shared: Path, tmp_path: Path
) -> None:
    """The include is pointed at the captured file and the note names the link,
    also when the config directory itself is reached through a symlink."""
    (shared / "wifi.yaml").write_text("ssid: shared\n")
    (project / "linked").symlink_to(shared)
    (project / "entry.yaml").write_text("wifi: !include linked/wifi.yaml\n")
    via = tmp_path / "via"
    via.symlink_to(project)
    CORE.config_path = via / "entry.yaml"
    discovered = discover_user_yaml_files(via / "entry.yaml")
    assert _symlink_descriptions(discovered) == ["linked -> ../shared"]
    files = _gather_redacted(discovered)
    assert "wifi: !include '../shared/wifi.yaml'" in files["entry.yaml"].decode()
    assert files["../shared/wifi.yaml"] == b"ssid: shared\n"


def test_gather_emits_every_alias_of_one_file(project: Path, shared: Path) -> None:
    """Every name linking to one file appears in the envelope, in both modes."""
    (shared / "sensor.yaml").write_bytes(b"platform: template\n")
    (project / "a.yaml").symlink_to(shared / "sensor.yaml")
    (project / "b.yaml").symlink_to(shared / "sensor.yaml")
    (project / "entry.yaml").write_text(
        "sensor:\n  - !include {file: a.yaml, vars: {n: 1}}\n"
        "  - !include {file: b.yaml, vars: {n: 2}}\n"
    )
    entries, _ = _gather_files(discover_user_yaml_files(project / "entry.yaml"))
    assert {rel for rel, _ in entries} == {"entry.yaml", "a.yaml", "b.yaml"}
    assert dict(_read_files_verbatim(entries))["b.yaml"] == b"platform: template\n"
    files = _gather_redacted(discover_user_yaml_files(project / "entry.yaml"))
    assert files["a.yaml"] == files["b.yaml"] == b"platform: template\n"


@pytest.mark.parametrize(
    ("files", "config", "in_log", "not_in_log"),
    [
        pytest.param(
            {"wifi.yaml": "password: esp32\nplatform: esp32\n"},
            {"wifi": [{"password": SensitiveStr("esp32")}]},
            "also matches the scalar at platform in wifi.yaml",
            "at password in wifi.yaml",
            id="unrelated_equal_scalar",
        ),
        pytest.param(
            {
                "wifi.yaml": "substitutions:\n  wifi_password: hunter2\nwifi:\n  password: ${wifi_password}\n"
            },
            {"wifi": [{"password": SensitiveStr("hunter2")}]},
            "",
            "also matches",
            id="substitution_definition",
        ),
        pytest.param(
            {
                "wifi.yaml": "ssid: Kitchen\nname: Kitchen Temperature\n",
                "secrets.yaml": "wifi_ssid: Kitchen\n",
            },
            {"wifi": [{"ssid": SensitiveStr("Kitchen")}]},
            "appears inside the value at name in wifi.yaml",
            "",
            id="secret_sourced_overlap",
        ),
    ],
)
def test_redacted_warns_without_failing(
    project: Path,
    caplog: pytest.LogCaptureFixture,
    files: dict[str, str],
    config: dict,
    in_log: str,
    not_in_log: str,
) -> None:
    """Collisions with a known value are swapped or kept with a warning, not an error."""
    out = _redact(project, files, config)
    assert b"!secret" in out["wifi.yaml"]
    assert in_log in caplog.text
    assert not not_in_log or not_in_log not in caplog.text


def test_uncaptured_note_lists_content() -> None:
    rel, content = _uncaptured_note(
        ["${board}.yaml"], ["https://github.com/org/repo@main"], ["linked -> ../shared"]
    )
    assert rel == UNCAPTURED_NOTE_PATH
    text = content.decode()
    assert text.startswith("# store_yaml:")
    for line in (
        "${board}.yaml",
        "https://github.com/org/repo@main",
        "linked -> ../shared",
    ):
        assert f"#   {line}" in text
    assert _uncaptured_note([], [], []) is None


def test_remote_package_descriptions() -> None:
    """Sources format as url@ref or url; userinfo is masked and a `!secret` URL named."""
    assert _remote_package_descriptions() == []
    data = packages._get_data()
    for url, ref in (
        ("https://github.com/org/repo", "main"),
        ("https://github.com/org/other", None),
        ("https://user:pw@host/repo", "main"),
        ("https://user:pw@host/private", None),
        ("https://github.com/org/x", "v1-secret"),
    ):
        data.remote_sources[packages.RemotePackageSource(url, ref)] = None
    yaml_util._SECRET_VALUES["https://user:pw@host/private"] = "pkg_url"
    yaml_util._SECRET_VALUES["v1-secret"] = "private_ref"
    assert _remote_package_descriptions() == [
        "https://github.com/org/repo@main",
        "https://github.com/org/other",
        "https://***@host/repo@main",
        "!secret pkg_url",
        "https://github.com/org/x@!secret private_ref",
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


def test_pack_envelope_rejects_duplicate_paths() -> None:
    with pytest.raises(EsphomeError, match="duplicate"):
        _pack_envelope([("entry.yaml", b"a: 1\n"), ("entry.yaml", b"b: 2\n")])


_ONE = _pack_envelope([("entry.yaml", b"esphome:\n")])
_ENTRY = struct.pack("<H", 6) + b"a.yaml" + struct.pack("<I", 1) + b"x"


@pytest.mark.parametrize(
    ("blob", "match"),
    [
        pytest.param(b"NOPE" + b"\x00" * 4, "magic", id="bad_magic"),
        *(
            pytest.param(_ONE[:cut], "corrupt", id=f"cut_{cut}")
            for cut in (5, 9, 12, -1)
        ),
        pytest.param(_ONE + b"\x00", "corrupt", id="trailing_bytes"),
        pytest.param(
            ENVELOPE_MAGIC
            + struct.pack("<I", 1)
            + struct.pack("<H", 2)
            + b"\xff\xfe"
            + struct.pack("<I", 0),
            "corrupt",
            id="invalid_utf8_path",
        ),
        pytest.param(
            ENVELOPE_MAGIC + struct.pack("<I", 2) + _ENTRY + _ENTRY,
            "rejected",
            id="duplicate",
        ),
        *(
            pytest.param(_pack_envelope([(path, b"boom\n")]), "rejected", id=path)
            for path in ("/etc/passwd", "\\evil.yaml", "C:/evil.yaml")
        ),
    ],
)
def test_unpack_envelope_rejects(blob: bytes, match: str) -> None:
    with pytest.raises(EsphomeError, match=match):
        unpack_envelope(blob)


@pytest.mark.parametrize(
    ("api", "ok"),
    [
        ({"encryption": {"key": "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8="}}, True),
        ({}, False),
        # Keyless encryption accepts the all-zeros key, so it does not count.
        ({"encryption": {}}, False),
    ],
    ids=["encrypted", "no_encryption", "keyless_encryption"],
)
def test_final_validate_requires_a_key(api: dict, ok: bool) -> None:
    config = {CONF_ALLOW_UNENCRYPTED: False}
    if ok:
        assert _run_final_validate({"api": api}, config) is config
        return
    with pytest.raises(cv.Invalid, match="requires API encryption"):
        _run_final_validate({"api": api}, config)


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


def test_redacted_rejects_a_secrets_file_that_includes_files(project: Path) -> None:
    """What a secrets file includes would ship verbatim beside its skeleton."""
    (project / "entry.yaml").write_text("wifi:\n  password: !secret wifi_password\n")
    (project / "secrets.yaml").write_text("<<: !include shared.yaml\n")
    (project / "shared.yaml").write_text("wifi_password: hunter2\n")
    with pytest.raises(EsphomeError, match="secrets.yaml loads shared.yaml"):
        _gather_redacted(discover_user_yaml_files(project / "entry.yaml"))
