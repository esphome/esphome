from collections.abc import Callable
from contextlib import AbstractContextManager
from dataclasses import dataclass
import importlib
import importlib.abc
import importlib.resources
import importlib.util
import logging
from pathlib import Path
import sys
from types import ModuleType
from typing import TYPE_CHECKING, Any

from esphome.const import SOURCE_FILE_EXTENSIONS
from esphome.types import ConfigType

if TYPE_CHECKING:
    from esphome.cpp_generator import MockObjClass

# `esphome.core.config` is imported lazily in `_lookup_module` when the
# "esphome" pseudo-component is first resolved. It pulls in
# `esphome.automation` and `esphome.config_validation`, which together
# dominate `esphome.__main__` startup cost when loaded eagerly.
# `esphome.cpp_generator` is similarly avoided at module scope; it pulls
# in `esphome.yaml_util` and is only needed for the `MockObjClass` type
# annotation, which is resolved lazily via `TYPE_CHECKING`.

_LOGGER = logging.getLogger(__name__)


@dataclass(frozen=True, order=True)
class FileResource:
    package: str
    resource: str

    def path(self) -> AbstractContextManager[Path]:
        return importlib.resources.as_file(
            importlib.resources.files(self.package) / self.resource
        )


class ComponentManifest:
    def __init__(self, module: ModuleType, recursive_sources: bool = False):
        self.module = module
        self.recursive_sources = recursive_sources

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
    def is_target_platform(self) -> bool:
        return getattr(self.module, "IS_TARGET_PLATFORM", False)

    @property
    def config_schema(self) -> Any | None:
        return getattr(self.module, "CONFIG_SCHEMA", None)

    @property
    def multi_conf(self) -> bool:
        return getattr(self.module, "MULTI_CONF", False)

    @property
    def multi_conf_no_default(self) -> bool:
        return getattr(self.module, "MULTI_CONF_NO_DEFAULT", False)

    @property
    def to_code(self) -> Callable[[Any], None] | None:
        return getattr(self.module, "to_code", None)

    @property
    def dependencies(self) -> list[str]:
        return getattr(self.module, "DEPENDENCIES", [])

    @property
    def conflicts_with(self) -> list[str]:
        return getattr(self.module, "CONFLICTS_WITH", [])

    @property
    def auto_load(
        self,
    ) -> list[str] | Callable[[], list[str]] | Callable[[ConfigType], list[str]]:
        return getattr(self.module, "AUTO_LOAD", [])

    @property
    def codeowners(self) -> list[str]:
        return getattr(self.module, "CODEOWNERS", [])

    @property
    def aliases(self) -> list[str]:
        """Legacy names that should transparently route to this component.

        See the :func:`_build_alias_map` documentation for how aliases are
        discovered (AST scan, no execution) and registered both for the YAML
        loader (top-level key rename in :mod:`esphome.config`) and for
        Python imports (``sys.meta_path`` finder, below).
        """
        return getattr(self.module, "ALIASES", [])

    @property
    def alias_removal_version(self) -> str | None:
        """Optional ESPHome version when the alias warning becomes a hard error.

        Surfaced in the deprecation warning emitted by the YAML pre-pass so
        users know how long they have to migrate. ``None`` means the warning
        does not mention a specific version.
        """
        return getattr(self.module, "ALIAS_REMOVAL_VERSION", None)

    @property
    def instance_type(self) -> "MockObjClass | None":
        return getattr(self.module, "INSTANCE_TYPE", None)

    @property
    def final_validate_schema(self) -> Callable[[ConfigType], None] | None:
        """Components can declare a `FINAL_VALIDATE_SCHEMA` cv.Schema that gets called
        after the main validation. In that function checks across components can be made.

        Note that the function can't mutate the configuration - no changes are saved
        """
        return getattr(self.module, "FINAL_VALIDATE_SCHEMA", None)

    @property
    def resources(self) -> list[FileResource]:
        """Return a list of all file resources defined in the package of this component.

        By default only files directly in the package directory are returned. Manifests
        constructed with ``recursive_sources=True`` also descend into non-subpackage
        subdirectories (subdirectories without an ``__init__.py``), so core code can
        live under ``esphome/core/<group>/`` without every component paying the cost.
        """
        ret: list[FileResource] = []

        # Get filter function for source files
        filter_source_files_func = getattr(self.module, "FILTER_SOURCE_FILES", None)

        # Get list of files to exclude
        excluded_files = (
            set(filter_source_files_func()) if filter_source_files_func else set()
        )

        root = importlib.resources.files(self.package)

        for child in root.iterdir():
            name = child.name
            if child.is_file():
                if Path(name).suffix not in SOURCE_FILE_EXTENSIONS:
                    continue
                if name in excluded_files:
                    continue
                ret.append(FileResource(self.package, name))
            elif self.recursive_sources and child.is_dir() and name != "__pycache__":
                # Skip Python subpackages — they load as their own components.
                if child.joinpath("__init__.py").is_file():
                    continue
                for sub in child.iterdir():
                    if not sub.is_file():
                        continue
                    if Path(sub.name).suffix not in SOURCE_FILE_EXTENSIONS:
                        continue
                    resource = f"{name}/{sub.name}"
                    if resource in excluded_files:
                        continue
                    ret.append(FileResource(self.package, resource))

        return ret


