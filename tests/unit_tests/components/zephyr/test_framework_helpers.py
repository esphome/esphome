"""Tests for esphome.components.zephyr.framework_helpers."""

from pathlib import Path
import sys
from unittest.mock import patch

import pytest

from esphome.components.zephyr import framework_helpers
from esphome.components.zephyr.framework_helpers import (
    ensure_venv,
    install_sitecustomize,
    needs_venv_rebuild,
    patch_gen_defines_dts_path,
    patch_west_build_source_dir_relpath,
)
from esphome.core import EsphomeError
from esphome.framework_helpers import get_python_env_executable_path


def test_needs_venv_rebuild_gates(tmp_path: Path) -> None:
    """The shared penv gate rebuilds on any missing or stale piece."""
    penv = tmp_path / "penv"
    penv.mkdir()
    python = penv / "python"
    sentinel = penv / ".ready"
    good_hash = "abc123"

    # Nothing in place yet
    assert needs_venv_rebuild(python, sentinel, good_hash)

    python.write_text("")
    # Interpreter present but no sentinel
    assert needs_venv_rebuild(python, sentinel, good_hash)

    sentinel.write_text(good_hash, encoding="utf-8")
    # Everything in place
    assert not needs_venv_rebuild(python, sentinel, good_hash)

    # Stale requirements hash
    assert needs_venv_rebuild(python, sentinel, "otherhash")


@pytest.mark.skipif(
    sys.platform == "win32", reason="symlink creation needs privileges on Windows"
)
def test_needs_venv_rebuild_on_dangling_interpreter_symlink(tmp_path: Path) -> None:
    """A cached venv restored after a host interpreter upgrade has a
    bin/python symlink whose target is gone; the valid sentinel must not
    mask it."""
    penv = tmp_path / "penv"
    penv.mkdir()
    python = penv / "python"
    sentinel = penv / ".ready"
    sentinel.write_text("abc123", encoding="utf-8")
    python.symlink_to(tmp_path / "hostedtoolcache" / "3.12.14" / "python3")
    assert python.is_symlink()
    assert not python.exists()

    assert needs_venv_rebuild(python, sentinel, "abc123")


def test_install_sitecustomize_writes_patch_on_windows(tmp_path: Path) -> None:
    with patch.object(framework_helpers.os, "name", "nt"):
        install_sitecustomize(tmp_path)

    written = tmp_path / "Lib" / "site-packages" / "sitecustomize.py"
    assert written.read_text(encoding="utf-8") == framework_helpers._SITECUSTOMIZE


def test_install_sitecustomize_does_nothing_elsewhere(tmp_path: Path) -> None:
    with patch.object(framework_helpers.os, "name", "posix"):
        install_sitecustomize(tmp_path)

    assert not any(tmp_path.iterdir())


def test_patch_gen_defines_relativizes_the_dts_path(tmp_path: Path) -> None:
    """The absolute dts.pre path is the only per device byte in the
    devicetree header; the patch makes gen_defines emit the basename."""
    gen = tmp_path / "zephyr" / "scripts" / "dts" / "gen_defines.py"
    gen.parent.mkdir(parents=True)
    gen.write_text("s = f'DTS input file:\\n  {edt.dts_path}\\n'\n")
    patch_gen_defines_dts_path(tmp_path)
    assert "{os.path.basename(edt.dts_path)}" in gen.read_text()
    assert not list(gen.parent.glob("*.tmp"))  # no leftovers
    before = gen.read_text()
    patch_gen_defines_dts_path(tmp_path)  # idempotent
    assert gen.read_text() == before
    patch_gen_defines_dts_path(tmp_path / "absent")  # tolerant


def test_patch_west_build_tolerates_a_source_dir_on_another_drive(
    tmp_path: Path,
) -> None:
    """West build's relative source path raises across Windows drives; the
    patch applies Zephyr main's fallback to the absolute path."""
    build = tmp_path / "zephyr" / "scripts" / "west_commands" / "build.py"
    build.parent.mkdir(parents=True)
    build.write_text(
        "    def _sanity_check_source_dir(self):\n"
        "        srcrel = os.path.relpath(self.source_dir)\n"
        "        self.check_force(True, srcrel)\n"
    )
    patch_west_build_source_dir_relpath(tmp_path)
    patched = build.read_text()
    assert "relative_to(pathlib.Path.cwd(), walk_up=True)" in patched
    assert (
        "        except ValueError:\n            srcrel = self.source_dir\n" in patched
    )
    assert "os.path.relpath" not in patched
    assert "        self.check_force(True, srcrel)\n" in patched
    assert not list(build.parent.glob("*.tmp"))
    patch_west_build_source_dir_relpath(tmp_path)  # idempotent
    assert build.read_text() == patched
    patch_west_build_source_dir_relpath(tmp_path / "absent")  # tolerant


def test_patch_gen_defines_warns_when_the_anchor_is_gone(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A reformatted upstream must not silently cost the sharing."""
    gen = tmp_path / "zephyr" / "scripts" / "dts" / "gen_defines.py"
    gen.parent.mkdir(parents=True)
    gen.write_text("s = 'something else entirely'\n")
    with caplog.at_level("WARNING"):
        patch_gen_defines_dts_path(tmp_path)
    assert "gen_defines.py no longer matches" in caplog.text


def _fake_create_venv(root: Path, msg: str | None = None) -> None:
    python = get_python_env_executable_path(root, "python")
    python.parent.mkdir(parents=True)
    python.touch()


def test_ensure_venv_builds_once_per_requirements(tmp_path: Path) -> None:
    """Built when missing, skipped while current, rebuilt when requirements change."""
    penv = tmp_path / "penv"
    requirements = tmp_path / "requirements.txt"
    requirements.write_text("west==1.5.0\n")
    with (
        patch.object(
            framework_helpers, "create_venv", side_effect=_fake_create_venv
        ) as create,
        patch.object(framework_helpers, "run_command_ok", return_value=True) as pip,
    ):
        assert ensure_venv(penv, requirements, "v1") is True
        assert ensure_venv(penv, requirements, "v1") is False
        requirements.write_text("west==1.6.0\n")
        assert ensure_venv(penv, requirements, "v1") is True

    assert create.call_count == 2
    assert pip.call_args.args[0][-2:] == ["-r", str(requirements)]


def test_ensure_venv_raises_when_pip_fails(tmp_path: Path) -> None:
    penv = tmp_path / "penv"
    requirements = tmp_path / "requirements.txt"
    requirements.write_text("west==1.5.0\n")
    with (
        patch.object(framework_helpers, "create_venv", side_effect=_fake_create_venv),
        patch.object(framework_helpers, "run_command_ok", return_value=False),
        pytest.raises(EsphomeError, match="Install requirements for v1"),
    ):
        ensure_venv(penv, requirements, "v1")

    assert not (penv / ".ready").exists()
