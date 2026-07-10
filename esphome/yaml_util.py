from __future__ import annotations

from collections.abc import Callable, Generator
from contextlib import contextmanager, suppress
from dataclasses import dataclass, field
import functools
import inspect
from io import BytesIO, TextIOBase, TextIOWrapper
from ipaddress import _BaseAddress, _BaseNetwork
import logging
import math
import os
from pathlib import Path
from typing import Any
import uuid

import yaml
from yaml import SafeLoader as PurePythonLoader
import yaml.constructor

try:
    from yaml import CSafeLoader as FastestAvailableSafeLoader
except ImportError:
    FastestAvailableSafeLoader = PurePythonLoader

from esphome import core
from esphome.config_helpers import Extend, Remove
from esphome.const import CONF_DEFAULTS
from esphome.core import (
    CORE,
    DocumentRange,
    EsphomeError,
    Lambda,
    MACAddress,
    TimePeriod,
)
from esphome.expression import has_substitution_or_expression
from esphome.helpers import add_class_to_obj
from esphome.util import OrderedDict, filter_yaml_files

_LOGGER = logging.getLogger(__name__)

# Mostly copied from Home Assistant because that code works fine and
# let's not reinvent the wheel here

SECRET_YAML = "secrets.yaml"
_SECRET_CACHE = {}
_SECRET_VALUES = {}
# Not thread-safe — config processing is single-threaded today.
_load_listeners: list[Callable[[Path], None]] = []

DocumentPath = list[str | int]

# Key under CORE.data used to accumulate keys that a `<<` merge silently
# dropped. The warning is emitted later (see esphome.config.validate_config),
# because the esphome: option that suppresses it isn't known while parsing.
_MERGE_WARNINGS_KEY = "yaml_dropped_merge_keys"


def _record_dropped_merge_key(parent_file: Path, key: Any) -> None:
    """Record a mapping key that a ``<<`` merge silently dropped.

    Merge keys follow the YAML spec's shallow, first-wins semantics: a key that
    already exists in the mapping (or came from an earlier merge) is discarded
    rather than deep-merged the way ``packages:`` would combine it. We collect
    these so a single warning can be emitted once the config is loaded.
    """
    esp_range = getattr(key, "esp_range", None)
    location = str(esp_range.start_mark) if esp_range is not None else str(parent_file)
    CORE.data.setdefault(_MERGE_WARNINGS_KEY, []).append((str(key), location))


def take_dropped_merge_keys() -> list[tuple[str, str]]:
    """Return and clear the keys dropped during ``<<`` merges so far."""
    return CORE.data.pop(_MERGE_WARNINGS_KEY, [])


class SensitiveStr(str):
    """Marker subclass for validated strings that should be masked in
    user-visible YAML output. ``cv.sensitive`` wraps validated values in this
    type so ``dump()`` can render them with ANSI conceal codes without
    needing a post-process regex.
    """

    __slots__ = ()


@contextmanager
def track_yaml_loads() -> Generator[list[Path]]:
    """Context manager that records every file loaded by the YAML loader.

    Yields a list that is populated with resolved Path objects for every
    file loaded through ``_load_yaml_internal`` while the context is active.
    """
    loaded: list[Path] = []

    def _on_load(fname: Path) -> None:
        loaded.append(Path(fname).resolve())

    _load_listeners.append(_on_load)
    try:
        yield loaded
    finally:
        _load_listeners.remove(_on_load)


class ESPHomeDataBase:
    @property
    def esp_range(self):
        return getattr(self, "_esp_range", None)

    @property
    def content_offset(self):
        return getattr(self, "_content_offset", 0)

    def from_node(self, node):
        # pylint: disable=attribute-defined-outside-init
        self._esp_range = DocumentRange.from_marks(node.start_mark, node.end_mark)
        if (
            isinstance(node, yaml.ScalarNode)
            and node.style is not None
            and node.style in "|>"
        ):
            self._content_offset = 1

    def from_database(self, database):
        # pylint: disable=attribute-defined-outside-init
        self._esp_range = database.esp_range
        self._content_offset = database.content_offset


class ESPLiteralValue:
    pass


def make_data_base(
    value, from_database: ESPHomeDataBase = None
) -> ESPHomeDataBase | Any:
    """Wrap a value in a ESPHomeDataBase object."""
    try:
        value = add_class_to_obj(value, ESPHomeDataBase)
        if from_database is not None:
            value.from_database(from_database)
        return value
    except TypeError:
        # Adding class failed, ignore error
        return value


def make_literal(value: Any) -> ESPLiteralValue | Any:
    """Wrap a value in an ESPLiteralValue object."""
    try:
        return add_class_to_obj(value, ESPLiteralValue)
    except TypeError:
        # Adding class failed, ignore error
        return value


