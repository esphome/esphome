"""Tests for esphome/platformio/pch.py.script against a fake SCons env."""

from __future__ import annotations

import os
from pathlib import Path
import stat
from unittest.mock import patch

import pytest

from esphome.platformio import toolchain

pytestmark = pytest.mark.skipif(
    os.name == "nt", reason="the fake compiler is a POSIX shell script"
)

_SCRIPT = Path(toolchain.__file__).parent / "pch.py.script"


class _FakePlatform:
    packages = {"framework-x": {}, "toolchain-y": {}}

    def get_package_version(self, name: str) -> str:
        if name == "toolchain-y":
            raise KeyError(name)
        return "1.2.3"

    def get_package(self, name: str) -> object | None:
        return None


class _BrokenPlatform(_FakePlatform):
    def get_package_version(self, name: str) -> str:
        raise RuntimeError("manifest parse error")


class _UnresolvedPlatform(_FakePlatform):
    """KeyError from a package that IS installed: unresolved identity."""

    def get_package_version(self, name: str) -> str:
        raise KeyError(name)

    def get_package(self, name: str) -> object:
        return object()


class _FakeSConsEnv(dict):
    """Just enough of a SCons construction environment for pch.py."""

    def __init__(
        self,
        proj_dir: Path,
        src_dir: Path,
        cxx: str,
        flags: list[str],
        platform_cls: type[_FakePlatform] = _FakePlatform,
    ):
        super().__init__(ENV={})
        self._subst = {
            "$PROJECT_DIR": str(proj_dir),
            "$PROJECT_SRC_DIR": str(src_dir),
            "$CXX": cxx,
        }
        self._flags = flags
        self._platform_cls = platform_cls
        self.prepended: list[str] = []

    def subst(self, expr: str) -> str:  # noqa: N802
        return self._subst[expr]

    def subst_list(self, expr: str) -> list[list[str]]:  # noqa: N802
        return [self._flags]

    def PioPlatform(self) -> _FakePlatform:  # noqa: N802
        return self._platform_cls()

    def Prepend(self, CXXFLAGS: list[str]) -> None:  # noqa: N802, N803
        self.prepended = CXXFLAGS


def _fake_cxx(
    tmp_path: Path,
    fail: bool = False,
    fail_msg: str | None = None,
    reject_pch: bool = False,
    probe_exit: int = 0,
) -> Path:
    """A compiler stand-in that records its argv and writes the -o target.

    With reject_pch it builds the .gch fine but, like GCC 10 on macOS arm64,
    warns on any consuming compile that the .gch cannot be loaded; probe_exit
    sets the exit code of non-header compiles (the load probe).
    """
    cxx = tmp_path / "fake-gxx"
    body = (
        'printf -- ---call---\\\\n >> "$0.argv"; printf \'%s\\n\' "$@" >> "$0.argv"\n'
    )
    if fail:
        body += f"echo {fail_msg or 'boom'} >&2\nexit 1\n"
    else:
        # Only the c++-header compile has a -o; the load probe has none
        body += 'out=""; prev=""; mf=0; dep=0; inc=0\nfor a in "$@"; do [ "$prev" = "-o" ] && out="$a"; prev="$a"; [ "$a" = "-MF" ] && mf=1; [ "$a" = "-include" ] && inc=1; case "$a" in -M|-MM|-MD|-MMD) dep=1;; esac; done\n'
        # Real cc1plus rejects -MF without a dependency flag
        body += 'if [ "$mf" = 1 ] && [ "$dep" = 0 ]; then echo "cc1plus: error: to generate dependencies you must specify either \x27-M\x27 or \x27-MM\x27" >&2; exit 1; fi\n'
        body += '[ -n "$out" ] && echo gch > "$out"\n'
        if reject_pch:
            # -Werror=invalid-pch makes rejection a nonzero exit; the
            # baseline (no -include) still passes
            body += 'case " $* " in *c++-header*) ;; *) if [ "$inc" = 1 ]; then echo "error: esphome_pch.h.gch: had text segment at different address" >&2; exit 1; fi;; esac\n'
        body += f'case " $* " in *c++-header*) exit 0;; *) exit {probe_exit};; esac\n'
    cxx.write_text("#!/bin/sh\n" + body)
    cxx.chmod(cxx.stat().st_mode | stat.S_IEXEC)
    return cxx


