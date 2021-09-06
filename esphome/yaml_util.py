import fnmatch
import functools
import inspect
import logging
import math
import os

import uuid
import yaml
import yaml.constructor

from esphome import core
from esphome.config_helpers import read_config_file
from esphome.core import (
    EsphomeError,
    IPAddress,
    Lambda,
    MACAddress,
    TimePeriod,
    DocumentRange,
)
from esphome.helpers import add_class_to_obj
from esphome.util import OrderedDict, filter_yaml_files

_LOGGER = logging.getLogger(__name__)

# Mostly copied from Home Assistant because that code works fine and
# let's not reinvent the wheel here

SECRET_YAML = "secrets.yaml"
_SECRET_CACHE = {}
_SECRET_VALUES = {}


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
        if isinstance(node, yaml.ScalarNode):
            if node.style is not None and node.style in "|>":
                self._content_offset = 1

    def from_database(self, database):
        # pylint: disable=attribute-defined-outside-init
        self._esp_range = database.esp_range
        self._content_offset = database.content_offset


class ESPForceValue:
    pass


def make_data_base(value, from_database: ESPHomeDataBase = None):
    try:
        value = add_class_to_obj(value, ESPHomeDataBase)
        if from_database is not None:
            value.from_database(from_database)
        return value
    except TypeError:
        # Adding class failed, ignore error
        return value


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