def add_context(value: Any, context_vars: dict[str, Any] | None) -> Any:
    """Tags a list/string/dict value with context vars that must be applied to it and its children
    during the substitution pass. If no vars are given, no tagging is done.
    If the value is already tagged, the new context vars are merged with existing ones,
    with new vars taking precedence. Returns the value tagged with ConfigContext. Returns
    the original value if value is not a list/string/dict.
    """
    if isinstance(value, dict) and CONF_DEFAULTS in value:
        context_vars = {
            **value.pop(CONF_DEFAULTS),
            **(context_vars or {}),
        }

    if isinstance(value, ConfigContext):
        value.set_context({**value.vars, **(context_vars or {})})
        return value

    if context_vars and isinstance(value, (dict, list, str, Lambda)):
        value = add_class_to_obj(value, ConfigContext)
        value.set_context(context_vars)
    return value


class ConfigContext:
    """This is a mixin class that holds substitution vars that should be applied
    to the tagged node and its children. During configuration loading, context vars can
    be added to nodes using `add_context` function, which applies the mixin storing
    the captured values and unevaluated expressions.
    The substitution pass then recreates the effective context by merging the context vars
    from this node and parent nodes.
    """

    @property
    def vars(self) -> dict[str, Any]:
        return self._context_vars

    def set_context(self, vars: dict[str, Any]) -> None:
        # pylint: disable=attribute-defined-outside-init
        self._context_vars = vars

    def copy_context_to_children(self) -> None:
        """Propagate context to children.

        isinstance(self, dict/list) works because ConfigContext is dynamically
        mixed into dict/list subclasses via add_class_to_obj in add_context().
        """
        if isinstance(self, dict):
            # pylint: disable=no-member
            tagged = {
                add_context(k, self.vars): add_context(v, self.vars)
                for k, v in self.items()
            }
            self.clear()
            self.update(tagged)
        elif isinstance(self, list):
            for i, item in enumerate(self):
                # pylint: disable=unsupported-assignment-operation
                self[i] = add_context(item, self.vars)


_UNSET = object()


class IncludeFile:
    """Deferred !include that is resolved during the substitution pass.

    Created during YAML parsing instead of loading the file immediately,
    allowing substitution variables to appear in the filename path
    (e.g. ``!include device-${platform}.yaml``). The actual file is
    loaded on the first call to ``load()``, and the result is cached.
    """

    def __init__(
        self,
        parent_file: Path,
        file: Path | str,
        vars: dict[str, Any] | None,
        yaml_loader: Callable[[Path], Any],
    ) -> None:
        self.parent_file = parent_file
        self.file = Path(file)
        self.vars = vars
        self.yaml_loader = yaml_loader
        self._content: Any = _UNSET

    def __repr__(self) -> str:
        return f"IncludeFile({self.file.as_posix()})"

    def load(self) -> Any:
        """Load and cache the included file content.

        Note: returns the cached mutable object on subsequent calls.
        Callers that need to modify the result should copy it first.
        """
        if self._content is not _UNSET:
            return self._content
        if self.has_unresolved_expressions():
            from esphome.config_validation import Invalid

            raise Invalid(
                f"Cannot load include with unresolved substitutions: {self.file}"
            )
        self._content = self.yaml_loader(Path(self.parent_file.parent / self.file))
        self._content = add_context(self._content, self.vars)
        return self._content

    def has_unresolved_expressions(self) -> bool:
        """Check if the filename contains substitution variables or Jinja expressions."""
        return has_substitution_or_expression(str(self.file))


def force_load_include_files(
    obj: Any,
    *,
    warn_on_unresolved: bool = True,
    _seen: set[int] | None = None,
) -> None:
    """Recursively resolve any deferred ``IncludeFile`` instances in a YAML tree.

    Nested ``!include`` returns a deferred ``IncludeFile`` that is only resolved
    later (substitution / packages pass). Callers that need every referenced
    file to actually load — bundle discovery, on-device YAML recovery — invoke
    this while a :func:`track_yaml_loads` listener is active so the underlying
    loader fires and records every reachable file.

    ``IncludeFile`` instances whose path contains unresolved substitution
    variables cannot be loaded. By default a warning is logged for each one;
    pass ``warn_on_unresolved=False`` (used by discovery paths that run on a
    fresh re-parse where substitutions haven't been applied yet) to demote it
    to a debug log.
    """
    if _seen is None:
        _seen = set()

    if isinstance(obj, IncludeFile):
        if id(obj) in _seen:
            return
        _seen.add(id(obj))
        if obj.has_unresolved_expressions():
            log = _LOGGER.warning if warn_on_unresolved else _LOGGER.debug
            log(
                "Cannot resolve !include %s (referenced from %s) with substitutions in path",
                obj.file,
                obj.parent_file,
            )
            return
        try:
            loaded = obj.load()
        except EsphomeError as err:
            _LOGGER.warning(
                "Failed to load !include %s (referenced from %s): %s",
                obj.file,
                obj.parent_file,
                err,
            )
            return
        force_load_include_files(
            loaded, warn_on_unresolved=warn_on_unresolved, _seen=_seen
        )
    elif isinstance(obj, dict):
        if id(obj) in _seen:
            return
        _seen.add(id(obj))
        for value in obj.values():
            force_load_include_files(
                value, warn_on_unresolved=warn_on_unresolved, _seen=_seen
            )
    elif isinstance(obj, (list, tuple)):
        if id(obj) in _seen:
            return
        _seen.add(id(obj))
        for item in obj:
            force_load_include_files(
                item, warn_on_unresolved=warn_on_unresolved, _seen=_seen
            )


