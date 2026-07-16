"""Tests for esphome.espidf.framework helpers."""

# pylint: disable=protected-access

from contextlib import contextmanager
import io
import json
import logging
from pathlib import Path
import sys
import tarfile
from types import SimpleNamespace
from unittest.mock import patch

import pytest

from esphome.espidf.framework import (
    _ccache_env,
    _check_stamp,
    _check_windows_path_length,
    _clone_idf_with_submodules,
    _get_framework_path,
    _get_idf_tool_paths,
    _get_idf_version,
    _get_python_env_path,
    _get_python_version,
    _parse_git_source,
    _patch_tools_json_for_linux_arm64,
    _windows_long_paths_enabled,
    _write_idf_version_txt,
    _write_stamp,
    check_esp_idf_install,
    get_framework_env,
    get_idf_tools_path,
)
from esphome.framework_helpers import _tar_extract_all, get_python_env_executable_path


@pytest.fixture(autouse=True)
def _isolate_idf_install_path(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    """Pin the ESP-IDF install root to a tmp dir for every test.

    The default location is the OS user cache dir, so without this any test
    that builds framework paths or pre-creates the framework dir would touch
    the real ``~/.cache/esphome`` on the developer's machine. Tests that need
    to exercise the override or default-resolution logic clear/override the env
    themselves.
    """
    monkeypatch.setenv("ESPHOME_ESP_IDF_PREFIX", str(tmp_path / "idf_install"))


@pytest.mark.parametrize(
    ("source", "expected"),
    [
        # github:// shorthand
        (
            "github://espressif/esp-idf",
            ("https://github.com/espressif/esp-idf.git", None),
        ),
        (
            "github://espressif/esp-idf@master",
            ("https://github.com/espressif/esp-idf.git", "master"),
        ),
        (
            "github://espressif/esp-idf@release/v6.0",
            ("https://github.com/espressif/esp-idf.git", "release/v6.0"),
        ),
        # explicit https://github.com/...git URL
        (
            "https://github.com/espressif/esp-idf.git",
            ("https://github.com/espressif/esp-idf.git", None),
        ),
        (
            "https://github.com/espressif/esp-idf.git@master",
            ("https://github.com/espressif/esp-idf.git", "master"),
        ),
        (
            "https://github.com/espressif/esp-idf.git@v6.0.1",
            ("https://github.com/espressif/esp-idf.git", "v6.0.1"),
        ),
        # '#' ref separator (PlatformIO/git-web convention) works on both forms
        (
            "https://github.com/espressif/esp-idf.git#release/v6.1",
            ("https://github.com/espressif/esp-idf.git", "release/v6.1"),
        ),
        (
            "github://espressif/esp-idf#release/v6.1",
            ("https://github.com/espressif/esp-idf.git", "release/v6.1"),
        ),
        (
            "github://espressif/esp-idf.git#master",
            ("https://github.com/espressif/esp-idf.git", "master"),
        ),
        # Tolerate a trailing ".git" on the shorthand so the user doesn't
        # silently end up with a doubled "...esp-idf.git.git" URL.
        (
            "github://espressif/esp-idf.git",
            ("https://github.com/espressif/esp-idf.git", None),
        ),
        (
            "github://espressif/esp-idf.git@master",
            ("https://github.com/espressif/esp-idf.git", "master"),
        ),
    ],
)
def test_parse_git_source_recognized(
    source: str, expected: tuple[str, str | None]
) -> None:
    assert _parse_git_source(source) == expected


@pytest.mark.parametrize(
    "source",
    [
        # archive URLs fall through to the existing download path
        "https://github.com/espressif/esp-idf/archive/refs/heads/master.zip",
        "https://dl.espressif.com/dl/esp-idf/v6.0.1/esp-idf-v6.0.1.zip",
        "https://github.com/esphome-libs/esp-idf/releases/download/v5.5.4/esp-idf-v5.5.4.tar.xz",
        # SSH and other git protocols are intentionally rejected — match
        # external_components, which only recognizes github:// + structured
        # dicts for these.
        "git@github.com:espressif/esp-idf.git",
        "ssh://git@github.com/espressif/esp-idf.git",
        "git://github.com/espressif/esp-idf.git",
        # non-GitHub .git URLs are intentionally rejected for the same reason
        "https://gitlab.com/foo/bar.git",
        "https://github.example.com/foo/bar.git",
    ],
)
def test_parse_git_source_rejected(source: str) -> None:
    assert _parse_git_source(source) is None


def _make_idf_tree(framework_path: Path) -> None:
    """Create the minimum tree _clone_idf_with_submodules sanity-checks for."""
    (framework_path / "tools").mkdir(parents=True)
    (framework_path / "tools" / "idf_tools.py").write_text("# stub\n")


def test_clone_idf_with_submodules_without_ref(tmp_path: Path) -> None:
    framework_path = tmp_path / "idf"
    framework_path.mkdir()
    _make_idf_tree(framework_path)

    with patch("esphome.git.run_git_command", return_value="") as run_git_command_mock:
        _clone_idf_with_submodules(
            framework_path, "https://github.com/espressif/esp-idf.git", None
        )

    # No ref -> just clone + submodule update, no fetch/reset.
    calls = [c.args[0] for c in run_git_command_mock.call_args_list]
    assert calls[0] == [
        "git",
        "clone",
        "--depth=1",
        "--",
        "https://github.com/espressif/esp-idf.git",
        str(framework_path),
    ]
    assert calls[-1][:5] == ["git", "submodule", "update", "--init", "--recursive"]
    assert not any(c[1] == "fetch" for c in calls)
    assert not any(c[1] == "reset" for c in calls)


def test_clone_idf_with_submodules_with_ref(tmp_path: Path) -> None:
    framework_path = tmp_path / "idf"
    framework_path.mkdir()
    _make_idf_tree(framework_path)

    with patch("esphome.git.run_git_command", return_value="") as run_git_command_mock:
        _clone_idf_with_submodules(
            framework_path,
            "https://github.com/espressif/esp-idf.git",
            "master",
        )

    calls = [c.args[0] for c in run_git_command_mock.call_args_list]
    # clone, fetch ref, reset hard, submodule update
    assert calls[0][:2] == ["git", "clone"]
    assert calls[1] == [
        "git",
        "fetch",
        "--depth=1",
        "--",
        "origin",
        "master",
    ]
    assert calls[2] == ["git", "reset", "--hard", "FETCH_HEAD"]
    assert calls[3][:5] == ["git", "submodule", "update", "--init", "--recursive"]


def test_clone_idf_with_submodules_raises_when_tree_missing(
    tmp_path: Path,
) -> None:
    framework_path = tmp_path / "idf"
    framework_path.mkdir()
    # Deliberately do NOT call _make_idf_tree — simulate a clone that
    # returned 0 but produced no tools/idf_tools.py.

    with (
        patch("esphome.git.run_git_command", return_value=""),
        pytest.raises(RuntimeError, match="no usable ESP-IDF tree"),
    ):
        _clone_idf_with_submodules(
            framework_path,
            "https://github.com/espressif/esp-idf.git",
            None,
        )


# ---------------------------------------------------------------------------
# Helpers for _tar_extract_all hard-link prefix-stripping tests
# ---------------------------------------------------------------------------


def _make_tar(
    members: list[tarfile.TarInfo], file_contents: dict[str, bytes]
) -> io.BytesIO:
    """Build an in-memory tar archive from a list of TarInfo objects."""
    buf = io.BytesIO()
    with tarfile.open(fileobj=buf, mode="w") as tf:
        for info in members:
            if info.isreg() and info.name in file_contents:
                data = file_contents[info.name]
                info.size = len(data)
                tf.addfile(info, io.BytesIO(data))
            else:
                tf.addfile(info)
    buf.seek(0)
    return buf


def _regular(name: str) -> tarfile.TarInfo:
    info = tarfile.TarInfo(name=name)
    info.type = tarfile.REGTYPE
    info.size = 0
    info.mode = 0o644
    return info


def _hardlink(name: str, linkname: str) -> tarfile.TarInfo:
    info = tarfile.TarInfo(name=name)
    info.type = tarfile.LNKTYPE
    info.linkname = linkname
    info.size = 0
    info.mode = 0o644
    return info


class TestTarExtractHardLinkPrefixStripping:
    """
    Covers the hard-link prefix-stripping block in _tar_extract_all (L528-541).

    Archive layout used by every test:

        wrapper/                   ← single top-level wrapper dir (stripped)
        wrapper/target.txt         ← regular file; becomes target.txt in dest
        wrapper/link_good          ← hard link to wrapper/target.txt  (kept, linkname stripped)
        wrapper/link_exact_root    ← hard link to "wrapper"            (skipped – equals strip_root)
        wrapper/link_exact_prefix  ← hard link to "wrapper/"           (skipped – equals strip_prefix)
        wrapper/link_outside       ← hard link to "other/target.txt"   (skipped – not under prefix)
    """

    WRAPPER = "wrapper"

    def _build_archive(self) -> io.BytesIO:
        members = [
            _regular(f"{self.WRAPPER}/"),
            _regular(f"{self.WRAPPER}/target.txt"),
            _hardlink(f"{self.WRAPPER}/link_good", f"{self.WRAPPER}/target.txt"),
            _hardlink(f"{self.WRAPPER}/link_exact_root", self.WRAPPER),
            _hardlink(f"{self.WRAPPER}/link_exact_prefix", f"{self.WRAPPER}/"),
            _hardlink(f"{self.WRAPPER}/link_outside", "other/target.txt"),
        ]
        return _make_tar(members, {f"{self.WRAPPER}/target.txt": b"hello"})

    def test_good_hardlink_is_extracted_with_stripped_linkname(
        self, tmp_path: Path
    ) -> None:
        """Hard link whose linkname starts with wrapper/ is extracted and its
        linkname has the prefix removed so tarfile can resolve the target."""
        _tar_extract_all(self._build_archive(), tmp_path)
        link = tmp_path / "link_good"
        assert link.exists(), "link_good should have been extracted"
        assert link.read_bytes() == b"hello"

    def test_hardlink_equal_to_strip_root_is_skipped(self, tmp_path: Path) -> None:
        """Hard link whose linkname equals strip_root exactly must be dropped."""
        _tar_extract_all(self._build_archive(), tmp_path)
        assert not (tmp_path / "link_exact_root").exists()

    def test_hardlink_equal_to_strip_prefix_is_skipped(self, tmp_path: Path) -> None:
        """Hard link whose linkname equals strip_prefix (strip_root + '/') must be dropped."""
        _tar_extract_all(self._build_archive(), tmp_path)
        assert not (tmp_path / "link_exact_prefix").exists()

    def test_hardlink_outside_prefix_is_skipped(self, tmp_path: Path) -> None:
        """Hard link whose linkname does not start with wrapper/ must be dropped."""
        _tar_extract_all(self._build_archive(), tmp_path)
        assert not (tmp_path / "link_outside").exists()

    def test_regular_file_and_no_spurious_files(self, tmp_path: Path) -> None:
        """Sanity check: target.txt is extracted and no unexpected files appear."""
        _tar_extract_all(self._build_archive(), tmp_path)
        assert (tmp_path / "target.txt").read_bytes() == b"hello"
        extracted = {p.name for p in tmp_path.iterdir()}
        assert extracted == {"target.txt", "link_good"}


_IDF_VERSION = "5.1.2"


@pytest.fixture
def espidf_mocks(setup_core: Path):
    """Patch the heavy I/O of check_esp_idf_install and pre-create the framework dir."""
    # archive_extract_all is mocked, so pre-create the framework dir that the
    # extracted-marker touch writes into.
    _get_framework_path(_IDF_VERSION).mkdir(parents=True, exist_ok=True)
    with (
        patch("esphome.espidf.framework.rmdir"),
        patch(
            "esphome.espidf.framework.download_from_mirrors",
            return_value="https://example.com/idf.tar.xz",
        ) as download,
        patch("esphome.espidf.framework.archive_extract_all") as extract,
        patch("esphome.espidf.framework.create_venv") as venv,
        patch("esphome.espidf.framework.run_command_ok", return_value=True) as run_ok,
        patch(
            "esphome.espidf.framework._get_idf_tool_paths", return_value=([], {})
        ) as tool_paths,
        patch("esphome.espidf.framework._clone_idf_with_submodules") as clone,
        patch("esphome.espidf.framework._write_idf_version_txt"),
        patch("esphome.espidf.framework._patch_tools_json_for_linux_arm64"),
        patch("esphome.espidf.framework._write_stamp"),
        patch("esphome.espidf.framework._check_stamp", return_value=True),
        patch("esphome.espidf.framework._get_idf_version", return_value=_IDF_VERSION),
        patch("esphome.espidf.framework._get_python_version", return_value="3.11.0"),
        patch("esphome.espidf.framework.get_system_python_path", return_value="python"),
    ):
        yield SimpleNamespace(
            download=download,
            extract=extract,
            venv=venv,
            run_ok=run_ok,
            tool_paths=tool_paths,
            clone=clone,
        )


def test_check_esp_idf_install_fresh(espidf_mocks: SimpleNamespace) -> None:
    """A forced install drives download/extract, venv creation, and pip installs."""
    framework_path, python_env_path = check_esp_idf_install(_IDF_VERSION, force=True)

    assert framework_path == _get_framework_path(_IDF_VERSION)
    assert python_env_path == _get_python_env_path(_IDF_VERSION)
    # framework tarball + python-env constraints file are both downloaded
    assert espidf_mocks.download.call_count == 2
    espidf_mocks.extract.assert_called_once()
    espidf_mocks.venv.assert_called_once()
    espidf_mocks.clone.assert_not_called()


def test_check_esp_idf_install_git_source(espidf_mocks: SimpleNamespace) -> None:
    """A git source_url clones instead of downloading; explicit tools skip discovery."""
    check_esp_idf_install(
        _IDF_VERSION,
        force=True,
        source_url="https://github.com/espressif/esp-idf.git",
        tools=["xtensa-esp-elf"],
    )

    espidf_mocks.clone.assert_called_once()
    # framework is cloned, so only the python-env constraints file is downloaded
    assert espidf_mocks.download.call_count == 1


def test_check_esp_idf_install_already_installed(espidf_mocks: SimpleNamespace) -> None:
    """Marker + matching stamps + existing python env → nothing is re-installed."""
    framework_path = _get_framework_path(_IDF_VERSION)
    (framework_path / ".esphome_extracted").touch()
    python_env_path = _get_python_env_path(_IDF_VERSION)
    env_python = get_python_env_executable_path(python_env_path, "python")
    env_python.parent.mkdir(parents=True, exist_ok=True)
    env_python.touch()

    check_esp_idf_install(_IDF_VERSION)

    espidf_mocks.extract.assert_not_called()
    espidf_mocks.venv.assert_not_called()


def test_check_esp_idf_install_framework_failure(espidf_mocks: SimpleNamespace) -> None:
    """A failing idf_tools install raises."""
    espidf_mocks.run_ok.side_effect = [False]
    with pytest.raises(RuntimeError, match="framework installation failure"):
        check_esp_idf_install(_IDF_VERSION, force=True)


def test_check_esp_idf_install_pip_upgrade_failure(
    espidf_mocks: SimpleNamespace,
) -> None:
    """A failing pip upgrade in the python env raises (framework install ok)."""
    espidf_mocks.run_ok.side_effect = [True, False]
    with pytest.raises(RuntimeError, match="Python environment packages failure"):
        check_esp_idf_install(_IDF_VERSION, force=True)


def test_check_esp_idf_install_feature_failure(espidf_mocks: SimpleNamespace) -> None:
    """A failing feature requirements install raises."""
    espidf_mocks.run_ok.side_effect = [True, True, False]
    with pytest.raises(RuntimeError, match="Python dependencies for"):
        check_esp_idf_install(_IDF_VERSION, force=True, features=["fb"])


def _mark_installed() -> None:
    """Create the extracted marker and python-env interpreter so the install
    check takes the already-installed path rather than force-installing."""
    (_get_framework_path(_IDF_VERSION) / ".esphome_extracted").touch()
    env_python = get_python_env_executable_path(
        _get_python_env_path(_IDF_VERSION), "python"
    )
    env_python.parent.mkdir(parents=True, exist_ok=True)
    env_python.touch()


def test_check_esp_idf_install_stamp_mismatch_reinstalls(
    espidf_mocks: SimpleNamespace,
) -> None:
    """A stamp mismatch reinstalls tools (marker present, so no re-extract)."""
    _mark_installed()
    with patch("esphome.espidf.framework._check_stamp", return_value=False):
        check_esp_idf_install(_IDF_VERSION)

    espidf_mocks.extract.assert_not_called()  # marker present -> no re-extract
    espidf_mocks.venv.assert_called_once()  # tools reinstall -> venv rebuilt


def test_check_esp_idf_install_check_command_failure_reinstalls(
    espidf_mocks: SimpleNamespace,
) -> None:
    """A failing tool-path resolution reinstalls tools (marker present, no re-extract)."""
    _mark_installed()
    # Managed tool resolution fails -> install stays True; the later installs succeed.
    espidf_mocks.tool_paths.side_effect = RuntimeError("missing ESP-IDF tool")
    check_esp_idf_install(_IDF_VERSION, features=["fb"])

    espidf_mocks.extract.assert_not_called()
    espidf_mocks.venv.assert_called_once()


def test_check_esp_idf_install_unknown_python_version_reinstalls(
    espidf_mocks: SimpleNamespace,
) -> None:
    """An undeterminable python version rebuilds the venv (framework stamp still ok)."""
    _mark_installed()
    with patch("esphome.espidf.framework._get_python_version", return_value=None):
        check_esp_idf_install(_IDF_VERSION)

    espidf_mocks.extract.assert_not_called()  # framework stamp matched
    espidf_mocks.venv.assert_called_once()  # python env rebuilt


def test_check_esp_idf_install_python_stamp_mismatch_rebuilds_venv(
    espidf_mocks: SimpleNamespace,
) -> None:
    """Framework stamp matches but the python-env stamp does not -> venv rebuilt."""

    # _check_stamp passes for the framework (no python_version key) and fails
    # for the python env (carries python_version), so only the venv rebuilds.
    def stamp_ok(_stamp_file, info: dict) -> bool:
        return "python_version" not in info

    _mark_installed()
    with patch("esphome.espidf.framework._check_stamp", side_effect=stamp_ok):
        check_esp_idf_install(_IDF_VERSION)

    espidf_mocks.extract.assert_not_called()
    espidf_mocks.venv.assert_called_once()


def test_check_esp_idf_install_unparseable_version(
    espidf_mocks: SimpleNamespace,
) -> None:
    """A non-semver version skips the MAJOR/MINOR substitutions without erroring."""
    bad_version = "main"
    _get_framework_path(bad_version).mkdir(parents=True, exist_ok=True)
    check_esp_idf_install(bad_version, force=True)

    espidf_mocks.extract.assert_called_once()


@pytest.mark.parametrize(
    ("version", "short_version"),
    [
        ("6.0.0", "6.0"),
        ("6.0.0-rc1", "6.0-rc1"),
        ("5.5.4", None),  # vX.Y tags only exist for X.Y.0 releases
    ],
)
def test_check_esp_idf_install_short_version_substitution(
    espidf_mocks: SimpleNamespace, version: str, short_version: str | None
) -> None:
    """SHORT_VERSION is only offered for x.y.0 releases, so the vX.Y mirror
    template is never tried for versions whose tag cannot exist."""
    _get_framework_path(version).mkdir(parents=True, exist_ok=True)
    check_esp_idf_install(version, force=True)

    # First call downloads the framework archive; a later call fetches the
    # constraints file with its own substitutions.
    substitutions = espidf_mocks.download.call_args_list[0][0][1]
    assert substitutions.get("SHORT_VERSION") == short_version
    assert substitutions["VERSION"] == version


# ---------------------------------------------------------------------------
# _patch_tools_json_for_linux_arm64 (arm64-only ninja backport)
# ---------------------------------------------------------------------------


def _write_tools_json(framework_path: Path, data: dict) -> Path:
    tools_dir = framework_path / "tools"
    tools_dir.mkdir(parents=True, exist_ok=True)
    tools_json = tools_dir / "tools.json"
    tools_json.write_text(json.dumps(data), encoding="utf-8")
    return tools_json


def test_patch_tools_json_non_aarch64_is_noop(tmp_path: Path) -> None:
    tools_json = _write_tools_json(
        tmp_path, {"tools": [{"name": "ninja", "versions": [{"name": "1.12.1"}]}]}
    )
    before = tools_json.read_text(encoding="utf-8")
    with patch("esphome.espidf.framework.platform.machine", return_value="x86_64"):
        _patch_tools_json_for_linux_arm64(tmp_path)
    assert tools_json.read_text(encoding="utf-8") == before


def test_patch_tools_json_missing_file_is_noop(tmp_path: Path) -> None:
    with patch("esphome.espidf.framework.platform.machine", return_value="aarch64"):
        _patch_tools_json_for_linux_arm64(tmp_path)  # no tools/tools.json present


def test_patch_tools_json_corrupt_file_warns_and_skips(tmp_path: Path) -> None:
    (tmp_path / "tools").mkdir()
    (tmp_path / "tools" / "tools.json").write_text("{ not json", encoding="utf-8")
    with patch("esphome.espidf.framework.platform.machine", return_value="aarch64"):
        _patch_tools_json_for_linux_arm64(tmp_path)  # JSONDecodeError -> skip


def test_patch_tools_json_injects_ninja_arm64(tmp_path: Path) -> None:
    tools_json = _write_tools_json(
        tmp_path,
        {
            "tools": [
                {"name": "ninja", "versions": [{"name": "1.12.1"}]},
                {"name": "cmake", "versions": [{"name": "3.24.0"}]},
            ]
        },
    )
    with patch("esphome.espidf.framework.platform.machine", return_value="aarch64"):
        _patch_tools_json_for_linux_arm64(tmp_path)

    data = json.loads(tools_json.read_text(encoding="utf-8"))
    ninja = next(t for t in data["tools"] if t["name"] == "ninja")
    assert "linux-arm64" in ninja["versions"][0]
    assert ninja["versions"][0]["linux-arm64"]["size"] == 121787


def test_patch_tools_json_already_patched_is_noop(tmp_path: Path) -> None:
    tools_json = _write_tools_json(
        tmp_path,
        {
            "tools": [
                {
                    "name": "ninja",
                    "versions": [{"name": "1.12.1", "linux-arm64": {"url": "x"}}],
                }
            ]
        },
    )
    before = tools_json.read_text(encoding="utf-8")
    with patch("esphome.espidf.framework.platform.machine", return_value="aarch64"):
        _patch_tools_json_for_linux_arm64(tmp_path)
    assert tools_json.read_text(encoding="utf-8") == before


# ---------------------------------------------------------------------------
# Subprocess-backed helpers (_exec -> run_command rename) and get_framework_env
# ---------------------------------------------------------------------------


def test_get_idf_version_parses_stdout(tmp_path: Path) -> None:
    with patch(
        "esphome.espidf.framework.run_command", return_value=(True, "5.1.2\n", "")
    ):
        assert _get_idf_version(tmp_path) == "5.1.2"


def test_get_idf_version_raises_on_failure(tmp_path: Path) -> None:
    with (
        patch("esphome.espidf.framework.run_command", return_value=(False, "", "boom")),
        pytest.raises(RuntimeError, match="Can't get ESP-IDF version"),
    ):
        _get_idf_version(tmp_path)


def test_get_idf_tool_paths_parses_json(tmp_path: Path) -> None:
    payload = json.dumps({"paths_to_export": ["/a", "/b"], "export_vars": {"X": "1"}})
    with patch(
        "esphome.espidf.framework.run_command", return_value=(True, payload, "")
    ):
        paths, export_vars = _get_idf_tool_paths(tmp_path)
    assert paths == ["/a", "/b"]
    assert export_vars == {"X": "1"}


def test_get_idf_tool_paths_raises_on_bad_json(tmp_path: Path) -> None:
    with (
        patch(
            "esphome.espidf.framework.run_command", return_value=(True, "not json", "")
        ),
        pytest.raises(RuntimeError, match="Can't extract ESP-IDF tool paths"),
    ):
        _get_idf_tool_paths(tmp_path)


def test_get_idf_tool_paths_raises_on_failure(tmp_path: Path) -> None:
    with (
        patch("esphome.espidf.framework.run_command", return_value=(False, "", "err")),
        pytest.raises(RuntimeError, match="Can't get ESP-IDF tool paths"),
    ):
        _get_idf_tool_paths(tmp_path)


def test_get_python_version_parses_stdout(tmp_path: Path) -> None:
    with patch(
        "esphome.espidf.framework.run_command", return_value=(True, "3.11.0\n", "")
    ):
        assert _get_python_version(tmp_path / "python") == "3.11.0"


def test_get_python_version_returns_falsy_on_failure(tmp_path: Path) -> None:
    with patch("esphome.espidf.framework.run_command", return_value=(False, "", "")):
        # non-throwing failure returns the (empty) stdout as-is
        assert not _get_python_version(tmp_path / "python")


def test_get_python_version_raises_when_requested(tmp_path: Path) -> None:
    with (
        patch("esphome.espidf.framework.run_command", return_value=(False, "", "")),
        pytest.raises(RuntimeError, match="Can't get Python version"),
    ):
        _get_python_version(tmp_path / "python", throw_exception=True)


def test_write_stamp_writes_json(tmp_path: Path) -> None:
    stamp = tmp_path / "stamp.json"
    _write_stamp(stamp, {"a": "1", "b": "2"})
    assert json.loads(stamp.read_text(encoding="utf-8")) == {"a": "1", "b": "2"}


def test_get_framework_env_with_python_env(tmp_path: Path) -> None:
    with (
        patch(
            "esphome.espidf.framework.get_idf_tools_path",
            return_value=tmp_path / "tools",
        ),
        patch("esphome.espidf.framework._get_idf_version", return_value="5.1.2"),
        patch(
            "esphome.espidf.framework._get_idf_tool_paths",
            return_value=(["/tool/bin"], {"IDF_X": "1"}),
        ),
        # ccache env is covered separately; keep this test host-independent.
        patch("esphome.espidf.framework._ccache_env", return_value={}),
    ):
        env = get_framework_env(
            tmp_path / "fw", tmp_path / "penv", {"PATH": "/usr/bin"}
        )

    assert env["IDF_PATH"] == str(tmp_path / "fw")
    assert env["ESP_IDF_VERSION"] == "5.1.2"
    assert env["IDF_X"] == "1"
    assert env["IDF_PYTHON_ENV_PATH"] == str(tmp_path / "penv")
    assert "/tool/bin" in env["PATH"]


def test_get_framework_env_without_python_env_uses_os_path(tmp_path: Path) -> None:
    with (
        patch(
            "esphome.espidf.framework.get_idf_tools_path",
            return_value=tmp_path / "tools",
        ),
        patch("esphome.espidf.framework._get_idf_version", return_value="5.1.2"),
        patch("esphome.espidf.framework._get_idf_tool_paths", return_value=([], {})),
        # ccache env is covered separately; keep this test host-independent.
        patch("esphome.espidf.framework._ccache_env", return_value={}),
    ):
        env = get_framework_env(tmp_path / "fw")

    assert "IDF_PYTHON_ENV_PATH" not in env
    assert env["PATH"]  # taken from os.environ


# ---------------------------------------------------------------------------
# _ccache_env
# ---------------------------------------------------------------------------


def _ccache_patches(tmp_path: Path, which: str | None, build_path: Path | None):
    return (
        patch("esphome.espidf.framework.shutil.which", return_value=which),
        patch(
            "esphome.espidf.framework.get_idf_tools_path",
            return_value=tmp_path / "tools",
        ),
        patch(
            "esphome.espidf.framework.CORE",
            SimpleNamespace(build_path=build_path),
        ),
    )


def test_ccache_env_default_enabled_when_available(tmp_path: Path) -> None:
    p1, p2, p3 = _ccache_patches(tmp_path, "/usr/bin/ccache", tmp_path / "build")
    with patch.dict("os.environ", {}, clear=True), p1, p2, p3:
        env = _ccache_env()
    assert env["IDF_CCACHE_ENABLE"] == "1"
    assert env["CCACHE_DIR"] == str(tmp_path / "tools" / "ccache")
    assert env["CCACHE_NOHASHDIR"] == "true"
    assert env["CCACHE_DEPEND"] == "1"
    assert env["CCACHE_BASEDIR"] == str((tmp_path / "build").resolve())


def test_ccache_env_disabled_when_binary_missing(tmp_path: Path) -> None:
    # build_path is None here too: a disabled cache must not require it.
    p1, p2, p3 = _ccache_patches(tmp_path, None, None)
    with patch.dict("os.environ", {}, clear=True), p1, p2, p3:
        assert _ccache_env() == {}


def test_ccache_env_opt_out_via_env(tmp_path: Path) -> None:
    # Explicit IDF_CCACHE_ENABLE=0 wins even when the binary is present, and
    # short-circuits before build_path is needed.
    p1, p2, p3 = _ccache_patches(tmp_path, "/usr/bin/ccache", None)
    with patch.dict("os.environ", {"IDF_CCACHE_ENABLE": "0"}, clear=True), p1, p2, p3:
        assert _ccache_env() == {}


def test_ccache_env_opt_in_without_binary(tmp_path: Path) -> None:
    # Explicit IDF_CCACHE_ENABLE=1 forces it on without probing PATH. It's
    # already in the environment, so it isn't re-emitted, but the rest is.
    p1, p2, p3 = _ccache_patches(tmp_path, None, tmp_path / "build")
    with patch.dict("os.environ", {"IDF_CCACHE_ENABLE": "1"}, clear=True), p1, p2, p3:
        env = _ccache_env()
    assert "IDF_CCACHE_ENABLE" not in env
    assert env["CCACHE_DIR"] == str(tmp_path / "tools" / "ccache")
    assert env["CCACHE_DEPEND"] == "1"


def test_ccache_env_preserves_user_overrides(tmp_path: Path) -> None:
    # User-set CCACHE_* values must not be clobbered; unset ones still default.
    p1, p2, p3 = _ccache_patches(tmp_path, "/usr/bin/ccache", tmp_path / "build")
    user_env = {"CCACHE_DIR": "/my/cache", "CCACHE_MAXSIZE": "9G"}
    with patch.dict("os.environ", user_env, clear=True), p1, p2, p3:
        env = _ccache_env()
    assert "CCACHE_DIR" not in env
    assert "CCACHE_MAXSIZE" not in env
    assert env["IDF_CCACHE_ENABLE"] == "1"
    assert env["CCACHE_DEPEND"] == "1"


def test_ccache_env_raises_without_build_path(tmp_path: Path) -> None:
    # Enabled but no build_path means the IDF env was built too early -- fail
    # loudly instead of silently dropping CCACHE_BASEDIR.
    p1, p2, p3 = _ccache_patches(tmp_path, "/usr/bin/ccache", None)
    with (
        patch.dict("os.environ", {}, clear=True),
        p1,
        p2,
        p3,
        pytest.raises(ValueError, match="build_path"),
    ):
        _ccache_env()


# ---------------------------------------------------------------------------
# _check_stamp / _write_idf_version_txt / get_idf_tools_path
# ---------------------------------------------------------------------------


def test_check_stamp_matches(tmp_path: Path) -> None:
    f = tmp_path / "s.json"
    f.write_text(json.dumps({"a": "1"}), encoding="utf-8")
    assert _check_stamp(f, {"a": "1"}) is True


def test_check_stamp_mismatch(tmp_path: Path) -> None:
    f = tmp_path / "s.json"
    f.write_text(json.dumps({"a": "1"}), encoding="utf-8")
    assert _check_stamp(f, {"a": "2"}) is False


def test_check_stamp_missing_file(tmp_path: Path) -> None:
    assert _check_stamp(tmp_path / "nope.json", {"a": "1"}) is False


def test_check_stamp_corrupt_file(tmp_path: Path) -> None:
    f = tmp_path / "s.json"
    f.write_text("{ not json", encoding="utf-8")
    assert _check_stamp(f, {"a": "1"}) is False


def test_write_idf_version_txt_writes_when_missing(tmp_path: Path) -> None:
    _write_idf_version_txt(tmp_path, "5.1.2")
    assert (tmp_path / "version.txt").read_text(encoding="utf-8") == "v5.1.2\n"


def test_write_idf_version_txt_skips_when_present(tmp_path: Path) -> None:
    (tmp_path / "version.txt").write_text("existing\n", encoding="utf-8")
    _write_idf_version_txt(tmp_path, "5.1.2")
    assert (tmp_path / "version.txt").read_text(encoding="utf-8") == "existing\n"


def testget_idf_tools_path_env_override(tmp_path: Path) -> None:
    override = str(tmp_path / "custom-idf")
    with patch.dict("os.environ", {"ESPHOME_ESP_IDF_PREFIX": override}):
        assert get_idf_tools_path() == Path(override)


@pytest.mark.parametrize("value", ["", "   "])
def testget_idf_tools_path_blank_env_falls_back_to_default(
    value: str, monkeypatch: pytest.MonkeyPatch
) -> None:
    """A blank ESPHOME_ESP_IDF_PREFIX is treated as unset, not as CWD.

    Path("") would resolve to the working directory, which clean-all could then
    delete by accident.
    """
    import platformdirs

    monkeypatch.setenv("ESPHOME_ESP_IDF_PREFIX", value)
    expected = (
        Path(platformdirs.user_cache_dir("esphome", appauthor=False)) / "idf"
    ).resolve()
    assert get_idf_tools_path() == expected


def testget_idf_tools_path_default_uses_user_cache(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Without the env override the install root is the machine-global OS user
    cache dir, not the per-config ``<data_dir>/idf``."""
    import platformdirs

    monkeypatch.delenv("ESPHOME_ESP_IDF_PREFIX", raising=False)
    expected = (
        Path(platformdirs.user_cache_dir("esphome", appauthor=False)) / "idf"
    ).resolve()
    assert get_idf_tools_path() == expected


def test_write_idf_version_txt_warns_on_write_error(tmp_path: Path) -> None:
    with patch("pathlib.Path.write_text", side_effect=OSError("denied")):
        # write failure is caught and warned, not raised
        _write_idf_version_txt(tmp_path, "5.1.2")


def _fake_winreg(
    query_result: int | None = None, query_error: OSError | None = None
) -> SimpleNamespace:
    """Build a minimal winreg stand-in (the real module is Windows-only)."""

    @contextmanager
    def open_key(root, path):
        yield "hkey"

    def query_value_ex(key, name):
        if query_error is not None:
            raise query_error
        return query_result, 4  # (value, REG_DWORD)

    return SimpleNamespace(
        HKEY_LOCAL_MACHINE=object(),
        OpenKey=open_key,
        QueryValueEx=query_value_ex,
    )


@pytest.mark.parametrize(("reg_value", "expected"), [(1, True), (0, False)])
def test_windows_long_paths_enabled_reads_registry(
    reg_value: int, expected: bool
) -> None:
    with patch.dict(sys.modules, {"winreg": _fake_winreg(query_result=reg_value)}):
        assert _windows_long_paths_enabled() is expected


def test_windows_long_paths_enabled_missing_value() -> None:
    """A missing registry value (FileNotFoundError is an OSError) reads as disabled."""
    fake = _fake_winreg(query_error=FileNotFoundError("no such value"))
    with patch.dict(sys.modules, {"winreg": fake}):
        assert _windows_long_paths_enabled() is False


# 8 chars -> projected well under the 260 limit even with the ~245-char reserve
_SHORT_IDF_PATH = "C:\\e\\idf"
# 25 chars -> projected over the limit
_LONG_IDF_PATH = "C:\\Users\\bob\\.esphome\\idf"


def test_check_windows_path_length_noop_off_windows(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Off Windows the check returns before touching the registry or the path."""
    with (
        patch("esphome.espidf.framework.platform.system", return_value="Linux"),
        patch(
            "esphome.espidf.framework._windows_long_paths_enabled"
        ) as long_paths_mock,
        caplog.at_level(logging.WARNING),
    ):
        _check_windows_path_length()
    long_paths_mock.assert_not_called()
    assert not caplog.records


def test_check_windows_path_length_noop_when_long_paths_enabled(
    caplog: pytest.LogCaptureFixture,
) -> None:
    with (
        patch("esphome.espidf.framework.platform.system", return_value="Windows"),
        patch(
            "esphome.espidf.framework._windows_long_paths_enabled", return_value=True
        ),
        patch("esphome.espidf.framework.get_idf_tools_path") as get_path_mock,
        caplog.at_level(logging.WARNING),
    ):
        _check_windows_path_length()
    get_path_mock.assert_not_called()
    assert not caplog.records


def test_check_windows_path_length_short_path_silent(
    caplog: pytest.LogCaptureFixture,
) -> None:
    with (
        patch("esphome.espidf.framework.platform.system", return_value="Windows"),
        patch(
            "esphome.espidf.framework._windows_long_paths_enabled", return_value=False
        ),
        patch(
            "esphome.espidf.framework.get_idf_tools_path",
            return_value=_SHORT_IDF_PATH,
        ),
        caplog.at_level(logging.WARNING),
    ):
        _check_windows_path_length()
    assert not caplog.records


def test_check_windows_path_length_long_path_warns(
    caplog: pytest.LogCaptureFixture,
) -> None:
    with (
        patch("esphome.espidf.framework.platform.system", return_value="Windows"),
        patch(
            "esphome.espidf.framework._windows_long_paths_enabled", return_value=False
        ),
        patch(
            "esphome.espidf.framework.get_idf_tools_path",
            return_value=_LONG_IDF_PATH,
        ),
        caplog.at_level(logging.WARNING),
    ):
        _check_windows_path_length()
    assert len(caplog.records) == 1
    message = caplog.records[0].getMessage()
    assert _LONG_IDF_PATH in message
    assert "long path support" in message
    # The install is global now; the remedy is the prefix env, not moving the project.
    assert "ESPHOME_ESP_IDF_PREFIX" in message
