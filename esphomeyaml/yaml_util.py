from __future__ import print_function

import codecs
import fnmatch
import logging
import os
from collections import OrderedDict

import yaml

from esphomeyaml.core import ESPHomeYAMLError, HexInt, IPAddress, Lambda, MACAddress, TimePeriod

_LOGGER = logging.getLogger(__name__)

# Mostly copied from Home Assistant because that code works fine and
# let's not reinvent the wheel here

SECRET_YAML = u'secrets.yaml'


class NodeListClass(list):
    """Wrapper class to be able to add attributes on a list."""

    pass


class NodeStrClass(unicode):
    """Wrapper class to be able to add attributes on a string."""

    pass


class SafeLineLoader(yaml.SafeLoader):  # pylint: disable=too-many-ancestors
    """Loader class that keeps track of line numbers."""

    def compose_node(self, parent, index):
        """Annotate a node with the first line it was seen."""
        last_line = self.line  # type: int
        node = super(SafeLineLoader, self).compose_node(parent, index)  # type: yaml.nodes.Node
        node.__line__ = last_line + 1
        return node


def load_yaml(fname):
    """Load a YAML file."""
    try:
        with codecs.open(fname, encoding='utf-8') as conf_file:
            return yaml.load(conf_file, Loader=SafeLineLoader) or OrderedDict()
    except yaml.YAMLError as exc:
        _LOGGER.error(exc)
        raise ESPHomeYAMLError(exc)
    except UnicodeDecodeError as exc:
        _LOGGER.error(u"Unable to read file %s: %s", fname, exc)
        raise ESPHomeYAMLError(exc)


def dump(dict_):
    """Dump YAML to a string and remove null."""
    return yaml.safe_dump(
        dict_, default_flow_style=False, allow_unicode=True)


def _ordered_dict(loader, node):
    """Load YAML mappings into an ordered dictionary to preserve key order."""
    loader.flatten_mapping(node)
    nodes = loader.construct_pairs(node)

    seen = {}
    for (key, _), (child_node, _) in zip(nodes, node.value):
        line = child_node.start_mark.line

        try:
            hash(key)
        except TypeError:
            fname = getattr(loader.stream, 'name', '')
            raise yaml.MarkedYAMLError(
                context="invalid key: \"{}\"".format(key),
                context_mark=yaml.Mark(fname, 0, line, -1, None, None)
            )

        if key in seen:
            fname = getattr(loader.stream, 'name', '')
            raise ESPHomeYAMLError(u'YAML file {} contains duplicate key "{}". '
                                   u'Check lines {} and {}.'.format(fname, key, seen[key], line))
        seen[key] = line

    return _add_reference(OrderedDict(nodes), loader, node)


def _construct_seq(loader, node):
    """Add line number and file name to Load YAML sequence."""
    obj, = loader.construct_yaml_seq(node)
    return _add_reference(obj, loader, node)


def _add_reference(obj, loader, node):
    """Add file reference information to an object."""
    if isinstance(obj, (str, unicode)):
        obj = NodeStrClass(obj)
    if isinstance(obj, list):
        return obj
    setattr(obj, '__config_file__', loader.name)
    setattr(obj, '__line__', node.start_mark.line)
    return obj


def _env_var_yaml(_, node):
    """Load environment variables and embed it into the configuration YAML."""
    args = node.value.split()

    # Check for a default value
    if len(args) > 1:
        return os.getenv(args[0], u' '.join(args[1:]))
    elif args[0] in os.environ:
        return os.environ[args[0]]
    raise ESPHomeYAMLError(u"Environment variable {} not defined.".format(node.value))


def _include_yaml(loader, node):
    """Load another YAML file and embeds it using the !include tag.

    Example:
        device_tracker: !include device_tracker.yaml
    """
    fname = os.path.join(os.path.dirname(loader.name), node.value)
    return _add_reference(load_yaml(fname), loader, node)


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


def _include_dir_named_yaml(loader, node):
    """Load multiple files from directory as a dictionary."""
    mapping = OrderedDict()  # type: OrderedDict
    loc = os.path.join(os.path.dirname(loader.name), node.value)
    for fname in _find_files(loc, '*.yaml'):
        filename = os.path.splitext(os.path.basename(fname))[0]
        mapping[filename] = load_yaml(fname)
    return _add_reference(mapping, loader, node)


def _include_dir_merge_named_yaml(loader, node):
    """Load multiple files from directory as a merged dictionary."""
    mapping = OrderedDict()  # type: OrderedDict
    loc = os.path.join(os.path.dirname(loader.name), node.value)
    for fname in _find_files(loc, '*.yaml'):
        if os.path.basename(fname) == SECRET_YAML:
            continue
        loaded_yaml = load_yaml(fname)
        if isinstance(loaded_yaml, dict):
            mapping.update(loaded_yaml)
    return _add_reference(mapping, loader, node)


def _include_dir_list_yaml(loader, node):
    """Load multiple files from directory as a list."""
    loc = os.path.join(os.path.dirname(loader.name), node.value)
    return [load_yaml(f) for f in _find_files(loc, '*.yaml')
            if os.path.basename(f) != SECRET_YAML]