@dataclass(slots=True)
class DiscoveredYamlFiles:
    """Result of :func:`discover_user_yaml_files`.

    ``files`` contains every resolved path the YAML loader touched while we
    were re-parsing the user's config; ``secrets`` is the subset whose
    *un-resolved* filename matched :data:`esphome.const.SECRETS_FILES` (so
    a ``secrets.yaml`` symlinked to a differently-named target is still
    flagged as secrets).
    """

    files: list[Path] = field(default_factory=list)
    secrets: set[Path] = field(default_factory=set)


def discover_user_yaml_files(config_path: Path) -> DiscoveredYamlFiles:
    """Fresh-re-parse ``config_path`` and report every file the YAML loader
    pulled in, plus which of them came in under a secrets filename.

    Does NOT run schema validation, substitutions, or package resolution — so
    component-internal YAML loaded by validators (LVGL helpers, dashboard
    imports, etc.) is *not* captured. Deferred ``!include`` references whose
    paths don't depend on substitutions are force-loaded here so they're
    captured too.

    Must run on a fresh parse because :meth:`IncludeFile.load` caches its
    result; on an already-resolved tree :meth:`load` returns without invoking
    the loader and the listener would not fire for the referenced files.
    """
    from esphome.const import SECRETS_FILES

    secrets: set[Path] = set()

    def _capture_secret(fname: Path) -> None:
        if Path(fname).name in SECRETS_FILES:
            secrets.add(Path(fname).resolve())

    with track_yaml_loads() as loaded:
        _load_listeners.append(_capture_secret)
        try:
            try:
                data = load_yaml(config_path)
            except EsphomeError:
                return DiscoveredYamlFiles(list(loaded), secrets)
            force_load_include_files(data, warn_on_unresolved=False)
        finally:
            _load_listeners.remove(_capture_secret)

    # Deduplicate while preserving first-seen order.
    seen: set[Path] = set()
    unique: list[Path] = []
    for path in loaded:
        if path not in seen:
            seen.add(path)
            unique.append(path)
    return DiscoveredYamlFiles(unique, secrets)


def _add_data_ref(fn):
    @functools.wraps(fn)
    def wrapped(loader, node):
        res = fn(loader, node)
        # newer PyYAML versions use generators, resolve them
        if inspect.isgenerator(res):
            generator = res
            res = next(generator)
            # Let generator finish
            for _ in generator:
                pass
        res = make_data_base(res)
        if isinstance(res, ESPHomeDataBase):
            res.from_node(node)
        return res

    return wrapped


_MAX_MERGE_INCLUDE_DEPTH = 10


def _resolve_merge_include(value: Any, node: yaml.Node, value_node: yaml.Node) -> Any:
    """Resolve an IncludeFile (and chains) and propagate context for merge key handling."""
    for _ in range(_MAX_MERGE_INCLUDE_DEPTH):
        if not isinstance(value, IncludeFile):
            break
        if value.has_unresolved_expressions():
            raise yaml.constructor.ConstructorError(
                "While constructing a mapping",
                node.start_mark,
                "Substitution in include filename with merge keys is not supported yet.",
                value_node.start_mark,
            )
        value = value.load()
    else:
        raise yaml.constructor.ConstructorError(
            "While constructing a mapping",
            node.start_mark,
            f"Maximum include chain depth ({_MAX_MERGE_INCLUDE_DEPTH}) exceeded in merge key",
            value_node.start_mark,
        )
    if isinstance(value, ConfigContext):
        # Since the parent dict/list will disappear, propagate
        # context to children now to retain context vars
        value.copy_context_to_children()
    return value


