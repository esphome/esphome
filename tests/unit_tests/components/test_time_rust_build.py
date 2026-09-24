"""Tests for isolated, on-demand Rust toolchain provisioning."""

import io
from pathlib import Path
import tarfile
from unittest.mock import Mock

import pytest

from esphome.components.time.statime import build
from esphome.core import EsphomeError

HOST = "x86_64-unknown-linux-gnu"


@pytest.mark.parametrize(
    ("system", "machine", "libc", "expected"),
    [
        ("Linux", "x86_64", "glibc", HOST),
        ("Linux", "aarch64", "glibc", "aarch64-unknown-linux-gnu"),
        ("Darwin", "arm64", "", "aarch64-apple-darwin"),
        ("Darwin", "x86_64", "", "x86_64-apple-darwin"),
        ("Windows", "AMD64", "", "x86_64-pc-windows-msvc"),
        ("Linux", "armv7l", "glibc", None),
        ("Linux", "x86_64", "musl", None),
    ],
)
def test_host_selection(
    monkeypatch: pytest.MonkeyPatch,
    system: str,
    machine: str,
    libc: str,
    expected: str | None,
) -> None:
    """Select official host artifacts and reject unsupported environments early."""
    monkeypatch.setattr(build.platform, "system", lambda: system)
    monkeypatch.setattr(build.platform, "machine", lambda: machine)
    monkeypatch.setattr(build.platform, "libc_ver", lambda: (libc, ""))
    if expected is None:
        with pytest.raises(EsphomeError, match="does not support"):
            build.host_triple()
    else:
        assert build.host_triple() == expected