def _include_dir_merge_list_yaml(loader, node):
    """Load multiple files from directory as a merged list."""
    path = os.path.join(os.path.dirname(loader.name), node.value)
    merged_list = []
    for fname in _find_files(path, '*.yaml'):
        if os.path.basename(fname) == SECRET_YAML:
            continue
        loaded_yaml = load_yaml(fname)
        if isinstance(loaded_yaml, list):
            merged_list.extend(loaded_yaml)
    return _add_reference(merged_list, loader, node)


# pylint: disable=protected-access
def _secret_yaml(loader, node):
    """Load secrets and embed it into the configuration YAML."""
    secret_path = os.path.join(os.path.dirname(loader.name), SECRET_YAML)
    secrets = load_yaml(secret_path)
    if node.value not in secrets:
        raise ESPHomeYAMLError(u"Secret {} not defined".format(node.value))
    return secrets[node.value]


def _lambda(loader, node):
    return Lambda(unicode(node.value))


yaml.SafeLoader.add_constructor(yaml.resolver.BaseResolver.DEFAULT_MAPPING_TAG, _ordered_dict)
yaml.SafeLoader.add_constructor(yaml.resolver.BaseResolver.DEFAULT_SEQUENCE_TAG, _construct_seq)
yaml.SafeLoader.add_constructor('!env_var', _env_var_yaml)
yaml.SafeLoader.add_constructor('!secret', _secret_yaml)
yaml.SafeLoader.add_constructor('!include', _include_yaml)
yaml.SafeLoader.add_constructor('!include_dir_list', _include_dir_list_yaml)
yaml.SafeLoader.add_constructor('!include_dir_merge_list',
                                _include_dir_merge_list_yaml)
yaml.SafeLoader.add_constructor('!include_dir_named', _include_dir_named_yaml)
yaml.SafeLoader.add_constructor('!include_dir_merge_named',
                                _include_dir_merge_named_yaml)
yaml.SafeLoader.add_constructor('!lambda', _lambda)


# From: https://gist.github.com/miracle2k/3184458
# pylint: disable=redefined-outer-name
def represent_odict(dump, tag, mapping, flow_style=None):
    """Like BaseRepresenter.represent_mapping but does not issue the sort()."""
    value = []
    node = yaml.MappingNode(tag, value, flow_style=flow_style)
    if dump.alias_key is not None:
        dump.represented_objects[dump.alias_key] = node
    best_style = True
    if hasattr(mapping, 'items'):
        mapping = mapping.items()
    for item_key, item_value in mapping:
        node_key = dump.represent_data(item_key)
        node_value = dump.represent_data(item_value)
        if not (isinstance(node_key, yaml.ScalarNode) and not node_key.style):
            best_style = False
        if not (isinstance(node_value, yaml.ScalarNode) and
                not node_value.style):
            best_style = False
        value.append((node_key, node_value))
    if flow_style is None:
        if dump.default_flow_style is not None:
            node.flow_style = dump.default_flow_style
        else:
            node.flow_style = best_style
    return node


def unicode_representer(_, uni):
    node = yaml.ScalarNode(tag=u'tag:yaml.org,2002:str', value=uni)
    return node


def hex_int_representer(_, data):
    node = yaml.ScalarNode(tag=u'tag:yaml.org,2002:int', value=str(data))
    return node


def stringify_representer(_, data):
    node = yaml.ScalarNode(tag=u'tag:yaml.org,2002:str', value=str(data))
    return node


TIME_PERIOD_UNIT_MAP = {
    'microseconds': 'us',
    'milliseconds': 'ms',
    'seconds': 's',
    'minutes': 'min',
    'hours': 'h',
    'days': 'd',
}


def represent_time_period(dumper, data):
    dictionary = data.as_dict()
    if len(dictionary) == 1:
        unit, value = dictionary.popitem()
        out = '{}{}'.format(value, TIME_PERIOD_UNIT_MAP[unit])
        return yaml.ScalarNode(tag=u'tag:yaml.org,2002:str', value=out)
    return represent_odict(dumper, 'tag:yaml.org,2002:map', dictionary)


def represent_lambda(_, data):
    node = yaml.ScalarNode(tag='!lambda', value=data.value, style='>')
    return node


yaml.SafeDumper.add_representer(
    OrderedDict,
    lambda dumper, value:
    represent_odict(dumper, 'tag:yaml.org,2002:map', value)
)

yaml.SafeDumper.add_representer(
    NodeListClass,
    lambda dumper, value:
    dumper.represent_sequence(dumper, 'tag:yaml.org,2002:map', value)
)

yaml.SafeDumper.add_representer(unicode, unicode_representer)
yaml.SafeDumper.add_representer(HexInt, hex_int_representer)
yaml.SafeDumper.add_representer(IPAddress, stringify_representer)
yaml.SafeDumper.add_representer(MACAddress, stringify_representer)
yaml.SafeDumper.add_multi_representer(TimePeriod, represent_time_period)
yaml.SafeDumper.add_multi_representer(Lambda, represent_lambda)