class ESPHomeLoaderMixin:
    """Loader class that keeps track of line numbers."""

    def __init__(
        self, name: Path, yaml_loader: Callable[[Path], dict[str, Any]]
    ) -> None:
        """Initialize the loader."""
        self.name = name
        self.yaml_loader = yaml_loader

    @_add_data_ref
    def construct_yaml_int(self, node):
        return super().construct_yaml_int(node)

    @_add_data_ref
    def construct_yaml_float(self, node):
        return super().construct_yaml_float(node)

    @_add_data_ref
    def construct_yaml_binary(self, node):
        return super().construct_yaml_binary(node)

    @_add_data_ref
    def construct_yaml_omap(self, node):
        return super().construct_yaml_omap(node)

    @_add_data_ref
    def construct_yaml_str(self, node):
        return super().construct_yaml_str(node)

    @_add_data_ref
    def construct_yaml_seq(self, node):
        return super().construct_yaml_seq(node)

    @_add_data_ref
    def construct_yaml_map(self, node: yaml.MappingNode) -> OrderedDict[str, Any]:
        """Traverses the given mapping node and returns a list of constructed key-value pairs."""
        assert isinstance(node, yaml.MappingNode)
        # A list of key-value pairs we find in the current mapping
        pairs = []
        # A list of key-value pairs we find while resolving merges ('<<' key), will be
        # added to pairs in a second pass
        merge_pairs = []
        # A dict of seen keys so far, used to alert the user of duplicate keys and checking
        # which keys to merge.
        # Value of dict items is the start mark of the previous declaration.
        seen_keys = {}

        for key_node, value_node in node.value:
            # merge key is '<<'
            is_merge_key = key_node.tag == "tag:yaml.org,2002:merge"
            # key has no explicit tag set
            is_default_tag = key_node.tag == "tag:yaml.org,2002:value"

            if is_default_tag:
                # Default tag for mapping keys is string
                key_node.tag = "tag:yaml.org,2002:str"

            if not is_merge_key:
                # base case, this is a simple key-value pair
                key = self.construct_object(key_node)
                value = self.construct_object(value_node)

                # Check if key is hashable
                try:
                    hash(key)
                except TypeError:
                    raise yaml.constructor.ConstructorError(
                        f'Invalid key "{key}" (not hashable)', key_node.start_mark
                    ) from None

                key = make_data_base(str(key))
                key.from_node(key_node)

                # Check if it is a duplicate key
                if key in seen_keys:
                    raise yaml.constructor.ConstructorError(
                        f'Duplicate key "{key}"',
                        key_node.start_mark,
                        "NOTE: Previous declaration here:",
                        seen_keys[key],
                    )
                seen_keys[key] = key_node.start_mark

                # Add to pairs
                pairs.append((key, value))
                continue

            # This is a merge key, resolve value and add to merge_pairs
            value = self.construct_object(value_node)

            value = _resolve_merge_include(value, node, value_node)

            if isinstance(value, dict):
                # base case, copy directly to merge_pairs
                # direct merge, like "<<: {some_key: some_value}"
                merge_pairs.extend(value.items())
            elif isinstance(value, list):
                # sequence merge, like "<<: [{some_key: some_value}, {other_key: some_value}]"
                for item in value:
                    item = _resolve_merge_include(item, node, value_node)
                    if not isinstance(item, dict):
                        raise yaml.constructor.ConstructorError(
                            "While constructing a mapping",
                            node.start_mark,
                            f"Expected a mapping for merging, but found {type(item)}",
                            value_node.start_mark,
                        )
                    merge_pairs.extend(item.items())
            else:
                raise yaml.constructor.ConstructorError(
                    "While constructing a mapping",
                    node.start_mark,
                    f"Expected a mapping or list of mappings for merging, but found {type(value)}",
                    value_node.start_mark,
                )

        if merge_pairs:
            # We found some merge keys along the way, merge them into base pairs
            # https://yaml.org/type/merge.html
            # Construct a new merge set with values overridden by current mapping or earlier
            # sequence entries removed
            for key, value in merge_pairs:
                if key in seen_keys:
                    # key already in the current map or from an earlier merge sequence entry,
                    # do not override
                    #
                    # "... each of its key/value pairs is inserted into the current mapping,
                    # unless the key already exists in it."
                    #
                    # "If the value associated with the merge key is a sequence, then this sequence
                    #  is expected to contain mapping nodes and each of these nodes is merged in
                    #  turn according to its order in the sequence. Keys in mapping nodes earlier
                    #  in the sequence override keys specified in later mapping nodes."
                    #
                    # This is a silent shallow drop (unlike `packages:`, which deep-merges).
                    # Record it so a warning can be emitted after the config loads.
                    _record_dropped_merge_key(self.name, key)
                    continue
                pairs.append((key, value))
                # Add key node to seen keys, for sequence merge values.
                seen_keys[key] = None

        return OrderedDict(pairs)

    @_add_data_ref
    def construct_env_var(self, node: yaml.Node) -> str:
        args = node.value.split()
        # Check for a default value
        if len(args) > 1:
            return os.getenv(args[0], " ".join(args[1:]))
        if args[0] in os.environ:
            return os.environ[args[0]]
        raise yaml.MarkedYAMLError(
            f"Environment variable '{node.value}' not defined", node.start_mark
        )

    def _rel_path(self, *args: str) -> Path:
        return self.name.parent / Path(*args)

    @_add_data_ref
    def construct_secret(self, node: yaml.Node) -> str:
        try:
            secrets = self.yaml_loader(self._rel_path(SECRET_YAML))
        except EsphomeError as e:
            if self.name == CORE.config_path:
                raise e
            try:
                main_config_dir = CORE.config_path.parent
                main_secret_yml = main_config_dir / SECRET_YAML
                secrets = self.yaml_loader(main_secret_yml)
            except EsphomeError as er:
                raise EsphomeError(f"{e}\n{er}") from er

        if node.value not in secrets:
            raise yaml.MarkedYAMLError(
                f"Secret '{node.value}' not defined", node.start_mark
            )
        val = secrets[node.value]
        _SECRET_VALUES[str(val)] = node.value
        return val

    @_add_data_ref
    def construct_include(self, node: yaml.Node) -> Any:
        from esphome.const import CONF_VARS

        def extract_file_vars(node):
            fields = self.construct_yaml_map(node)
            file = fields.get("file")
            if file is None:
                raise yaml.MarkedYAMLError("Must include 'file'", node.start_mark)
            vars = fields.get(CONF_VARS)
            return file, vars

        if isinstance(node, yaml.nodes.MappingNode):
            file, vars = extract_file_vars(node)
        else:
            file, vars = node.value, None

        return IncludeFile(self.name, file, vars, self.yaml_loader)

    # Directory includes (!include_dir_*) load eagerly during YAML parsing
    # because their paths are directory names, not individual files, and
    # substitutions in directory paths are not supported.

    @_add_data_ref
    def construct_include_dir_list(self, node: yaml.Node) -> list[dict[str, Any]]:
        files = filter_yaml_files(_find_files(self._rel_path(node.value), "*.yaml"))
        return [self.yaml_loader(f) for f in files]

    @_add_data_ref
    def construct_include_dir_merge_list(self, node: yaml.Node) -> list[dict[str, Any]]:
        files = filter_yaml_files(_find_files(self._rel_path(node.value), "*.yaml"))
        merged_list = []
        for fname in files:
            loaded_yaml = self.yaml_loader(fname)
            if isinstance(loaded_yaml, list):
                merged_list.extend(loaded_yaml)
        return merged_list

    @_add_data_ref
    def construct_include_dir_named(
        self, node: yaml.Node
    ) -> OrderedDict[str, dict[str, Any]]:
        files = filter_yaml_files(_find_files(self._rel_path(node.value), "*.yaml"))
        mapping = OrderedDict()
        for fname in files:
            filename = fname.stem
            mapping[filename] = self.yaml_loader(fname)
        return mapping

    @_add_data_ref
    def construct_include_dir_merge_named(
        self, node: yaml.Node
    ) -> OrderedDict[str, dict[str, Any]]:
        files = filter_yaml_files(_find_files(self._rel_path(node.value), "*.yaml"))
        mapping = OrderedDict()
        for fname in files:
            loaded_yaml = self.yaml_loader(fname)
            if isinstance(loaded_yaml, dict):
                mapping.update(loaded_yaml)
        return mapping

    @_add_data_ref
    def construct_lambda(self, node: yaml.Node) -> Lambda:
        return Lambda(str(node.value))

    @_add_data_ref
    def construct_literal(self, node: yaml.Node) -> ESPLiteralValue:
        obj = None
        if isinstance(node, yaml.ScalarNode):
            obj = self.construct_scalar(node)
        elif isinstance(node, yaml.SequenceNode):
            obj = self.construct_sequence(node)
        elif isinstance(node, yaml.MappingNode):
            obj = self.construct_mapping(node)
        return make_literal(obj)

    @_add_data_ref
    def construct_extend(self, node: yaml.Node) -> Extend:
        return Extend(str(node.value))

    @_add_data_ref
    def construct_remove(self, node: yaml.Node) -> Remove:
        return Remove(str(node.value))