def test_environment_is_private(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Never reuse or change the caller's personal Rust homes or cross compiler."""
    monkeypatch.setenv("CARGO_HOME", "/personal/cargo")
    monkeypatch.setenv("RUSTUP_HOME", "/personal/rustup")
    monkeypatch.setenv("RUSTUP_TOOLCHAIN", "nightly")
    monkeypatch.setenv("RUSTC", "/personal/rustc")
    monkeypatch.setenv("CC", "riscv32-esp-elf-gcc")
    env = build.build_environment(tmp_path)
    assert env["CARGO_HOME"] == str(tmp_path / "cargo")
    assert env["RUSTUP_HOME"] == str(tmp_path / "rustup")
    assert "RUSTUP_TOOLCHAIN" not in env
    assert "RUSTC" not in env
    assert "CC" not in env
    assert build.os.environ["CARGO_HOME"] == "/personal/cargo"
    assert build.os.environ["RUSTUP_HOME"] == "/personal/rustup"


def make_toolchain(directory: Path, target: str) -> Path:
    """Create only the files required for a completed installation."""
    (directory / "bin").mkdir(parents=True, exist_ok=True)
    (directory / "bin" / "rustc").touch()
    (directory / "bin" / "cargo").touch()
    library = directory / "lib" / "rustlib" / target / "lib"
    library.mkdir(parents=True, exist_ok=True)
    (library / "libcore-test.rlib").touch()
    return directory


def test_cache_and_incomplete_install(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Reuse a completed cache offline, and repair missing target libraries."""
    monkeypatch.setattr(build, "host_triple", lambda: HOST)
    monkeypatch.setattr(build, "tools_cache_path", lambda *args: tmp_path)
    toolchain = tmp_path / "rust" / "compiler"
    install = Mock(
        side_effect=lambda *args: make_toolchain(toolchain, build.RISC_V_TARGET)
    )
    monkeypatch.setattr(build, "install_riscv", install)
    run = Mock()
    monkeypatch.setattr(build, "run", run)
    first, env = build.prepare_toolchain(build.RISC_V_TARGET)
    assert first == toolchain / "bin" / "cargo"
    assert env["RUSTC"] == str(toolchain / "bin" / "rustc")
    assert install.call_count == 1
    assert run.call_count == 2
    second, _ = build.prepare_toolchain(build.RISC_V_TARGET)
    assert first == second
    assert install.call_count == 1
    assert (
        run.call_count == 2
    )  # No compiler invocation or network probe on a warm cache.
    (
        toolchain
        / "lib"
        / "rustlib"
        / build.RISC_V_TARGET
        / "lib"
        / "libcore-test.rlib"
    ).unlink()
    build.prepare_toolchain(build.RISC_V_TARGET)
    assert install.call_count == 2


def test_failed_install_has_no_stamp(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """A failed compiler download must be retried on the next build."""
    monkeypatch.setattr(build, "host_triple", lambda: HOST)
    monkeypatch.setattr(build, "tools_cache_path", lambda *args: tmp_path)
    install = Mock(side_effect=EsphomeError("download failed"))
    monkeypatch.setattr(build, "install_riscv", install)
    for _ in range(2):
        with pytest.raises(EsphomeError, match="download failed"):
            build.prepare_toolchain(build.RISC_V_TARGET)
    assert install.call_count == 2
    assert not list((tmp_path / "rust").glob("*.ready"))


def test_bootstrap_is_pinned(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    """Verify the bootstrap checksum and keep installation out of shell profiles."""
    download = Mock(side_effect=lambda url, path, **kwargs: path.touch())
    monkeypatch.setattr(build, "download_with_resume", download)
    run = Mock()
    monkeypatch.setattr(build, "run", run)
    env = build.build_environment(tmp_path)
    result = build.install_riscv(tmp_path, HOST, env)
    assert result == tmp_path / "rustup" / "toolchains" / f"{build.RUST_VERSION}-{HOST}"
    assert download.call_args.kwargs["sha256"] == build.RUSTUP_SHA256[HOST]
    assert f"/archive/{build.RUSTUP_VERSION}/" in download.call_args.args[0]
    bootstrap = run.call_args_list[0].args[0]
    assert "--no-modify-path" in bootstrap
    assert bootstrap[bootstrap.index("--default-toolchain") + 1] == "none"
    install = run.call_args_list[1].args[0]
    assert install[install.index("--profile") + 1] == "minimal"
    assert install[install.index("--target") + 1] == build.RISC_V_TARGET
    assert "--no-self-update" in install
    assert not (tmp_path / "rustup-init").exists()


def test_checksum_failure_does_not_execute(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Never execute an installer whose download failed verification."""
    monkeypatch.setattr(
        build, "download_with_resume", Mock(side_effect=EsphomeError("checksum"))
    )
    run = Mock()
    monkeypatch.setattr(build, "run", run)
    with pytest.raises(EsphomeError, match="checksum"):
        build.install_riscv(tmp_path, HOST, build.build_environment(tmp_path))
    run.assert_not_called()


def test_xtensa_installer_layout(tmp_path: Path) -> None:
    """Import official installer components without copying docs or running scripts."""
    archive = tmp_path / "rust.tar.xz"
    entries = {
        "components": b"rustc\ncargo\nrust-docs\n",
        "rustc/bin/rustc": b"compiler",
        "cargo/bin/cargo": b"cargo",
        "rustc/manifest.in": b"installer metadata",
        "rust-docs/share/doc/index.html": b"unused documentation",
    }
    with tarfile.open(archive, "w:xz") as output:
        for name, data in entries.items():
            info = tarfile.TarInfo(f"rust-nightly-host/{name}")
            info.size = len(data)
            info.mode = 0o755
            output.addfile(info, io.BytesIO(data))
    destination = tmp_path / "installed"
    build.unpack_components(archive, destination, ("rustc", "cargo"))
    assert (destination / "bin" / "rustc").read_bytes() == b"compiler"
    assert not (destination / "manifest.in").exists()
    assert not (destination / "share").exists()
    with pytest.raises(EsphomeError, match="Missing"):
        build.unpack_components(archive, destination, ("rust-src",))


def test_unsupported_xtensa_host_does_not_download(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Do not attempt to bootstrap an unavailable Xtensa compiler on Intel macOS."""
    monkeypatch.setattr(build, "host_triple", lambda: "x86_64-apple-darwin")
    cache = Mock()
    monkeypatch.setattr(build, "tools_cache_path", cache)
    with pytest.raises(EsphomeError, match="has no release"):
        build.prepare_toolchain(build.XTENSA_TARGETS[0])
    cache.assert_not_called()
