import logging
from typing import Callable, Optional, Any, ContextManager
from types import ModuleType
import importlib
import importlib.util
import importlib.resources
import importlib.abc
import sys
from pathlib import Path
from dataclasses import dataclass

from esphome.const import SOURCE_FILE_EXTENSIONS
import esphome.core.config
from esphome.core import CORE
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)


@dataclass(frozen=True, order=True)
class FileResource:
    package: str
    resource: str

    def path(self) -> ContextManager[Path]:
        return importlib.resources.as_file(
            importlib.resources.files(self.package) / self.resource
        )


class ComponentManifest:
    def __init__(self, module: ModuleType):
        self.module = module

    @property
    def package(self) -> str:
        """Return the package name the module is contained in.

        Examples:
        - esphome/components/gpio/__init__.py -> esphome.components.gpio
        - esphome/components/gpio/switch/__init__.py -> esphome.components.gpio.switch
        - esphome/components/a4988/stepper.py -> esphome.components.a4988
        """
        return self.module.__package__

    @property
    def is_platform(self) -> bool:
        return len(self.module.__name__.split(".")) == 4

    @property
    def is_platform_component(self) -> bool:
        return getattr(self.module, "IS_PLATFORM_COMPONENT", False)

    @property
    def config_schema(self) -> Optional[Any]:
        return getattr(self.module, "CONFIG_SCHEMA", None)

    @property
    def multi_conf(self) -> bool:
        return getattr(self.module, "MULTI_CONF", False)

    @property
    def multi_conf_no_default(self) -> bool:
        return getattr(self.module, "MULTI_CONF_NO_DEFAULT", False)

    @property
    def to_code(self) -> Optional[Callable[[Any], None]]:
        return getattr(self.module, "to_code", None)

    @property
    def dependencies(self) -> list[str]:
        return getattr(self.module, "DEPENDENCIES", [])

    @property
    def conflicts_with(self) -> list[str]:
        return getattr(self.module, "CONFLICTS_WITH", [])

    @property
    def auto_load(self) -> list[str]:
        al = getattr(self.module, "AUTO_LOAD", [])
        if callable(al):
            return al()
        return al

    @property
    def codeowners(self) -> list[str]:
        return getattr(self.module, "CODEOWNERS", [])

    @property
    def final_validate_schema(self) -> Optional[Callable[[ConfigType], None]]:
        """Components can declare a `FINAL_VALIDATE_SCHEMA` cv.Schema that gets called
        after the main validation. In that function checks across components can be made.

        Note that the function can't mutate the configuration - no changes are saved
        """
        return getattr(self.module, "FINAL_VALIDATE_SCHEMA", None)

    @property
    def resources(self) -> list[FileResource]:
        """Return a list of all file resources defined in the package of this component.

        This will return all cpp source files that are located in the same folder as the
        loaded .py file (does not look through subdirectories)
        """
        ret = []

        for resource in (
            r.name
            for r in importlib.resources.files(self.package).iterdir()
            if r.is_file()
        ):
            if Path(resource).suffix not in SOURCE_FILE_EXTENSIONS:
                continue
            if not importlib.resources.files(self.package).joinpath(resource).is_file():
                # Not a resource = this is a directory (yeah this is confusing)
                continue
            ret.append(FileResource(self.package, resource))
        return ret


class ComponentMetaFinder(importlib.abc.MetaPathFinder):
    def __init__(
        self, components_path: Path, allowed_components: Optional[list[str]] = None
    ) -> None:
        self._allowed_components = allowed_components
        self._finders = []
        for hook in sys.path_hooks:
            try:
                finder = hook(str(components_path))
            except ImportError:
                continue
            self._finders.append(finder)

    def find_spec(self, fullname: str, path: Optional[list[str]], target=None):
        if not fullname.startswith("esphome.components."):
            return None
        parts = fullname.split(".")
        if len(parts) != 3:
            # only handle direct components, not platforms
            # platforms are handled automatically when parent is imported
            return None
        component = parts[2]
        if (
            self._allowed_components is not None
            and component not in self._allowed_components
        ):
            return None

        for finder in self._finders:
            spec = finder.find_spec(fullname, target=target)
            if spec is not None:
                return spec
        return None


def clear_component_meta_finders():
    sys.meta_path = [x for x in sys.meta_path if not isinstance(x, ComponentMetaFinder)]


def install_meta_finder(
    components_path: Path, allowed_components: Optional[list[str]] = None
):
    sys.meta_path.insert(0, ComponentMetaFinder(components_path, allowed_components))


def install_custom_components_meta_finder():
    custom_components_dir = (Path(CORE.config_dir) / "custom_components").resolve()
    install_meta_finder(custom_components_dir)


def _lookup_module(domain):
    if domain in _COMPONENT_CACHE:
        return _COMPONENT_CACHE[domain]

    try:
        module = importlib.import_module(f"esphome.components.{domain}")
    except ImportError as e:
        if "No module named" not in str(e):
            _LOGGER.error("Unable to import component %s:", domain, exc_info=True)
        return None
    except Exception:  # pylint: disable=broad-except
        _LOGGER.error("Unable to load component %s:", domain, exc_info=True)
        return None

    manif = ComponentManifest(module)
    _COMPONENT_CACHE[domain] = manif
    return manif


def get_component(domain):
    assert "." not in domain
    return _lookup_module(domain)


def get_platform(domain, platform):
    full = f"{platform}.{domain}"
    return _lookup_module(full)


_COMPONENT_CACHE = {}
CORE_COMPONENTS_PATH = (Path(__file__).parent / "components").resolve()
_COMPONENT_CACHE["esphome"] = ComponentManifest(esphome.core.config)