class ESPHomeLoader(ESPHomeLoaderMixin, FastestAvailableSafeLoader):
    """Loader class that keeps track of line numbers."""

    def __init__(
        self,
        stream: TextIOBase | BytesIO,
        name: Path,
        yaml_loader: Callable[[Path], dict[str, Any]],
    ) -> None:
        FastestAvailableSafeLoader.__init__(self, stream)
        ESPHomeLoaderMixin.__init__(self, name, yaml_loader)


class ESPHomePurePythonLoader(ESPHomeLoaderMixin, PurePythonLoader):
    """Loader class that keeps track of line numbers."""

    def __init__(
        self,
        stream: TextIOBase | BytesIO,
        name: Path,
        yaml_loader: Callable[[Path], dict[str, Any]],
    ) -> None:
        PurePythonLoader.__init__(self, stream)
        ESPHomeLoaderMixin.__init__(self, name, yaml_loader)


for _loader in (ESPHomeLoader, ESPHomePurePythonLoader):
    _loader.add_constructor("tag:yaml.org,2002:int", _loader.construct_yaml_int)
    _loader.add_constructor("tag:yaml.org,2002:float", _loader.construct_yaml_float)
    _loader.add_constructor("tag:yaml.org,2002:binary", _loader.construct_yaml_binary)
    _loader.add_constructor("tag:yaml.org,2002:omap", _loader.construct_yaml_omap)
    _loader.add_constructor("tag:yaml.org,2002:str", _loader.construct_yaml_str)
    _loader.add_constructor("tag:yaml.org,2002:seq", _loader.construct_yaml_seq)
    _loader.add_constructor("tag:yaml.org,2002:map", _loader.construct_yaml_map)
    _loader.add_constructor("!env_var", _loader.construct_env_var)
    _loader.add_constructor("!secret", _loader.construct_secret)
    _loader.add_constructor("!include", _loader.construct_include)
    _loader.add_constructor("!include_dir_list", _loader.construct_include_dir_list)
    _loader.add_constructor(
        "!include_dir_merge_list", _loader.construct_include_dir_merge_list
    )
    _loader.add_constructor("!include_dir_named", _loader.construct_include_dir_named)
    _loader.add_constructor(
        "!include_dir_merge_named", _loader.construct_include_dir_merge_named
    )
    _loader.add_constructor("!lambda", _loader.construct_lambda)
    _loader.add_constructor("!literal", _loader.construct_literal)
    _loader.add_constructor("!extend", _loader.construct_extend)
    _loader.add_constructor("!remove", _loader.construct_remove)