def _run_script(
    tmp_path: Path,
    flags: list[str] | None = None,
    fail: bool = False,
    fail_msg: str | None = None,
    reject_pch: bool = False,
    probe_exit: int = 0,
    missing_cxx: bool = False,
    env_vars: dict[str, str] | None = None,
    name: str = "dev",
    platform_cls: type[_FakePlatform] = _FakePlatform,
) -> _FakeSConsEnv:
    proj = tmp_path / name
    src = proj / "src"
    (src / "esphome" / "core").mkdir(parents=True, exist_ok=True)
    (src / "esphome" / "core" / "defines.h").write_text("#define USE_X\n")
    cxx = _fake_cxx(
        tmp_path,
        fail=fail,
        fail_msg=fail_msg,
        reject_pch=reject_pch,
        probe_exit=probe_exit,
    )
    if missing_cxx:
        cxx = tmp_path / "no-such-gxx"
    args = (proj, src, str(cxx), flags or ["-DX=1"], platform_cls)
    # Distinct objects: the -include flags must land on projenv only
    global_env = _FakeSConsEnv(*args)
    projenv = _FakeSConsEnv(*args)
    projenv.global_env = global_env
    source = _SCRIPT.read_text()
    with patch.dict(os.environ, env_vars or {}, clear=True):
        exec(  # noqa: S102
            compile(source, "pch.py", "exec"),
            {"Import": lambda *_names: None, "env": global_env, "projenv": projenv},
        )
    return projenv


def test_pch_script_builds_and_prepends_relative_include(tmp_path: Path) -> None:
    scons_env = _run_script(tmp_path)
    proj = tmp_path / "dev"
    assert (proj / "esphome_pch.h").read_text().endswith('"esphome/core/defines.h"\n')
    assert (proj / "esphome_pch.h.gch").is_file()
    assert len((proj / "esphome_pch.h.gch.sum").read_text().strip()) == 64
    # Relative include: an absolute path would poison ccache keys
    assert scons_env.prepended == [
        "-Winvalid-pch",
        "-Wno-error=invalid-pch",
        "-include",
        "esphome_pch.h",
    ]
    # In production projenv["ENV"] aliases os.environ; only the -include
    # flags are genuinely scoped to projenv (src compiles)
    assert scons_env["ENV"]["CCACHE_SLOPPINESS"] == "pch_defines,time_macros"
    assert scons_env["ENV"]["CCACHE_PCH_EXTSUM"] == "true"
    assert scons_env.global_env.prepended == []


def test_pch_script_preserves_spaced_flag_elements(tmp_path: Path) -> None:
    """One SCons element stays one compiler argv; -include pairs are
    stripped from the .gch compile."""
    spaced = tmp_path / "My Configs"
    spaced.mkdir()
    (tmp_path / "dev" / "src").mkdir(parents=True, exist_ok=True)
    (tmp_path / "dev" / "src" / "other.h").write_text("")
    flags = ['-DUSB_PRODUCT=\\"Pico 2W\\"', "-I", str(spaced), "-include", "other.h"]
    _run_script(tmp_path, flags=flags)
    calls = (tmp_path / "fake-gxx.argv").read_text().split("---call---\n")
    gch_call = next(c for c in calls if "c++-header" in c).splitlines()
    assert '-DUSB_PRODUCT="Pico 2W"' in gch_call
    assert str(spaced) in gch_call
    assert "-include" not in gch_call
    # The stripped src-resolvable -include is folded into the prefix header
    pch = (tmp_path / "dev" / "esphome_pch.h").read_text()
    assert pch.splitlines()[0] == '#include "other.h"'


