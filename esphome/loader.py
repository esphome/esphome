import logging
import typing
from typing import Callable, List, Optional, Dict, Any, ContextManager
from types import ModuleType
import importlib
import importlib.util
import importlib.resources
import importlib.abc
import sys
from pathlib import Path

from esphome.const import ESP_PLATFORMS, SOURCE_FILE_EXTENSIONS
import esphome.core.config
from esphome.core import CORE

_LOGGER = logging.getLogger(__name__)


class SourceFile:
    def __init__(
        self,
        package: importlib.resources.Package,
        resource: importlib.resources.Resource,
    ) -> None:
        self._package = package
        self._resource = resource

    def open_binary(self) -> typing.BinaryIO:
        return importlib.resources.open_binary(self._package, self._resource)

    def path(self) -> ContextManager[Path]:
        return importlib.resources.path(self._package, self._resource)


class ComponentManifest:
    def __init__(self, module: ModuleType):
        self.module = module

    @property
    def package(self) -> str:
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
    def to_code(self) -> Optional[Callable[[Any], None]]:
        return getattr(self.module, "to_code", None)

    @property
    def esp_platforms(self) -> List[str]:
        return getattr(self.module, "ESP_PLATFORMS", ESP_PLATFORMS)

    @property
    def dependencies(self) -> List[str]:
        return getattr(self.module, "DEPENDENCIES", [])

    @property
    def conflicts_with(self) -> List[str]:
        return getattr(self.module, "CONFLICTS_WITH", [])

    @property
    def auto_load(self) -> List[str]:
        return getattr(self.module, "AUTO_LOAD", [])

    @property
    def codeowners(self) -> List[str]:
        return getattr(self.module, "CODEOWNERS", [])

    @property
    def source_files(self) -> Dict[Path, SourceFile]:
        ret = {}
        for resource in importlib.resources.contents(self.package):
            if Path(resource).suffix not in SOURCE_FILE_EXTENSIONS:
                continue
            if not importlib.resources.is_resource(self.package, resource):
                # Not a resource = this is a directory (yeah this is confusing)
                continue
            # Always use / for C++ include names
            target_path = Path(*self.package.split(".")) / resource
            ret[target_path] = SourceFile(self.package, resource)
        return ret


class ComponentMetaFinder(importlib.abc.MetaPathFinder):
    def __init__(
        self, components_path: Path, allowed_components: Optional[List[str]] = None
    ) -> None:
        self._allowed_components = allowed_components
        self._finders = []
        for hook in sys.path_hooks:
            try:
                finder = hook(str(components_path))
            except ImportError:
                continue
            self._finders.append(finder)

    def find_spec(self, fullname: str, path: Optional[List[str]], target=None):
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
    components_path: Path, allowed_components: Optional[List[str]] = None
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
    else:
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
