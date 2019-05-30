from __future__ import print_function

import fnmatch
import functools
import logging
import math
import os

import uuid
import yaml
import yaml.constructor

from esphome import core
from esphome.config_helpers import read_config_file
from esphome.core import EsphomeError, IPAddress, Lambda, MACAddress, TimePeriod, DocumentRange
from esphome.py_compat import text_type, IS_PY2
from esphome.util import OrderedDict

_LOGGER = logging.getLogger(__name__)

# Mostly copied from Home Assistant because that code works fine and
# let's not reinvent the wheel here

SECRET_YAML = u'secrets.yaml'
_SECRET_CACHE = {}
_SECRET_VALUES = {}


class NodeListClass(list):
    pass


class NodeStrClass(text_type):
    pass


class ESPHomeDataBase(object):
    @property
    def esp_range(self):
        return getattr(self, '_esp_range', None)

    def from_node(self, node):
        # pylint: disable=attribute-defined-outside-init
        self._esp_range = DocumentRange.from_marks(node.start_mark, node.end_mark)


class ESPInt(int, ESPHomeDataBase):
    pass


class ESPFloat(float, ESPHomeDataBase):
    pass


class ESPStr(str, ESPHomeDataBase):
    pass


class ESPDict(OrderedDict, ESPHomeDataBase):
    pass


class ESPList(list, ESPHomeDataBase):
    pass


class ESPLambda(Lambda, ESPHomeDataBase):
    pass


ESP_TYPES = {
    int: ESPInt,
    float: ESPFloat,
    str: ESPStr,
    dict: ESPDict,
    list: ESPList,
    Lambda: ESPLambda,
}
if IS_PY2:
    class ESPUnicode(unicode, ESPHomeDataBase):
        pass

    ESP_TYPES[unicode] = ESPUnicode


def make_data_base(value):
    for typ, cons in ESP_TYPES.items():
        if isinstance(value, typ):
            return cons(value)
    return value


def _add_data_ref(fn):
    @functools.wraps(fn)
    def wrapped(loader, node):
        res = fn(loader, node)
        res = make_data_base(res)
        if isinstance(res, ESPHomeDataBase):
            res.from_node(node)
        return res

    return wrapped