class ComponentMetaFinder(importlib.abc.MetaPathFinder):
    def __init__(
        self, components_path: Path, allowed_components: list[str] | None = None
    ) -> None:
        self._allowed_components = allowed_components
        self._finders = []
        for hook in sys.path_hooks:
            try:
                finder = hook(str(components_path))
            except ImportError:
                continue
            self._finders.append(finder)

    def find_spec(self, fullname: str, path: list[str] | None, target=None):
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
    components_path: Path, allowed_components: list[str] | None = None
):
    sys.meta_path.insert(0, ComponentMetaFinder(components_path, allowed_components))


def _lookup_module(domain: str, exception: bool) -> ComponentManifest | None:
    if domain in _COMPONENT_CACHE:
        return _COMPONENT_CACHE[domain]

    if domain == "esphome":
        import esphome.core.config

        manif = ComponentManifest(esphome.core.config, recursive_sources=True)
        _COMPONENT_CACHE[domain] = manif
        return manif

    # If `domain` is the legacy name of a renamed component, redirect to the
    # canonical module so the rest of the loader (and every caller of
    # `get_component(legacy)`) transparently sees the new component.
    alias_map = _get_alias_map()
    if domain in alias_map:
        canonical = alias_map[domain]
        manif = _lookup_module(canonical, exception)
        if manif is not None:
            _COMPONENT_CACHE[domain] = manif
        return manif

    try:
        module = importlib.import_module(f"esphome.components.{domain}")
    except ImportError as e:
        if exception:
            raise
        if "No module named" in str(e):
            _LOGGER.info(
                "Unable to import component %s: %s", domain, str(e), exc_info=False
            )
        else:
            _LOGGER.exception("Unable to import component %s:", domain)
        return None
    except Exception:  # pylint: disable=broad-except
        if exception:
            raise
        _LOGGER.exception("Unable to load component %s:", domain)
        return None

    manif = ComponentManifest(module)
    _COMPONENT_CACHE[domain] = manif
    return manif


def get_component(domain: str, exception: bool = False) -> ComponentManifest | None:
    assert "." not in domain
    return _lookup_module(domain, exception)


def get_platform(domain: str, platform: str) -> ComponentManifest | None:
    full = f"{platform}.{domain}"
    return _lookup_module(full, False)


_COMPONENT_CACHE: dict[str, ComponentManifest] = {}
CORE_COMPONENTS_PATH = (Path(__file__).parent / "components").resolve()


def _replace_component_manifest(domain: str, manifest: ComponentManifest) -> None:
    """Replace the cached manifest for a component.

    This is an intentionally-supported hook for the C++ test infrastructure
    to install ``ComponentManifestOverride`` wrappers.  Normal application
    code should never call this.
    """
    _COMPONENT_CACHE[domain] = manifest


# ---------------------------------------------------------------------------
# Component aliases (renamed-platform back-compat)
# ---------------------------------------------------------------------------
#
# A component can declare ``ALIASES = ["legacy_name"]`` (and optionally
# ``ALIAS_REMOVAL_VERSION = "YYYY.M.0"``) in its ``__init__.py``. Two
# integrations are then wired up automatically:
#
#   1. **Python imports** — a ``sys.meta_path`` finder (``_AliasFinder``)
#      intercepts ``esphome.components.<legacy>``/``...<legacy>.<sub>``
#      imports and resolves them against the canonical component so external
#      custom components that still import from the old path keep working.
#
#   2. **YAML loader** — ``_lookup_module`` consults the alias map so
#      ``get_component("legacy")`` returns the canonical manifest. The
#      ``esphome.config`` pre-pass uses the same map to rewrite legacy
#      top-level keys in the user's config (with a deprecation warning) so
#      dependency checks, schema validation and codegen all see only the
#      canonical name.
#
# Both lookups are populated by ``_build_alias_map``, which **AST-parses**
# every component's ``__init__.py`` rather than importing it. That keeps the
# cost low: scanning ~400 components on disk takes ~5 ms instead of the
# multi-second cost of executing every component's import side-effects.


