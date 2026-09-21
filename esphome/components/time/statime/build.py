"""Build the pinned Rust adapter with an isolated, cached toolchain.

Invoked by CMake with ESPHome's Python, only when the Rust library is built.
Kept with the external component so existing ESPHome builders can use the draft.
"""

import argparse
import logging
import os
from pathlib import Path
import platform
import shutil
import subprocess
import tempfile

from filelock import FileLock

from esphome.build_helpers.tools_cache import IDF_TOOLS_CACHE, tools_cache_path
from esphome.core import EsphomeError
from esphome.framework_helpers import archive_extract_all, download_with_resume

_LOGGER = logging.getLogger(__name__)
RUST_VERSION = "1.98.1"
RUSTUP_VERSION = "1.29.1"
XTENSA_VERSION = "1.97.0.0"
RISC_V_TARGET = "riscv32imac-unknown-none-elf"
XTENSA_TARGETS = ("xtensa-esp32-none-elf", "xtensa-esp32s3-none-elf")
# Official release checksums; never execute an unverified bootstrap download.
RUSTUP_SHA256 = {
    "x86_64-unknown-linux-gnu": "dda7234360b7f578ca8b0ddcb80145646fa61a67c1720a5abc7051b35c9fcb71",
    "aarch64-unknown-linux-gnu": "15f6e4ce9f583b929c996c91562bad6d4454f3281de858b02cdfdef615fac433",
    "x86_64-apple-darwin": "259e2b84274434085163fe8d556510571772cda2aa6d87ca6aa664f57bc644e3",
    "aarch64-apple-darwin": "ec1b9233e7f72990ecd8e62063fa7f6c3dfc2bec8e97f88bff165f9100ac696a",
    "x86_64-pc-windows-msvc": "6f4bef66261261fcb43131be8720bab817d403a09edec7455c371974b90bdb7e",
}
XTENSA_SHA256 = {
    "x86_64-unknown-linux-gnu": "a99bfee69221e9ff6d86388f6811ee688cd405e6a0400a3cd1784e8d463e9d99",
    "aarch64-unknown-linux-gnu": "dc6efa8906849608497c2ffb3d6a9466588835a79b5e8886c229de133640945c",
    "aarch64-apple-darwin": "430fcbf54967e99d16debe48926a1df558ab8b67af3c060a658d25ce752bd790",
    "x86_64-pc-windows-msvc": "43eadbfd09c411d9a93768a2fa92063adc23f10f1b0fd28703e266ab40e98e74",
}
XTENSA_SOURCE_SHA256 = (
    "568d688b9f8f332ec4d04657544fad23e99ce11e9e6cf5835979e68a28c68b73"
)


def host_triple() -> str:
    """Select a supported compiler host before creating or downloading anything."""
    system = platform.system()
    machine = platform.machine().lower()
    arch = {"amd64": "x86_64", "arm64": "aarch64"}.get(machine, machine)
    suffix = {
        "Linux": "unknown-linux-gnu",
        "Darwin": "apple-darwin",
        "Windows": "pc-windows-msvc",
    }.get(system)
    host = f"{arch}-{suffix}"
    if host not in RUSTUP_SHA256 or (
        system == "Linux" and platform.libc_ver()[0] != "glibc"
    ):
        raise EsphomeError(
            f"Automatic Rust installation does not support {system}/{machine}. "
            "Use a supported Linux glibc, macOS or Windows build host."
        )
    return host


def build_environment(root: Path) -> dict[str, str]:
    """Keep toolchains, Cargo configuration and downloaded crates out of user homes."""
    env = os.environ.copy()
    env.update(
        RUSTUP_HOME=str(root / "rustup"),
        CARGO_HOME=str(root / "cargo"),
        RUSTUP_AUTO_INSTALL="0",
        RUSTUP_DIST_SERVER="https://static.rust-lang.org",
        RUSTUP_UPDATE_ROOT="https://static.rust-lang.org/rustup",
        RUSTUP_INIT_SKIP_PATH_CHECK="yes",
    )
    # IDF may supply cross-compilers; Cargo host build scripts need the host compiler.
    for key in ("RUSTC", "RUSTUP_TOOLCHAIN", "CC", "CXX", "AR", "CFLAGS", "CXXFLAGS"):
        env.pop(key, None)
    return env


def run(command: list[str], env: dict[str, str]) -> None:
    """Run a build step without a shell and preserve its diagnostic output."""
    subprocess.run(command, env=env, check=True)


def install_riscv(root: Path, host: str, env: dict[str, str]) -> Path:
    """Install only the pinned minimal toolchain and C6 standard library."""
    executable = ".exe" if "windows" in host else ""
    rustup = root / "cargo" / "bin" / f"rustup{executable}"
    if not rustup.is_file():
        installer = root / f"rustup-init{executable}"
        download_with_resume(
            f"https://static.rust-lang.org/rustup/archive/{RUSTUP_VERSION}/{host}/{installer.name}",
            installer,
            sha256=RUSTUP_SHA256[host],
        )
        installer.chmod(0o755)
        try:
            run(
                [
                    str(installer),
                    "-y",
                    "--no-modify-path",
                    "--profile",
                    "minimal",
                    "--default-toolchain",
                    "none",
                    "--default-host",
                    host,
                ],
                env,
            )
        finally:
            installer.unlink(missing_ok=True)
    run(
        [
            str(rustup),
            "toolchain",
            "install",
            f"{RUST_VERSION}-{host}",
            "--profile",
            "minimal",
            "--target",
            RISC_V_TARGET,
            "--no-self-update",
        ],
        env,
    )
    return root / "rustup" / "toolchains" / f"{RUST_VERSION}-{host}"