class ESPHomeLoader(yaml.SafeLoader):  # pylint: disable=too-many-ancestors
    """Loader class that keeps track of line numbers."""

    @_add_data_ref
    def construct_yaml_int(self, node):
        return super(ESPHomeLoader, self).construct_yaml_int(node)

    @_add_data_ref
    def construct_yaml_float(self, node):
        return super(ESPHomeLoader, self).construct_yaml_float(node)

    @_add_data_ref
    def construct_yaml_binary(self, node):
        return super(ESPHomeLoader, self).construct_yaml_binary(node)

    @_add_data_ref
    def construct_yaml_omap(self, node):
        return super(ESPHomeLoader, self).construct_yaml_omap(node)

    @_add_data_ref
    def construct_yaml_str(self, node):
        return super(ESPHomeLoader, self).construct_yaml_str(node)

    @_add_data_ref
    def construct_yaml_seq(self, node):
        return super(ESPHomeLoader, self).construct_yaml_seq(node)

    def custom_flatten_mapping(self, node):
        pre_merge = []
        post_merge = []
        index = 0
        while index < len(node.value):
            if isinstance(node.value[index], yaml.ScalarNode):
                index += 1
                continue

            key_node, value_node = node.value[index]
            if key_node.tag == u'tag:yaml.org,2002:merge':
                del node.value[index]

                if isinstance(value_node, yaml.MappingNode):
                    self.custom_flatten_mapping(value_node)
                    node.value = node.value[:index] + value_node.value + node.value[index:]
                elif isinstance(value_node, yaml.SequenceNode):
                    submerge = []
                    for subnode in value_node.value:
                        if not isinstance(subnode, yaml.MappingNode):
                            raise yaml.constructor.ConstructorError(
                                "while constructing a mapping", node.start_mark,
                                "expected a mapping for merging, but found %{}".format(subnode.id),
                                subnode.start_mark)
                        self.custom_flatten_mapping(subnode)
                        submerge.append(subnode.value)
                    # submerge.reverse()
                    node.value = node.value[:index] + submerge + node.value[index:]
                elif isinstance(value_node, yaml.ScalarNode):
                    node.value = node.value[:index] + [value_node] + node.value[index:]
                    # post_merge.append(value_node)
                else:
                    raise yaml.constructor.ConstructorError(
                        "while constructing a mapping", node.start_mark,
                        "expected a mapping or list of mappings for merging, "
                        "but found {}".format(value_node.id), value_node.start_mark)
            elif key_node.tag == u'tag:yaml.org,2002:value':
                key_node.tag = u'tag:yaml.org,2002:str'
                index += 1
            else:
                index += 1
        if pre_merge:
            node.value = pre_merge + node.value
        if post_merge:
            node.value = node.value + post_merge

    def custom_construct_pairs(self, node):
        pairs = []
        for kv in node.value:
            if isinstance(kv, yaml.ScalarNode):
                obj = self.construct_object(kv)
                if not isinstance(obj, dict):
                    raise EsphomeError(
                        "Expected mapping for anchored include tag, got {}".format(type(obj)))
                for key, value in obj.items():
                    pairs.append((key, value))
            else:
                key_node, value_node = kv
                key = self.construct_object(key_node)
                value = self.construct_object(value_node)
                pairs.append((key, value))

        return pairs

    @_add_data_ref
    def construct_yaml_map(self, node):
        self.custom_flatten_mapping(node)
        nodes = self.custom_construct_pairs(node)

        seen = {}
        for (key, _), nv in zip(nodes, node.value):
            if isinstance(nv, yaml.ScalarNode):
                line = nv.start_mark.line
            else:
                line = nv[0].start_mark.line

            try:
                hash(key)
            except TypeError:
                raise yaml.MarkedYAMLError(
                    context="invalid key: \"{}\"".format(key),
                    context_mark=yaml.Mark(self.name, 0, line, -1, None, None)
                )

            if key in seen:
                raise yaml.MarkedYAMLError(
                    context="duplicate key: \"{}\"".format(key),
                    context_mark=yaml.Mark(self.name, 0, line, -1, None, None)
                )
            seen[key] = line

        return OrderedDict(nodes)

    @_add_data_ref
    def construct_env_var(self, node):
        args = node.value.split()
        # Check for a default value
        if len(args) > 1:
            return os.getenv(args[0], u' '.join(args[1:]))
        if args[0] in os.environ:
            return os.environ[args[0]]
        raise yaml.MarkedYAMLError(
            context=u"Environment variable '{}' not defined".format(node.value),
            context_mark=node.start_mark
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
                context=u"Secret '{}' not defined".format(node.value),
                context_mark=node.start_mark
            )
        val = secrets[node.value]
        _SECRET_VALUES[text_type(val)] = node.value
        return val

    @_add_data_ref
    def construct_include(self, node):
        return _load_yaml_internal(self._rel_path(node.value))

    @_add_data_ref
    def construct_include_dir_list(self, node):
        files = _filter_yaml_files(_find_files(self._rel_path(node.value), '*.yaml'))
        return [_load_yaml_internal(f) for f in files]

    @_add_data_ref
    def construct_include_dir_merge_list(self, node):
        files = _filter_yaml_files(_find_files(self._rel_path(node.value), '*.yaml'))
        merged_list = []
        for fname in files:
            loaded_yaml = _load_yaml_internal(fname)
            if isinstance(loaded_yaml, list):
                merged_list.extend(loaded_yaml)
        return merged_list

    @_add_data_ref
    def construct_include_dir_named(self, node):
        files = _filter_yaml_files(_find_files(self._rel_path(node.value), '*.yaml'))
        mapping = OrderedDict()
        for fname in files:
            filename = os.path.splitext(os.path.basename(fname))[0]
            mapping[filename] = _load_yaml_internal(fname)
        return mapping

    @_add_data_ref
    def construct_include_dir_merge_named(self, node):
        files = _filter_yaml_files(_find_files(self._rel_path(node.value), '*.yaml'))
        mapping = OrderedDict()
        for fname in files:
            loaded_yaml = _load_yaml_internal(fname)
            if isinstance(loaded_yaml, dict):
                mapping.update(loaded_yaml)
        return mapping

    @_add_data_ref
    def construct_lambda(self, node):
        return Lambda(text_type(node.value))


def _filter_yaml_files(files):
    files = [f for f in files if os.path.basename(f) != SECRET_YAML]
    files = [f for f in files if not os.path.basename(f).startswith('.')]
    return files


ESPHomeLoader.add_constructor(u'tag:yaml.org,2002:int', ESPHomeLoader.construct_yaml_int)
ESPHomeLoader.add_constructor(u'tag:yaml.org,2002:float', ESPHomeLoader.construct_yaml_float)
ESPHomeLoader.add_constructor(u'tag:yaml.org,2002:binary', ESPHomeLoader.construct_yaml_binary)
ESPHomeLoader.add_constructor(u'tag:yaml.org,2002:omap', ESPHomeLoader.construct_yaml_omap)
ESPHomeLoader.add_constructor(u'tag:yaml.org,2002:str', ESPHomeLoader.construct_yaml_str)
ESPHomeLoader.add_constructor(u'tag:yaml.org,2002:seq', ESPHomeLoader.construct_yaml_seq)
ESPHomeLoader.add_constructor(u'tag:yaml.org,2002:map', ESPHomeLoader.construct_yaml_map)
ESPHomeLoader.add_constructor('!env_var', ESPHomeLoader.construct_env_var)
ESPHomeLoader.add_constructor('!secret', ESPHomeLoader.construct_secret)
ESPHomeLoader.add_constructor('!include', ESPHomeLoader.construct_include)
ESPHomeLoader.add_constructor('!include_dir_list', ESPHomeLoader.construct_include_dir_list)
ESPHomeLoader.add_constructor('!include_dir_merge_list',
                              ESPHomeLoader.construct_include_dir_merge_list)