_ALIAS_MAP_CACHE: dict[str, str] | None = None
_ALIAS_META_CACHE: dict[str, "AliasMeta"] | None = None


@dataclass(frozen=True)
class AliasMeta:
    """Metadata for a single deprecated alias entry.

    Used by the YAML pre-pass in :mod:`esphome.config` to produce a
    deprecation warning citing the canonical name and (optionally) the
    removal version declared by the canonical component.
    """

    canonical: str
    removal_version: str | None


def _ensure_alias_caches() -> None:
    """Populate both alias caches from a single directory scan.

    ``_build_alias_map`` returns both maps together, so building them in one
    shot avoids scanning every component's ``__init__.py`` twice when a run
    needs both the canonical map (loader) and the metadata map (config
    pre-pass).
    """
    global _ALIAS_MAP_CACHE, _ALIAS_META_CACHE
    if _ALIAS_MAP_CACHE is None or _ALIAS_META_CACHE is None:
        _ALIAS_MAP_CACHE, _ALIAS_META_CACHE = _build_alias_map()


def _get_alias_map() -> dict[str, str]:
    """Return the legacy-name → canonical-name map, building it lazily."""
    _ensure_alias_caches()
    return _ALIAS_MAP_CACHE


def get_alias_metadata() -> dict[str, AliasMeta]:
    """Return the legacy-name → :class:`AliasMeta` map (cached).

    Used by the YAML pre-pass to format a per-alias deprecation warning.
    """
    _ensure_alias_caches()
    return _ALIAS_META_CACHE


def _build_alias_map() -> tuple[dict[str, str], dict[str, AliasMeta]]:
    """Scan every core component dir for ``ALIASES`` declarations.

    Uses :mod:`ast` to read each component's ``__init__.py`` without
    executing it — component import side-effects (logger setup,
    namespace registration, etc.) shouldn't run just because we're
    enumerating aliases.

    Raises if the same alias is claimed by two canonical components, since
    silently picking one would cause non-deterministic routing depending on
    directory-iteration order. Also raises if an alias shadows an existing
    component package: that would hijack a live component domain and, in the
    self-alias case (alias == canonical), send ``_lookup_module`` into
    infinite recursion redirecting a domain to itself.
    """
    import ast

    alias_to_canonical: dict[str, str] = {}
    alias_to_meta: dict[str, AliasMeta] = {}

    if not CORE_COMPONENTS_PATH.is_dir():
        return alias_to_canonical, alias_to_meta

    for child in sorted(CORE_COMPONENTS_PATH.iterdir()):
        if not child.is_dir():
            continue
        init = child / "__init__.py"
        if not init.is_file():
            continue
        aliases, removal_version = _read_aliases(init, ast)
        if not aliases:
            continue
        canonical = child.name
        for alias in aliases:
            if (CORE_COMPONENTS_PATH / alias / "__init__.py").is_file():
                from esphome.core import EsphomeError

                raise EsphomeError(
                    f"Component alias '{alias}' (declared by '{canonical}') "
                    "shadows an existing component package of the same name. "
                    "An alias may only name a component that no longer exists."
                )
            if alias in alias_to_canonical:
                from esphome.core import EsphomeError

                raise EsphomeError(
                    f"Component alias '{alias}' is declared by both "
                    f"'{alias_to_canonical[alias]}' and '{canonical}'. "
                    "Each alias must map to exactly one canonical component."
                )
            alias_to_canonical[alias] = canonical
            alias_to_meta[alias] = AliasMeta(
                canonical=canonical, removal_version=removal_version
            )
    return alias_to_canonical, alias_to_meta


