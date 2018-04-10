from __future__ import print_function
import codecs
import logging
from collections import OrderedDict

import yaml

from esphomeyaml.core import ESPHomeYAMLError, HexInt, IPAddress

_LOGGER = logging.getLogger(__name__)


class NodeListClass(list):
    """Wrapper class to be able to add attributes on a list."""

    pass


class NodeStrClass(unicode):
    """Wrapper class to be able to add attributes on a string."""

    pass


class SafeLineLoader(yaml.SafeLoader):
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
            _LOGGER.error(
                u'YAML file %s contains duplicate key "%s". '
                u'Check lines %d and %d.', fname, key, seen[key], line)
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


def _include(self, node):
        """Include file"""
        filename = self.construct_scalar(node)
        with open(filename, 'r') as f:
            return yaml.load(f, yaml.SafeLoader)


yaml.SafeLoader.add_constructor(yaml.resolver.BaseResolver.DEFAULT_MAPPING_TAG, _ordered_dict)
yaml.SafeLoader.add_constructor(yaml.resolver.BaseResolver.DEFAULT_SEQUENCE_TAG, _construct_seq)
yaml.SafeLoader.add_constructor('!include', _include)


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


def unicode_representer(dumper, uni):
    node = yaml.ScalarNode(tag=u'tag:yaml.org,2002:str', value=uni)
    return node


def hex_int_representer(dumper, data):
    node = yaml.ScalarNode(tag=u'tag:yaml.org,2002:int', value=str(data))
    return node


def ipaddress_representer(dumper, data):
    node = yaml.ScalarNode(tag=u'tag:yaml.org,2002:str', value=str(data))
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
yaml.SafeDumper.add_representer(IPAddress, ipaddress_representer)