ESPHomeLoader.add_constructor('!include_dir_named', ESPHomeLoader.construct_include_dir_named)
ESPHomeLoader.add_constructor('!include_dir_merge_named',
                              ESPHomeLoader.construct_include_dir_merge_named)
ESPHomeLoader.add_constructor('!lambda', ESPHomeLoader.construct_lambda)


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
        raise EsphomeError(exc)
    finally:
        loader.dispose()


def dump(dict_):
    """Dump YAML to a string and remove null."""
    return yaml.dump(dict_, default_flow_style=False, allow_unicode=True,
                     Dumper=ESPHomeDumper)


def _is_file_valid(name):
    """Decide if a file is valid."""
    return not name.startswith(u'.')


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
        return _SECRET_VALUES[text_type(value)]
    except (KeyError, ValueError):
        return None


class ESPHomeDumper(yaml.SafeDumper):  # pylint: disable=too-many-ancestors
    def represent_mapping(self, tag, mapping, flow_style=None):
        value = []
        node = yaml.MappingNode(tag, value, flow_style=flow_style)
        if self.alias_key is not None:
            self.represented_objects[self.alias_key] = node
        best_style = True
        if hasattr(mapping, 'items'):
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
        return self.represent_scalar(tag=u'!secret', value=_SECRET_VALUES[text_type(value)])

    def represent_stringify(self, value):
        if is_secret(value):
            return self.represent_secret(value)
        return self.represent_scalar(tag=u'tag:yaml.org,2002:str', value=text_type(value))

    # pylint: disable=arguments-differ
    def represent_bool(self, value):
        return self.represent_scalar(u'tag:yaml.org,2002:bool', u'true' if value else u'false')

    def represent_int(self, value):
        if is_secret(value):
            return self.represent_secret(value)
        return self.represent_scalar(tag=u'tag:yaml.org,2002:int', value=text_type(value))

    def represent_float(self, value):
        if is_secret(value):
            return self.represent_secret(value)
        if math.isnan(value):
            value = u'.nan'
        elif math.isinf(value):
            value = u'.inf' if value > 0 else u'-.inf'
        else:
            value = text_type(repr(value)).lower()
            # Note that in some cases `repr(data)` represents a float number
            # without the decimal parts.  For instance:
            #   >>> repr(1e17)
            #   '1e17'
            # Unfortunately, this is not a valid float representation according
            # to the definition of the `!!float` tag.  We fix this by adding
            # '.0' before the 'e' symbol.
            if u'.' not in value and u'e' in value:
                value = value.replace(u'e', u'.0e', 1)
        return self.represent_scalar(tag=u'tag:yaml.org,2002:float', value=value)

    def represent_lambda(self, value):
        if is_secret(value.value):
            return self.represent_secret(value.value)
        return self.represent_scalar(tag='!lambda', value=value.value, style='|')

    def represent_id(self, value):
        if is_secret(value.id):
            return self.represent_secret(value.id)
        return self.represent_stringify(value.id)


ESPHomeDumper.add_multi_representer(
    dict,
    lambda dumper, value: dumper.represent_mapping('tag:yaml.org,2002:map', value)
)
ESPHomeDumper.add_multi_representer(
    list,
    lambda dumper, value: dumper.represent_sequence('tag:yaml.org,2002:seq', value)
)
ESPHomeDumper.add_multi_representer(bool, ESPHomeDumper.represent_bool)
ESPHomeDumper.add_multi_representer(str, ESPHomeDumper.represent_stringify)
ESPHomeDumper.add_multi_representer(int, ESPHomeDumper.represent_int)
ESPHomeDumper.add_multi_representer(float, ESPHomeDumper.represent_float)
if IS_PY2:
    ESPHomeDumper.add_multi_representer(unicode, ESPHomeDumper.represent_stringify)
    ESPHomeDumper.add_multi_representer(long, ESPHomeDumper.represent_int)
ESPHomeDumper.add_multi_representer(IPAddress, ESPHomeDumper.represent_stringify)
ESPHomeDumper.add_multi_representer(MACAddress, ESPHomeDumper.represent_stringify)
ESPHomeDumper.add_multi_representer(TimePeriod, ESPHomeDumper.represent_stringify)
ESPHomeDumper.add_multi_representer(Lambda, ESPHomeDumper.represent_lambda)
ESPHomeDumper.add_multi_representer(core.ID, ESPHomeDumper.represent_id)
ESPHomeDumper.add_multi_representer(uuid.UUID, ESPHomeDumper.represent_stringify)
