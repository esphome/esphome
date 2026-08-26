"""Contract test for the PlatformIO surface script/platformio_install_deps.py drives."""

import inspect


def test_platformio_surface_for_install_deps_script() -> None:
    """A PlatformIO bump that changes these members must fail here, not
    silently turn the docker image's parallel preinstall into a no-op."""
    from platformio.package.manager._install import PackageManagerInstallMixin
    from platformio.package.manager.base import BasePackageManager
    from platformio.package.manager.library import LibraryPackageManager
    from platformio.package.manager.platform import PlatformPackageManager
    from platformio.package.manager.tool import ToolPackageManager
    from platformio.package.meta import PackageSpec

    params = inspect.signature(PackageManagerInstallMixin._install).parameters
    assert "spec" in params
    assert "skip_dependencies" in params
    for cls in (ToolPackageManager, LibraryPackageManager, PlatformPackageManager):
        assert "package_dir" in inspect.signature(cls.__init__).parameters
    for name in ("lock", "unlock", "get_package"):
        assert callable(getattr(BasePackageManager, name))
    assert PackageSpec("owner/name @ ^1.0").name == "name"