def load_yaml(fname: Path, clear_secrets: bool = True) -> Any:
    if clear_secrets:
        _SECRET_VALUES.clear()
        _SECRET_CACHE.clear()
    return _load_yaml_internal(fname)


def _load_yaml_internal(fname: Path) -> Any:
    """Load a YAML file."""
    for listener in _load_listeners:
        listener(fname)
    try:
        with fname.open(encoding="utf-8") as f_handle:
            res = parse_yaml(fname, f_handle)
    except (UnicodeDecodeError, OSError) as err:
        raise EsphomeError(f"Error reading file {fname}: {err}") from err
    # Top-level !include returns a deferred IncludeFile; resolve it so
    # callers always receive the final content.
    if isinstance(res, IncludeFile):
        res = res.load()
    return res


def parse_yaml(file_name: Path, file_handle: TextIOWrapper, yaml_loader=None) -> Any:
    """Parse a YAML file."""
    if yaml_loader is None:
        yaml_loader = _load_yaml_internal
    try:
        return _load_yaml_internal_with_type(
            ESPHomeLoader, file_name, file_handle, yaml_loader
        )
    except EsphomeError:
        # Loading failed, so we now load with the Python loader which has more
        # readable exceptions
        # Rewind the stream so we can try again
        file_handle.seek(0, 0)
        return _load_yaml_internal_with_type(
            ESPHomePurePythonLoader, file_name, file_handle, yaml_loader
        )


def _load_yaml_internal_with_type(
    loader_type: type[ESPHomeLoader | ESPHomePurePythonLoader],
    fname: Path,
    content: TextIOWrapper,
    yaml_loader: Callable[[Path], dict[str, Any]],
) -> Any:
    """Load a YAML file.

    Supports an optional leading YAML frontmatter document: when the file
    contains two YAML documents separated by ``---``, the first document is
    treated as metadata and stored in :attr:`CORE.frontmatter` keyed by the
    resolved file path, while the second document is returned as the actual
    configuration. Frontmatter is ignored by config validation and code
    generation.
    """
    loader = loader_type(content, fname, yaml_loader)
    try:
        documents: list[Any] = []
        while loader.check_data():
            documents.append(loader.get_data())
        if len(documents) > 2:
            raise EsphomeError(
                f"YAML file '{fname}' contains {len(documents)} documents but "
                f"at most two are supported (an optional frontmatter document "
                f"followed by the configuration)."
            )
        if len(documents) == 2:
            frontmatter = documents[0]
            config = documents[1]
            if frontmatter is not None:
                CORE.frontmatter[Path(fname).resolve()] = frontmatter
            return config if config is not None else OrderedDict()
        if len(documents) == 1:
            return documents[0] or OrderedDict()
        return OrderedDict()
    except yaml.YAMLError as exc:
        raise EsphomeError(exc) from exc
    finally:
        loader.dispose()


def dump(dict_, show_secrets=False, sort_keys=False):
    """Dump YAML to a string and remove null."""
    if show_secrets:
        _SECRET_VALUES.clear()
        _SECRET_CACHE.clear()

    # Per-call subclass so the redaction flag doesn't leak across calls.
    # (``_SECRET_VALUES`` / ``_SECRET_CACHE`` remain module globals; YAML
    # processing is single-threaded today, so this isolates only the flag.)
    class _Dumper(ESPHomeDumper):
        _redact_sensitive = not show_secrets

    return yaml.dump(
        dict_,
        default_flow_style=False,
        allow_unicode=True,
        Dumper=_Dumper,
        sort_keys=sort_keys,
    )


def _is_file_valid(name: str) -> bool:
    """Decide if a file is valid."""
    return not name.startswith(".")


def _find_files(directory: Path, pattern):
    """Recursively load files in a directory."""
    for root, dirs, files in os.walk(directory):
        dirs[:] = [d for d in dirs if _is_file_valid(d)]
        for f in files:
            filename = Path(f)
            if _is_file_valid(f) and filename.match(pattern):
                filename = Path(root) / filename
                yield filename


def is_secret(value):
    try:
        return _SECRET_VALUES[str(value)]
    except (KeyError, ValueError):
        return None


def _path_doc(item: Any) -> str | None:
    """Return the source document name if *item* carries location info."""
    if isinstance(item, ESPHomeDataBase) and (r := item.esp_range) is not None:
        return r.start_mark.document
    return None


def _fmt_mark(loc: Any) -> str:
    """Render a DocumentLocation as a 1-based 'file line:col' string."""
    return f"{loc.document} {loc.line + 1}:{loc.column + 1}"