def _read_aliases(
    init_path: Path, ast_module: ModuleType
) -> tuple[list[str], str | None]:
    """Extract ``ALIASES`` and ``ALIAS_REMOVAL_VERSION`` from a component
    ``__init__.py`` via AST parsing.

    Only handles the simple ``NAME = [str_literal, ...]`` / ``NAME = "..."``
    forms — anything more dynamic (function call, conditional, etc.) is
    silently ignored. Components should keep their alias declarations
    static so this scanner can see them.
    """
    try:
        source = init_path.read_text(encoding="utf-8")
    except OSError as err:
        _LOGGER.warning(
            "Could not read %s while scanning for component aliases: %s",
            init_path,
            err,
        )
        return [], None

    # Cheap substring pre-filter: almost no component declares ALIASES, and
    # parsing every component __init__.py with ast is comparatively expensive.
    # Skip the parse entirely unless the token appears in the file at all.
    if "ALIASES" not in source:
        return [], None

    try:
        tree = ast_module.parse(source)
    except SyntaxError as err:
        _LOGGER.warning(
            "Could not parse %s while scanning for component aliases: %s",
            init_path,
            err,
        )
        return [], None

    aliases: list[str] = []
    removal_version: str | None = None

    for node in tree.body:
        if not isinstance(node, ast_module.Assign):
            continue
        for target in node.targets:
            if not isinstance(target, ast_module.Name):
                continue
            if target.id == "ALIASES" and isinstance(node.value, ast_module.List):
                aliases.extend(
                    elt.value
                    for elt in node.value.elts
                    if isinstance(elt, ast_module.Constant)
                    and isinstance(elt.value, str)
                )
            elif (
                target.id == "ALIAS_REMOVAL_VERSION"
                and isinstance(node.value, ast_module.Constant)
                and isinstance(node.value.value, str)
            ):
                removal_version = node.value.value
    return aliases, removal_version


class _AliasFinder(importlib.abc.MetaPathFinder):
    """``sys.meta_path`` finder that resolves legacy-component imports.

    Routes ``esphome.components.<alias>[.<submod>]`` to the canonical
    component's module/submodule of the same name, so external code that
    still imports ``from esphome.components.rp2040 import boards`` keeps
    working without the canonical component having to maintain a shim
    package on disk.

    The finder caches the resolved module in ``sys.modules`` under the
    legacy name on first lookup, so subsequent imports hit the cache and
    skip this finder entirely.
    """

    _PREFIX = "esphome.components."

    def find_spec(self, fullname, path, target=None):  # noqa: ARG002
        if not fullname.startswith(self._PREFIX):
            return None
        # Anything matching the ``esphome.components.`` prefix splits into at
        # least three parts, so ``parts[2]`` (the domain) always exists.
        parts = fullname.split(".")
        domain = parts[2]
        alias_map = _get_alias_map()
        if domain not in alias_map:
            return None

        parts[2] = alias_map[domain]
        canonical_fullname = ".".join(parts)
        try:
            canonical_module = importlib.import_module(canonical_fullname)
        except ModuleNotFoundError as err:
            # Only treat a missing *canonical target* as "no alias to
            # resolve" (let the normal import machinery report it). If some
            # other module is missing, the canonical exists but failed to
            # import one of its own dependencies — surface that real error
            # rather than masking it as an unresolved alias.
            if err.name == canonical_fullname:
                return None
            raise
        # Do NOT pre-populate ``sys.modules[fullname]`` here. Python's
        # ``_find_spec`` (in importlib._bootstrap) has an optimization that
        # detects ``name in sys.modules`` after a finder returns and prefers
        # ``sys.modules[name].__spec__`` over the finder's spec — for an
        # alias, that's the canonical module's own SourceFileLoader spec,
        # which Python then *re-loads*, defeating the aliasing. Letting
        # ``_load_unlocked`` populate sys.modules itself (via our
        # ``_AliasLoader.create_module``) sidesteps that branch.
        return importlib.util.spec_from_loader(fullname, _AliasLoader(canonical_module))


class _AliasLoader(importlib.abc.Loader):
    """No-op loader that returns the already-resolved canonical module.

    :class:`_AliasFinder` populates ``sys.modules`` itself; this loader
    just satisfies the :mod:`importlib` protocol so Python doesn't try to
    re-execute the module.
    """

    def __init__(self, module: ModuleType) -> None:
        self._module = module

    def create_module(self, spec):  # noqa: ARG002
        return self._module

    def exec_module(self, module):  # noqa: ARG002
        # Nothing to execute — the canonical module is already initialized.
        return None


# Register once at module load. Idempotent: re-installing the finder on
# repeated imports (e.g. by tests that reload `esphome.loader`) is a no-op
# because we check for an existing instance first.
def _install_alias_finder() -> None:
    for entry in sys.meta_path:
        if isinstance(entry, _AliasFinder):
            return
    sys.meta_path.append(_AliasFinder())


_install_alias_finder()
