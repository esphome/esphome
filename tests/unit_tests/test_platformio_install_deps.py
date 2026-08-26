"""Tests for script/platformio_install_deps.py."""

from argparse import Namespace
import importlib.util
import inspect
from pathlib import Path

_SCRIPT = Path(__file__).parents[2] / "script" / "platformio_install_deps.py"


def _load_script():
    spec = importlib.util.spec_from_file_location("platformio_install_deps", _SCRIPT)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def test_spec_key_collapses_destinations() -> None:
    """Two specs delivering one package share a directory and one key."""
    mod = _load_script()
    assert mod.spec_key("esphome/noise-c @ 0.1.21") == "noise-c"
    assert mod.spec_key("esphome/noise-c@0.1.21") == "noise-c"
    assert mod.spec_key("ESP32Async/AsyncTCP @ ^3.4.10") == mod.spec_key(
        "esp32async/asynctcp @ 3.5.0"
    )
    url = "https://github.com/pioarduino/platform-espressif32/releases/download/{v}/platform-espressif32.zip"
    assert mod.spec_key(url.format(v="55.03.311")) == mod.spec_key(
        url.format(v="54.03.20")
    )


def test_parse_specs_and_cli_args(tmp_path: Path) -> None:
    """Parsing skips unpinned and interpolated entries; the CLI rebuild
    keeps the original flag pairing."""
    ini = tmp_path / "platformio.ini"
    ini.write_text(
        "[env:a]\n"
        "platform = fake/platform@1\n"
        "lib_deps =\n"
        "    esphome/noise-c @ 0.1.21\n"
        "    ${common.lib_deps}\n"
        "    internal_lib\n"
        "[env:b]\n"
        "lib_deps =\n"
        "    esphome/noise-c @ 0.1.21\n"
    )
    mod = _load_script()
    args = Namespace(libraries=True, platforms=True, tools=False)
    libs, platforms, tools = mod.parse_specs(str(ini), args)
    # exact-string duplicates collapse; distinct version pins survive
    assert libs == ["esphome/noise-c @ 0.1.21"]
    assert platforms == ["fake/platform@1"]
    assert tools == []
    assert mod.build_cli_args(libs, platforms, tools) == [
        "-l",
        "esphome/noise-c @ 0.1.21",
        "-p",
        "fake/platform@1",
    ]


def test_platformio_surface_for_install_deps_script() -> None:
    """A PlatformIO bump that changes these members must fail here, not
    silently turn the docker image's parallel preinstall into a no-op."""
    from platformio.package.manager._install import PackageManagerInstallMixin
    from platformio.package.manager.base import BasePackageManager
    from platformio.package.manager.library import LibraryPackageManager
    from platformio.package.manager.tool import ToolPackageManager
    from platformio.package.meta import PackageSpec

    params = inspect.signature(PackageManagerInstallMixin._install).parameters
    assert "spec" in params
    assert "skip_dependencies" in params
    for cls in (ToolPackageManager, LibraryPackageManager):
        assert "package_dir" in inspect.signature(cls.__init__).parameters
    for name in ("lock", "unlock", "get_package"):
        assert callable(getattr(BasePackageManager, name))
    assert PackageSpec("owner/name @ ^1.0").name == "name"