def _obj_loc(obj: Any) -> str:
    """Return formatted source location for *obj*, or '' if it has none."""
    if isinstance(obj, ESPHomeDataBase) and (r := obj.esp_range) is not None:
        return _fmt_mark(r.start_mark)
    return ""


def _fmt_segment(seg: list) -> str:
    """Format a path segment, rendering integers as [n] subscripts."""
    parts: list[str] = []
    for item in seg:
        if isinstance(item, int):
            if parts:
                parts[-1] = f"{parts[-1]}[{item}]"
            else:
                parts.append(f"[{item}]")
        else:
            parts.append(str(item))
    return "->".join(parts)


def _split_into_frames(
    path: DocumentPath,
) -> list[tuple[list, str]]:
    """Group *path* into per-file frames at include boundaries.

    A "frame" is the slice of the path that belongs to one source document.
    Each path item is either:

      * a **located key** — has an ``ESPHomeDataBase`` source mark; this is
        what tells us which document owns the surrounding keys.
      * an **integer** — a list subscript; always attaches to the open frame
        (renders as ``foo[3]`` on the previous name).
      * an **unlocated string** — a key with no source mark (e.g. constants
        like ``CONF_PACKAGES``); it describes the parent of the *next* file,
        so it migrates to the next frame when the document changes.

    Returns a list of ``(items, "file line:col")`` tuples in walk order
    (outermost frame first).
    """
    frames: list[tuple[list, str]] = []
    open_frame: list = []
    next_frame_keys: list = []  # unlocated strings buffered for the next frame
    open_doc: str | None = None
    open_loc = ""

    for item in path:
        doc = _path_doc(item)
        if doc is None:
            # Ints subscript the open frame's last name; everything else
            # (strings, or leading ints with no open frame) is buffered for
            # the next frame.
            if isinstance(item, int) and open_doc is not None:
                open_frame.append(item)
            else:
                next_frame_keys.append(item)
            continue
        if open_doc is not None and doc != open_doc:
            # Crossed an include boundary: close the open frame.
            frames.append((open_frame, open_loc))
            open_frame = []
        open_frame.extend(next_frame_keys)
        next_frame_keys.clear()
        open_frame.append(item)
        open_doc = doc
        open_loc = _fmt_mark(item.esp_range.start_mark)

    if open_doc is not None:
        # Trailing buffered keys belong to the innermost (last) frame.
        open_frame.extend(next_frame_keys)
        frames.append((open_frame, open_loc))
    return frames


def format_path(path: DocumentPath, current_obj: Any) -> str:
    """Build a human-readable include stack from a config path.

    Each YAML key in *path* that carries an ``ESPHomeDataBase`` ``esp_range``
    reveals which file it came from.  When the source document changes between
    consecutive such keys, that is an include boundary.  The path is split
    into per-file frames and formatted innermost-first, e.g.::

        In: packages->roam in common/package/wifi.yaml 26:10
          Included from packages->net in common/hardware.yaml 44:2
          Included from packages->device in my_project.yaml 11:2

    The innermost ``In:`` line uses the location from *current_obj* when
    available (the value that triggered the error) for extra precision.
    """
    frames = _split_into_frames(path)
    obj_loc = _obj_loc(current_obj)

    if not frames:
        # No source info anywhere in the path: render as a flat path,
        # using current_obj's location if it happens to have one.
        suffix = f" in {obj_loc}" if obj_loc else ""
        return f"In: {_fmt_segment(path)}{suffix}"

    inner_seg, inner_loc = frames[-1]
    lines = [f"In: {_fmt_segment(inner_seg)} in {obj_loc or inner_loc}"]
    for seg, loc in reversed(frames[:-1]):
        lines.append(f"  Included from {_fmt_segment(seg)} in {loc}")
    return "\n".join(lines)