def test_pch_script_folds_joined_force_include_spelling(tmp_path: Path) -> None:
    """-includefoo.h folds like the separated form, matching the native path."""
    (tmp_path / "dev" / "src").mkdir(parents=True, exist_ok=True)
    (tmp_path / "dev" / "src" / "other.h").write_text("")
    _run_script(tmp_path, flags=["-DX=1", "-includeother.h"])
    pch = (tmp_path / "dev" / "esphome_pch.h").read_text()
    assert pch.splitlines()[0] == '#include "other.h"'


def test_pch_script_leaves_absolute_force_includes_unfolded(
    tmp_path: Path,
) -> None:
    """An absolute -include resolves through src_dir / name; it must still
    stay consumer-only or the host path enters the .sum."""
    outside = tmp_path / "outside.h"
    outside.write_text("")
    _run_script(tmp_path, flags=["-DX=1", "-include", str(outside)])
    pch = (tmp_path / "dev" / "esphome_pch.h").read_text()
    assert "outside.h" not in pch


def test_pch_script_leaves_non_src_force_includes_unfolded(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """A user -include outside src/ must not enter the prefix header:
    consumers keep their own copy, so folding an unguarded header would
    include it twice."""
    _run_script(tmp_path, flags=["-DX=1", "-include", "user_extra.h"])
    pch = (tmp_path / "dev" / "esphome_pch.h").read_text()
    assert "user_extra.h" not in pch
    assert pch.splitlines()[-1] == '#include "esphome/core/defines.h"'
    assert "not precompiling non-src force-includes" in capsys.readouterr().out


def test_pch_script_sum_is_device_independent(tmp_path: Path) -> None:
    """Regression: identical configs in different dirs share cache keys."""
    sums = []
    for name in ("dev_a", "dev_b"):
        proj = tmp_path / name
        _run_script(
            tmp_path,
            flags=["-DX=1", "-I", str(proj / "include")],
            env_vars={"CCACHE_BASEDIR": str(proj)},
            name=name,
        )
        sums.append((proj / "esphome_pch.h.gch.sum").read_text())
        (tmp_path / "fake-gxx").unlink()
        (tmp_path / "fake-gxx.argv").unlink(missing_ok=True)
    assert sums[0] == sums[1]


def test_pch_script_transient_compiler_failure_does_not_latch(
    tmp_path: Path,
) -> None:
    """ENOSPC-style failures clear on their own; no .failed marker."""
    scons_env = _run_script(tmp_path, fail=True, fail_msg="No space left on device")
    proj = tmp_path / "dev"
    assert not (proj / "esphome_pch.h.gch.failed").exists()
    assert scons_env.prepended == []


def test_pch_script_failure_marker_suppresses_retry(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    scons_env = _run_script(tmp_path, fail=True)
    proj = tmp_path / "dev"
    assert (proj / "esphome_pch.h.gch.failed").is_file()
    assert not (proj / "esphome_pch.h.gch.sum").exists()
    assert scons_env.prepended == []
    # Second run: same checksum, no compile attempt, but says so
    attempts = (tmp_path / "fake-gxx.argv").read_text().count("c++-header")
    _run_script(tmp_path, fail=True)
    out = capsys.readouterr().out
    assert (tmp_path / "fake-gxx.argv").read_text().count("c++-header") == attempts
    assert "delete esphome_pch.h.gch.failed to retry" in out


def test_pch_script_probe_rejection_falls_back(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """A toolchain that cannot load its own .gch (GCC 10 on macOS arm64)
    must not leave consumers paying for a pch every compile rejects."""
    scons_env = _run_script(tmp_path, reject_pch=True)
    proj = tmp_path / "dev"
    assert not (proj / "esphome_pch.h.gch").exists()
    assert not (proj / "esphome_pch.h.gch.sum").exists()
    assert (proj / "esphome_pch.h.gch.failed").is_file()
    assert scons_env.prepended == []
    assert "toolchain cannot load the pch" in capsys.readouterr().out


def test_pch_script_spawn_failure_is_transient(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """A spawn failure must not latch a .failed marker (matches espidf)."""
    scons_env = _run_script(tmp_path, missing_cxx=True)
    proj = tmp_path / "dev"
    assert not (proj / "esphome_pch.h.gch.failed").exists()
    assert not (proj / "esphome_pch.h.gch.sum").exists()
    assert scons_env.prepended == []
    assert "did not run" in capsys.readouterr().out


def test_pch_script_probe_environment_failure_does_not_latch(
    tmp_path: Path,
) -> None:
    """Probe AND baseline failing is environmental: no marker, retry."""
    scons_env = _run_script(tmp_path, probe_exit=1)
    proj = tmp_path / "dev"
    assert not (proj / "esphome_pch.h.gch").exists()
    assert not (proj / "esphome_pch.h.gch.failed").exists()
    assert scons_env.prepended == []


def test_pch_script_unresolved_package_version_skips_pch(tmp_path: Path) -> None:
    """A KeyError for an installed package is unresolved identity, not absence."""
    scons_env = _run_script(tmp_path, platform_cls=_UnresolvedPlatform)
    assert not (tmp_path / "dev" / "esphome_pch.h.gch").exists()
    assert scons_env.prepended == []


def test_pch_script_package_version_error_skips_pch(tmp_path: Path) -> None:
    """Without trustworthy package identity a stale .gch could survive an
    upgrade, so the script must not build one at all."""
    scons_env = _run_script(tmp_path, platform_cls=_BrokenPlatform)
    proj = tmp_path / "dev"
    assert not (proj / "esphome_pch.h.gch").exists()
    assert scons_env.prepended == []


def test_pch_script_corrupt_sidecar_reads_as_stale(tmp_path: Path) -> None:
    """A truncated/corrupt .failed marker must not disable the pch forever."""
    _run_script(tmp_path, fail=True)
    proj = tmp_path / "dev"
    (proj / "esphome_pch.h.gch.failed").write_bytes(b"\xff\xfe corrupt")
    _run_script(tmp_path)
    assert (proj / "esphome_pch.h.gch").is_file()
    assert (proj / "esphome_pch.h.gch.sum").is_file()


def test_pch_script_rebuilds_when_header_missing(tmp_path: Path) -> None:
    _run_script(tmp_path)
    proj = tmp_path / "dev"
    (proj / "esphome_pch.h").unlink()
    _run_script(tmp_path)
    assert (proj / "esphome_pch.h").is_file()


def test_copy_pch_script(tmp_path: Path) -> None:
    from esphome.core import CORE

    CORE.build_path = tmp_path
    toolchain.copy_pch_script()
    assert (tmp_path / "pch.py").read_text() == _SCRIPT.read_text()


def test_pch_script_unions_user_sloppiness(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """A user CCACHE_SLOPPINESS without the pch tokens gets them unioned on,
    mirroring ccache_pch_env, or every src TU is a permanent miss."""
    scons_env = _run_script(tmp_path, env_vars={"CCACHE_SLOPPINESS": "locale"})
    assert scons_env["ENV"]["CCACHE_SLOPPINESS"] == "locale,pch_defines,time_macros"
    assert "adding pch_defines,time_macros" in capsys.readouterr().out


def test_pch_script_unmodelable_flag_skips_pch(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """Unbalanced quotes and multi-token elements cannot be reproduced as
    one argv; the pch is skipped rather than built with diverging flags."""
    for bad in ("-DFOO='bar", "-DA=1\t-DB=2"):
        scons_env = _run_script(tmp_path, flags=["-DX=1", bad])
        assert scons_env.prepended == []
        assert not (tmp_path / "dev" / "esphome_pch.h.gch").exists()
    assert "unmodelable flag" in capsys.readouterr().out


@pytest.mark.skipif(
    getattr(os, "geteuid", lambda: -1)() == 0, reason="root ignores file modes"
)
def test_pch_script_unlistable_include_dir_skips_pch(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """An unlistable subtree must not silently drop out of the digest."""
    proj = tmp_path / "dev"
    override = proj / "lwip_override"
    hidden = override / "hidden"
    hidden.mkdir(parents=True)
    (hidden / "gen.h").write_text("")
    hidden.chmod(0)
    try:
        scons_env = _run_script(tmp_path, flags=["-DX=1", "-I", str(override)])
    finally:
        hidden.chmod(0o755)
    assert scons_env.prepended == []
    assert "skipping precompiled header" in capsys.readouterr().out


def test_pch_script_nobuild_without_projenv_is_noop(tmp_path: Path) -> None:
    """-t nobuild never exports projenv; the script must not abort."""
    proj = tmp_path / "dev"
    (proj / "src").mkdir(parents=True)

    def strict_import(*names: str) -> None:
        if "projenv" in names:
            raise RuntimeError("Import of non-existent variable 'projenv'")

    env = _FakeSConsEnv(proj, proj / "src", "g++", ["-DX=1"])
    exec(  # noqa: S102
        compile(_SCRIPT.read_text(), "pch.py", "exec"),
        {"Import": strict_import, "env": env},
    )
    assert not (proj / "esphome_pch.h").exists()


def test_pch_script_ignores_library_trees_and_non_headers(tmp_path: Path) -> None:
    """.piolibdeps and non-header files must not enter the digest (or be
    read at all); package versions already cover library identity."""
    proj = tmp_path / "dev"
    libdeps = proj / ".piolibdeps" / "lib" / "src"
    libdeps.mkdir(parents=True)
    (libdeps / "lib.h").write_text("#define A 1\n")
    override = proj / "lwip_override"
    override.mkdir(parents=True)
    (override / "lwipopts.h").write_text("#define TCP_MSS 1460\n")
    (override / "notes.txt").write_text("v1\n")
    flags = ["-DX=1", "-I", str(libdeps), "-I", str(override)]
    _run_script(tmp_path, flags=flags)
    first = (proj / "esphome_pch.h.gch.sum").read_text()
    (libdeps / "lib.h").write_text("#define A 2\n")
    (override / "notes.txt").write_text("v2\n")
    (tmp_path / "fake-gxx.argv").unlink(missing_ok=True)
    _run_script(tmp_path, flags=flags)
    assert (proj / "esphome_pch.h.gch.sum").read_text() == first


def test_pch_script_hashes_project_local_include_dirs(tmp_path: Path) -> None:
    """Generated headers in project-local -I dirs (e.g. rp2's lwip_override)
    must invalidate the checksum when they change."""
    proj = tmp_path / "dev"
    override = proj / "lwip_override"
    override.mkdir(parents=True)
    (override / "lwipopts.h").write_text("#define TCP_MSS 1460\n")
    flags = ["-DX=1", "-I", str(override)]
    _run_script(tmp_path, flags=flags)
    first = (proj / "esphome_pch.h.gch.sum").read_text()
    (override / "lwipopts.h").write_text("#define TCP_MSS 536\n")
    (tmp_path / "fake-gxx.argv").unlink(missing_ok=True)
    _run_script(tmp_path, flags=flags)
    assert (proj / "esphome_pch.h.gch.sum").read_text() != first


@pytest.mark.skipif(
    getattr(os, "geteuid", lambda: -1)() == 0, reason="root ignores file modes"
)
def test_pch_script_unreadable_local_header_skips_pch(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """An unreadable generated header means unknown identity: no pch."""
    proj = tmp_path / "dev"
    override = proj / "lwip_override"
    override.mkdir(parents=True)
    secret = override / "lwipopts.h"
    secret.write_text("#define TCP_MSS 1460\n")
    secret.chmod(0)
    scons_env = _run_script(tmp_path, flags=["-DX=1", "-I", str(override)])
    assert not (proj / "esphome_pch.h.gch.sum").exists()
    assert scons_env.prepended == []
    assert "skipping precompiled header" in capsys.readouterr().out


def test_pch_script_strict_reprobes_cached_gch(tmp_path: Path) -> None:
    """Rejection is per-process: strict re-proves a cached .gch loads."""
    _run_script(tmp_path)
    proj = tmp_path / "dev"
    assert (proj / "esphome_pch.h.gch").is_file()
    (tmp_path / "fake-gxx.argv").unlink(missing_ok=True)
    # Second run: cache fresh, but the toolchain now rejects loads
    with pytest.raises(RuntimeError, match="not used"):
        _run_script(tmp_path, reject_pch=True, env_vars={"ESPHOME_PCH_STRICT": "1"})
    assert not (proj / "esphome_pch.h.gch").exists()


def test_pch_script_strict_raises_when_pch_not_used(tmp_path: Path) -> None:
    """ESPHOME_PCH_STRICT fails the build instead of degrading."""
    with pytest.raises(RuntimeError, match="ESPHOME_PCH_STRICT"):
        _run_script(tmp_path, fail=True, env_vars={"ESPHOME_PCH_STRICT": "1"})


def test_pch_script_strict_fails_without_scons(tmp_path: Path) -> None:
    """No SCons under PlatformIO is an anomaly; strict must not pass."""
    proj = tmp_path / "dev"
    (proj / "src").mkdir(parents=True)

    def strict_import(*names: str) -> None:
        if "projenv" in names:
            raise RuntimeError("Import of non-existent variable 'projenv'")

    env = _FakeSConsEnv(proj, proj / "src", "g++", ["-DX=1"])
    with (
        patch.dict(os.environ, {"ESPHOME_PCH_STRICT": "1"}, clear=True),
        pytest.raises(RuntimeError, match="not used"),
    ):
        exec(  # noqa: S102
            compile(_SCRIPT.read_text(), "pch.py", "exec"),
            {"Import": strict_import, "env": env},
        )


def test_pch_script_strict_reraises_internal_errors(tmp_path: Path) -> None:
    """The catch-all must not swallow programming errors in strict mode."""
    with pytest.raises(TypeError):
        _run_script(tmp_path, env_vars={"ESPHOME_PCH_STRICT": "1"}, platform_cls=None)


@pytest.mark.parametrize(("targets", "passes"), [(["nobuild"], True), ([], False)])
def test_pch_script_strict_projenv_skip_gated_on_nobuild(
    tmp_path: Path, targets: list[str], passes: bool
) -> None:
    """-t nobuild compiles nothing, so the skip passes strict; a missing
    projenv on a real compile must not."""
    import sys
    import types

    proj = tmp_path / "dev"
    (proj / "src").mkdir(parents=True)

    def strict_import(*names: str) -> None:
        if "projenv" in names:
            raise RuntimeError("Import of non-existent variable 'projenv'")

    scons = types.ModuleType("SCons")
    scons_script = types.ModuleType("SCons.Script")
    scons_script.COMMAND_LINE_TARGETS = targets
    env = _FakeSConsEnv(proj, proj / "src", "g++", ["-DX=1"])
    with (
        patch.dict(sys.modules, {"SCons": scons, "SCons.Script": scons_script}),
        patch.dict(os.environ, {"ESPHOME_PCH_STRICT": "1"}, clear=True),
    ):
        run = lambda: exec(  # noqa: S102, E731
            compile(_SCRIPT.read_text(), "pch.py", "exec"),
            {"Import": strict_import, "env": env},
        )
        if passes:
            run()
        else:
            with pytest.raises(RuntimeError, match="not used"):
                run()
    assert not (proj / "esphome_pch.h").exists()


def test_pch_script_strict_passes_on_success(tmp_path: Path) -> None:
    scons_env = _run_script(tmp_path, env_vars={"ESPHOME_PCH_STRICT": "1"})
    # Strict escalates the consumer edges too
    assert "-Werror=invalid-pch" in scons_env.prepended
    assert "-Wno-error=invalid-pch" not in scons_env.prepended