class ESPHomeLoader(yaml.SafeLoader):  # pylint: disable=too-many-ancestors
    """Loader class that keeps track of line numbers."""

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
    def construct_yaml_map(self, node):
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
                    # pylint: disable=raise-missing-from
                    raise yaml.constructor.ConstructorError(
                        f'Invalid key "{key}" (not hashable)', key_node.start_mark
                    )

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
            if isinstance(value, dict):
                # base case, copy directly to merge_pairs
                # direct merge, like "<<: {some_key: some_value}"
                merge_pairs.extend(value.items())
            elif isinstance(value, list):
                # sequence merge, like "<<: [{some_key: some_value}, {other_key: some_value}]"
                for item in value:
                    if not isinstance(item, dict):
                        raise yaml.constructor.ConstructorError(
                            "While constructing a mapping",
                            node.start_mark,
                            "Expected a mapping for merging, but found {}".format(
                                type(item)
                            ),
                            value_node.start_mark,
                        )
                    merge_pairs.extend(item.items())
            else:
                raise yaml.constructor.ConstructorError(
                    "While constructing a mapping",
                    node.start_mark,
                    "Expected a mapping or list of mappings for merging, "
                    "but found {}".format(type(value)),
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
                    continue
                pairs.append((key, value))
                # Add key node to seen keys, for sequence merge values.
                seen_keys[key] = None

        return OrderedDict(pairs)

    @_add_data_ref
    def construct_env_var(self, node):
        args = node.value.split()
        # Check for a default value
        if len(args) > 1:
            return os.getenv(args[0], " ".join(args[1:]))
        if args[0] in os.environ:
            return os.environ[args[0]]
        raise yaml.MarkedYAMLError(
            f"Environment variable '{node.value}' not defined", node.start_mark
        )

    @property
    def _directory(self):
        return os.path.dirname(self.name)

    def _rel_path(self, *args):
        return os.path.join(self._directory, *args)

    @_add_data_ref
    def construct_secret(self, node):
        secrets = _load_yaml_internal(self._rel_path(SECRET_YAML))
        if node.value not in secrets:
            raise yaml.MarkedYAMLError(
                f"Secret '{node.value}' not defined", node.start_mark
            )
        val = secrets[node.value]
        _SECRET_VALUES[str(val)] = node.value
        return val

    @_add_data_ref
    def construct_include(self, node):
        return _load_yaml_internal(self._rel_path(node.value))

    @_add_data_ref
    def construct_include_dir_list(self, node):
        files = filter_yaml_files(_find_files(self._rel_path(node.value), "*.yaml"))
        return [_load_yaml_internal(f) for f in files]

    @_add_data_ref
    def construct_include_dir_merge_list(self, node):
        files = filter_yaml_files(_find_files(self._rel_path(node.value), "*.yaml"))
        merged_list = []
        for fname in files:
            loaded_yaml = _load_yaml_internal(fname)
            if isinstance(loaded_yaml, list):
                merged_list.extend(loaded_yaml)
        return merged_list

    @_add_data_ref
    def construct_include_dir_named(self, node):
        files = filter_yaml_files(_find_files(self._rel_path(node.value), "*.yaml"))
        mapping = OrderedDict()
        for fname in files:
            filename = os.path.splitext(os.path.basename(fname))[0]
            mapping[filename] = _load_yaml_internal(fname)
        return mapping

    @_add_data_ref
    def construct_include_dir_merge_named(self, node):
        files = filter_yaml_files(_find_files(self._rel_path(node.value), "*.yaml"))
        mapping = OrderedDict()
        for fname in files:
            loaded_yaml = _load_yaml_internal(fname)
            if isinstance(loaded_yaml, dict):
                mapping.update(loaded_yaml)
        return mapping

    @_add_data_ref
    def construct_lambda(self, node):
        return Lambda(str(node.value))

    @_add_data_ref
    def construct_force(self, node):
        obj = self.construct_scalar(node)
        return add_class_to_obj(obj, ESPForceValue)


ESPHomeLoader.add_constructor("tag:yaml.org,2002:int", ESPHomeLoader.construct_yaml_int)
ESPHomeLoader.add_constructor(
    "tag:yaml.org,2002:float", ESPHomeLoader.construct_yaml_float
)
ESPHomeLoader.add_constructor(
    "tag:yaml.org,2002:binary", ESPHomeLoader.construct_yaml_binary
)
ESPHomeLoader.add_constructor(
    "tag:yaml.org,2002:omap", ESPHomeLoader.construct_yaml_omap
)
ESPHomeLoader.add_constructor("tag:yaml.org,2002:str", ESPHomeLoader.construct_yaml_str)
ESPHomeLoader.add_constructor("tag:yaml.org,2002:seq", ESPHomeLoader.construct_yaml_seq)
ESPHomeLoader.add_constructor("tag:yaml.org,2002:map", ESPHomeLoader.construct_yaml_map)
ESPHomeLoader.add_constructor("!env_var", ESPHomeLoader.construct_env_var)
ESPHomeLoader.add_constructor("!secret", ESPHomeLoader.construct_secret)
ESPHomeLoader.add_constructor("!include", ESPHomeLoader.construct_include)
ESPHomeLoader.add_constructor(
    "!include_dir_list", ESPHomeLoader.construct_include_dir_list
)
ESPHomeLoader.add_constructor(
    "!include_dir_merge_list", ESPHomeLoader.construct_include_dir_merge_list
)
ESPHomeLoader.add_constructor(
    "!include_dir_named", ESPHomeLoader.construct_include_dir_named
)
ESPHomeLoader.add_constructor(
    "!include_dir_merge_named", ESPHomeLoader.construct_include_dir_merge_named
)
ESPHomeLoader.add_constructor("!lambda", ESPHomeLoader.construct_lambda)
ESPHomeLoader.add_constructor("!force", ESPHomeLoader.construct_force)


def load_yaml(fname):
    _SECRET_VALUES.clear()
    _SECRET_CACHE.clear()
    return _load_yaml_internal(fname)


def _load_yaml_internal(fname):
    content = read_config_file(fname)
    loader = ESPHomeLoader(content)
    loader.name = fname
    try:
        return loader.get_single_data() or OrderedDict()
    except yaml.YAMLError as exc:
        raise EsphomeError(exc) from exc
    finally:
        loader.dispose()


def dump(dict_):
    """Dump YAML to a string and remove null."""
    return yaml.dump(
        dict_, default_flow_style=False, allow_unicode=True, Dumper=ESPHomeDumper
    )


def _is_file_valid(name):
    """Decide if a file is valid."""
    return not name.startswith(".")


def _find_files(directory, pattern):
    """Recursively load files in a directory."""
    for root, dirs, files in os.walk(directory, topdown=True):
        dirs[:] = [d for d in dirs if _is_file_valid(d)]
        for basename in files:
            if _is_file_valid(basename) and fnmatch.fnmatch(basename, pattern):
                filename = os.path.join(root, basename)
                yield filename


def is_secret(value):
    try:
        return _SECRET_VALUES[str(value)]
    except (KeyError, ValueError):
        return None


class ESPHomeDumper(yaml.SafeDumper):  # pylint: disable=too-many-ancestors
    def represent_mapping(self, tag, mapping, flow_style=None):
        value = []
        node = yaml.MappingNode(tag, value, flow_style=flow_style)
        if self.alias_key is not None:
            self.represented_objects[self.alias_key] = node
        best_style = True
        if hasattr(mapping, "items"):
            mapping = list(mapping.items())
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

    def represent_id(self, value):
        if is_secret(value.id):
            return self.represent_secret(value.id)
        return self.represent_stringify(value.id)


ESPHomeDumper.add_multi_representer(
    dict, lambda dumper, value: dumper.represent_mapping("tag:yaml.org,2002:map", value)
)
ESPHomeDumper.add_multi_representer(
    list,
    lambda dumper, value: dumper.represent_sequence("tag:yaml.org,2002:seq", value),
)
ESPHomeDumper.add_multi_representer(bool, ESPHomeDumper.represent_bool)
ESPHomeDumper.add_multi_representer(str, ESPHomeDumper.represent_stringify)
ESPHomeDumper.add_multi_representer(int, ESPHomeDumper.represent_int)
ESPHomeDumper.add_multi_representer(float, ESPHomeDumper.represent_float)
ESPHomeDumper.add_multi_representer(IPAddress, ESPHomeDumper.represent_stringify)
ESPHomeDumper.add_multi_representer(MACAddress, ESPHomeDumper.represent_stringify)
ESPHomeDumper.add_multi_representer(TimePeriod, ESPHomeDumper.represent_stringify)
ESPHomeDumper.add_multi_representer(Lambda, ESPHomeDumper.represent_lambda)
ESPHomeDumper.add_multi_representer(core.ID, ESPHomeDumper.represent_id)
ESPHomeDumper.add_multi_representer(uuid.UUID, ESPHomeDumper.represent_stringify)