class ESPHomeDumper(yaml.SafeDumper):
    # Default for the base class; per-call subclass in ``dump()`` overrides.
    # When True, ``represent_sensitive`` wraps values in ANSI conceal codes.
    _redact_sensitive: bool = False

    def represent_mapping(self, tag, mapping, flow_style=None):
        value = []
        node = yaml.MappingNode(tag, value, flow_style=flow_style)
        if self.alias_key is not None:
            self.represented_objects[self.alias_key] = node
        best_style = True
        if hasattr(mapping, "items"):
            mapping = list(mapping.items())
        if self.sort_keys:
            with suppress(TypeError):
                mapping = sorted(mapping)
        for item_key, item_value in mapping:
            node_key = self.represent_data(item_key)
            node_value = self.represent_data(item_value)
            if not (isinstance(node_key, yaml.ScalarNode) and not node_key.style):
                best_style = False
            if not (isinstance(node_value, yaml.ScalarNode) and not node_value.style):
                best_style = False
            value.append((node_key, node_value))
        if flow_style is None:
            if self.default_flow_style is not None:
                node.flow_style = self.default_flow_style
            else:
                node.flow_style = best_style
        return node

    def represent_secret(self, value):
        return self.represent_scalar(tag="!secret", value=_SECRET_VALUES[str(value)])

    def represent_stringify(self, value):
        if is_secret(value):
            return self.represent_secret(value)
        return self.represent_scalar(tag="tag:yaml.org,2002:str", value=str(value))

    def represent_sensitive(self, value: SensitiveStr) -> yaml.ScalarNode:
        # Only the redact-and-not-a-secret branch is unique to sensitive
        # values; otherwise let ``represent_stringify`` handle ``!secret``
        # precedence and the plain-str fallthrough. Conceal sequence is
        # emitted as literal ``\033`` text (not actual ESC bytes) so the
        # output matches the prior regex format and device-builder's
        # ``\033[8m...\033[28m`` parser keeps working.
        if self._redact_sensitive and not is_secret(value):
            return self.represent_scalar(
                tag="tag:yaml.org,2002:str",
                value=f"\\033[8m{value}\\033[28m",
            )
        return self.represent_stringify(value)

    # pylint: disable=arguments-renamed
    def represent_bool(self, value):
        return self.represent_scalar(
            "tag:yaml.org,2002:bool", "true" if value else "false"
        )

    # pylint: disable=arguments-renamed
    def represent_int(self, value):
        if is_secret(value):
            return self.represent_secret(value)
        return self.represent_scalar(tag="tag:yaml.org,2002:int", value=str(value))

    # pylint: disable=arguments-renamed
    def represent_float(self, value):
        if is_secret(value):
            return self.represent_secret(value)
        if math.isnan(value):
            value = ".nan"
        elif math.isinf(value):
            value = ".inf" if value > 0 else "-.inf"
        else:
            value = str(repr(value)).lower()
            # Note that in some cases `repr(data)` represents a float number
            # without the decimal parts.  For instance:
            #   >>> repr(1e17)
            #   '1e17'
            # Unfortunately, this is not a valid float representation according
            # to the definition of the `!!float` tag.  We fix this by adding
            # '.0' before the 'e' symbol.
            if "." not in value and "e" in value:
                value = value.replace("e", ".0e", 1)
        return self.represent_scalar(tag="tag:yaml.org,2002:float", value=value)

    def represent_lambda(self, value):
        if is_secret(value.value):
            return self.represent_secret(value.value)
        return self.represent_scalar(tag="!lambda", value=value.value, style="|")

    def represent_extend(self, value):
        return self.represent_scalar(tag="!extend", value=value.value)

    def represent_remove(self, value):
        return self.represent_scalar(tag="!remove", value=value.value)

    def represent_include_file(self, value):
        if value.vars:
            mapping = {"file": value.file.as_posix(), "vars": value.vars}
            return self.represent_mapping(
                tag="!include", mapping=mapping, flow_style=False
            )
        return self.represent_scalar(tag="!include", value=value.file.as_posix())

    def represent_id(self, value):
        if is_secret(value.id):
            return self.represent_secret(value.id)
        return self.represent_stringify(value.id)

    # The below override configures this dumper to indent output YAML properly:
    def increase_indent(self, flow=False, indentless=False):
        return super().increase_indent(flow, False)


ESPHomeDumper.add_multi_representer(
    dict, lambda dumper, value: dumper.represent_mapping("tag:yaml.org,2002:map", value)
)
ESPHomeDumper.add_multi_representer(
    list,
    lambda dumper, value: dumper.represent_sequence("tag:yaml.org,2002:seq", value),
)
ESPHomeDumper.add_multi_representer(bool, ESPHomeDumper.represent_bool)
ESPHomeDumper.add_multi_representer(str, ESPHomeDumper.represent_stringify)
# MRO-walked dispatch; SensitiveStr's own entry wins over the str one.
ESPHomeDumper.add_multi_representer(SensitiveStr, ESPHomeDumper.represent_sensitive)
ESPHomeDumper.add_multi_representer(int, ESPHomeDumper.represent_int)
ESPHomeDumper.add_multi_representer(float, ESPHomeDumper.represent_float)
ESPHomeDumper.add_multi_representer(_BaseAddress, ESPHomeDumper.represent_stringify)
ESPHomeDumper.add_multi_representer(_BaseNetwork, ESPHomeDumper.represent_stringify)
ESPHomeDumper.add_multi_representer(MACAddress, ESPHomeDumper.represent_stringify)
ESPHomeDumper.add_multi_representer(TimePeriod, ESPHomeDumper.represent_stringify)
ESPHomeDumper.add_multi_representer(Lambda, ESPHomeDumper.represent_lambda)
ESPHomeDumper.add_multi_representer(Extend, ESPHomeDumper.represent_extend)
ESPHomeDumper.add_multi_representer(Remove, ESPHomeDumper.represent_remove)
ESPHomeDumper.add_multi_representer(core.ID, ESPHomeDumper.represent_id)
ESPHomeDumper.add_multi_representer(uuid.UUID, ESPHomeDumper.represent_stringify)
ESPHomeDumper.add_multi_representer(Path, ESPHomeDumper.represent_stringify)
ESPHomeDumper.add_multi_representer(IncludeFile, ESPHomeDumper.represent_include_file)