def unpack_components(
    archive: Path, destination: Path, components: tuple[str, ...]
) -> None:
    """Extract selected official Rust installer components, without running scripts."""
    with tempfile.TemporaryDirectory(dir=archive.parent) as directory:
        extracted = Path(directory)
        archive_extract_all(archive, extracted)
        # ESPHome's extractor removes the archive's common top-level directory.
        package = extracted
        if not (package / "components").is_file():
            raise EsphomeError(f"Unexpected Rust archive layout: {archive.name}")
        available = (package / "components").read_text().splitlines()
        for component in components:
            if component not in available:
                raise EsphomeError(f"Missing {component} in {archive.name}")
            shutil.copytree(
                package / component,
                destination,
                dirs_exist_ok=True,
                ignore=shutil.ignore_patterns("manifest.in"),
            )


def install_xtensa(root: Path, host: str) -> Path:
    """Install the pinned esp-rs compiler and matching sources for build-std."""
    destination = root / "toolchains" / f"esp-{XTENSA_VERSION}-{host}"
    destination.parent.mkdir(parents=True, exist_ok=True)
    extension = "zip" if "windows" in host else "tar.xz"
    base = f"https://github.com/esp-rs/rust-build/releases/download/v{XTENSA_VERSION}"
    packages = (
        (
            f"rust-{XTENSA_VERSION}-{host}.{extension}",
            XTENSA_SHA256[host],
            ("rustc", "cargo", f"rust-std-{host}"),
        ),
        (f"rust-src-{XTENSA_VERSION}.tar.xz", XTENSA_SOURCE_SHA256, ("rust-src",)),
    )
    # Publish only a complete installation; an interrupted extraction is never reused.
    with tempfile.TemporaryDirectory(dir=destination.parent) as directory:
        staging = Path(directory) / "toolchain"
        for filename, checksum, components in packages:
            archive = root / filename
            download_with_resume(f"{base}/{filename}", archive, sha256=checksum)
            try:
                unpack_components(archive, staging, components)
            finally:
                archive.unlink(missing_ok=True)
        if destination.exists():
            shutil.rmtree(destination)
        staging.rename(destination)
    return destination


def installation_complete(toolchain: Path, host: str, target: str) -> bool:
    """Do not trust a completion stamp when required installed files are missing."""
    executable = ".exe" if "windows" in host else ""
    if not all(
        (toolchain / "bin" / f"{name}{executable}").is_file()
        for name in ("rustc", "cargo")
    ):
        return False
    if target == RISC_V_TARGET:
        return any(
            (toolchain / "lib" / "rustlib" / target / "lib").glob("libcore-*.rlib")
        )
    return (
        toolchain
        / "lib"
        / "rustlib"
        / "src"
        / "rust"
        / "library"
        / "core"
        / "Cargo.toml"
    ).is_file()


def prepare_toolchain(target: str) -> tuple[Path, dict[str, str]]:
    """Serialize installation and reuse completed toolchains without network access."""
    host = host_triple()
    if target != RISC_V_TARGET and target not in XTENSA_TARGETS:
        raise EsphomeError(f"Unsupported Rust target: {target}")
    if target in XTENSA_TARGETS and host not in XTENSA_SHA256:
        raise EsphomeError(f"esp-rs {XTENSA_VERSION} has no release for {host}")
    root = tools_cache_path(*IDF_TOOLS_CACHE) / "rust"
    root.mkdir(parents=True, exist_ok=True)
    env = build_environment(root)
    version = RUST_VERSION if target == RISC_V_TARGET else f"esp-{XTENSA_VERSION}"
    stamp = root / f"{version}-{host}.ready"
    with FileLock(str(root / "install.lock"), timeout=900):
        toolchain = Path(stamp.read_text()) if stamp.is_file() else None
        executable = ".exe" if "windows" in host else ""
        if toolchain is None or not installation_complete(toolchain, host, target):
            _LOGGER.info("Installing Rust %s for %s in %s", version, target, root)
            toolchain = (
                install_riscv(root, host, env)
                if target == RISC_V_TARGET
                else install_xtensa(root, host)
            )
            # Verify the actual compiler can run before recording a successful install.
            run([str(toolchain / "bin" / f"rustc{executable}"), "--version"], env)
            run([str(toolchain / "bin" / f"cargo{executable}"), "--version"], env)
            if not installation_complete(toolchain, host, target):
                raise EsphomeError(f"Incomplete Rust toolchain in {toolchain}")
            stamp.write_text(str(toolchain))
        else:
            _LOGGER.info("Using cached Rust %s for %s", version, target)
    env["RUSTC"] = str(toolchain / "bin" / f"rustc{executable}")
    return toolchain / "bin" / f"cargo{executable}", env


def main() -> int:
    """Prepare the selected compiler and build the adapter from locked sources."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--target", required=True, choices=(RISC_V_TARGET, *XTENSA_TARGETS)
    )
    parser.add_argument("--target-dir", required=True, type=Path)
    args = parser.parse_args()
    logging.basicConfig(level=logging.INFO, format="%(levelname)s %(message)s")
    try:
        cargo, env = prepare_toolchain(args.target)
        command = [
            str(cargo),
            "build",
            "--manifest-path",
            str(Path(__file__).with_name("Cargo.toml")),
            "--target",
            args.target,
            "--target-dir",
            str(args.target_dir),
            "--release",
            "--locked",
        ]
        if args.target in XTENSA_TARGETS:
            command.append("-Zbuild-std=core")
        run(command, env)
    except (EsphomeError, OSError, ValueError, subprocess.CalledProcessError) as err:
        _LOGGER.error("Rust adapter build failed: %s", err)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
