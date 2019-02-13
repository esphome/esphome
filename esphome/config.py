from __future__ import print_function

from collections import OrderedDict
import importlib
import json
import logging
import re

import voluptuous as vol

from esphome import core, core_config, yaml_util
from esphome.components import substitutions
from esphome.const import CONF_ESPHOME, CONF_PLATFORM, ESP_PLATFORMS
from esphome.core import CORE, EsphomeError
from esphome.helpers import color, indent
from esphome.py_compat import text_type
from esphome.util import safe_print

# pylint: disable=unused-import, wrong-import-order
from typing import List, Optional, Tuple, Union  # noqa
from esphome.core import ConfigType  # noqa

_LOGGER = logging.getLogger(__name__)

_COMPONENT_CACHE = {}


def get_component(domain):
    if domain in _COMPONENT_CACHE:
        return _COMPONENT_CACHE[domain]

    path = 'esphome.components.{}'.format(domain)
    try:
        module = importlib.import_module(path)
    except (ImportError, ValueError) as err:
        _LOGGER.debug(err)
    else:
        _COMPONENT_CACHE[domain] = module
        return module

    _LOGGER.error("Unable to find component %s", domain)
    return None


def get_platform(domain, platform):
    return get_component("{}.{}".format(domain, platform))


def is_platform_component(component):
    return hasattr(component, 'PLATFORM_SCHEMA')


def iter_components(config):
    for domain, conf in config.items():
        if domain == CONF_ESPHOME:
            yield CONF_ESPHOME, core_config, conf
            continue
        component = get_component(domain)
        if getattr(component, 'MULTI_CONF', False):
            for conf_ in conf:
                yield domain, component, conf_
        else:
            yield domain, component, conf
        if is_platform_component(component):
            for p_config in conf:
                p_name = u"{}.{}".format(domain, p_config[CONF_PLATFORM])
                platform = get_component(p_name)
                yield p_name, platform, p_config


ConfigPath = List[Union[str, int]]


def _path_begins_with_(path, other):  # type: (ConfigPath, ConfigPath) -> bool
    if len(path) < len(other):
        return False
    return path[:len(other)] == other


def _path_begins_with(path, other):  # type: (ConfigPath, ConfigPath) -> bool
    ret = _path_begins_with_(path, other)
    return ret


class Config(OrderedDict):
    def __init__(self):
        super(Config, self).__init__()
        self.errors = []  # type: List[Tuple[basestring, ConfigPath]]
        self.domains = []  # type: List[Tuple[ConfigPath, basestring]]

    def add_error(self, message, path):
        # type: (basestring, ConfigPath) -> None
        if not isinstance(message, text_type):
            message = text_type(message)
        self.errors.append((message, path))

    def add_domain(self, path, name):
        # type: (ConfigPath, basestring) -> None
        self.domains.append((path, name))

    def remove_domain(self, path, name):
        self.domains.remove((path, name))

    def lookup_domain(self, path):
        # type: (ConfigPath) -> Optional[basestring]
        best_len = 0
        best_domain = None
        for d_path, domain in self.domains:
            if len(d_path) < best_len:
                continue
            if _path_begins_with(path, d_path):
                best_len = len(d_path)
                best_domain = domain
        return best_domain

    def is_in_error_path(self, path):
        for _, p in self.errors:
            if _path_begins_with(p, path):
                return True
        return False

    def get_error_for_path(self, path):
        for msg, p in self.errors:
            if self.nested_item_path(p) == path:
                return msg
        return None

    def nested_item(self, path):
        data = self
        for item_index in path:
            try:
                data = data[item_index]
            except (KeyError, IndexError, TypeError):
                return {}
        return data

    def nested_item_path(self, path):
        data = self
        part = []
        for item_index in path:
            try:
                data = data[item_index]
            except (KeyError, IndexError, TypeError):
                return part
            part.append(item_index)
        return part


def iter_ids(config, path=None):
    path = path or []
    if isinstance(config, core.ID):
        yield config, path
    elif isinstance(config, core.Lambda):
        for id in config.requires_ids:
            yield id, path
    elif isinstance(config, list):
        for i, item in enumerate(config):
            for result in iter_ids(item, path + [i]):
                yield result
    elif isinstance(config, dict):
        for key, value in config.items():
            for result in iter_ids(value, path + [key]):
                yield result


def do_id_pass(result):  # type: (Config) -> None
    from esphome.cpp_generator import MockObjClass

    declare_ids = []  # type: List[Tuple[core.ID, ConfigPath]]
    searching_ids = []  # type: List[Tuple[core.ID, ConfigPath]]
    for id, path in iter_ids(result):
        if id.is_declaration:
            if id.id is not None and any(v[0].id == id.id for v in declare_ids):
                result.add_error(u"ID {} redefined!".format(id.id), path)
                continue
            declare_ids.append((id, path))
        else:
            searching_ids.append((id, path))
    # Resolve default ids after manual IDs
    for id, _ in declare_ids:
        id.resolve([v[0].id for v in declare_ids])

    # Check searched IDs
    for id, path in searching_ids:
        if id.id is not None:
            # manually declared
            match = next((v[0] for v in declare_ids if v[0].id == id.id), None)
            if match is None:
                # No declared ID with this name
                result.add_error("Couldn't find ID '{}'".format(id.id), path)
                continue
            if not isinstance(match.type, MockObjClass) or not isinstance(id.type, MockObjClass):
                continue
            if not match.type.inherits_from(id.type):
                result.add_error("ID '{}' of type {} doesn't inherit from {}. Please double check "
                                 "your ID is pointing to the correct value"
                                 "".format(id.id, match.type, id.type), path)

        if id.id is None and id.type is not None:
            for v in declare_ids:
                if v[0] is None or not isinstance(v[0].type, MockObjClass):
                    continue
                inherits = v[0].type.inherits_from(id.type)
                if inherits:
                    id.id = v[0].id
                    break
            else:
                result.add_error("Couldn't resolve ID for type '{}'".format(id.type), path)


def validate_config(config):
    result = Config()

    def _comp_error(ex, path):
        # type: (vol.Invalid, List[basestring]) -> None
        if isinstance(ex, vol.MultipleInvalid):
            errors = ex.errors
        else:
            errors = [ex]

        for e in errors:
            path_ = path + e.path
            domain = result.lookup_domain(path_) or ''
            result.add_error(_format_vol_invalid(e, config, path, domain), path_)

    skip_paths = list()  # type: List[ConfigPath]

    # Step 1: Load everything
    result.add_domain([CONF_ESPHOME], CONF_ESPHOME)
    result[CONF_ESPHOME] = config[CONF_ESPHOME]

    for domain, conf in config.items():
        domain = str(domain)
        if domain == CONF_ESPHOME or domain.startswith(u'.'):
            skip_paths.append([domain])
            continue
        result.add_domain([domain], domain)
        result[domain] = conf
        if conf is None:
            result[domain] = conf = {}
        component = get_component(domain)
        if component is None:
            result.add_error(u"Component not found: {}".format(domain), [domain])
            skip_paths.append([domain])
            continue

        if not isinstance(conf, list) and getattr(component, 'MULTI_CONF', False):
            result[domain] = conf = [conf]

        success = True
        dependencies = getattr(component, 'DEPENDENCIES', [])
        for dependency in dependencies:
            if dependency not in config:
                result.add_error(u"Component {} requires component {}".format(domain, dependency),
                                 [domain])
                success = False
        if not success:
            skip_paths.append([domain])
            continue

        success = True
        conflicts_with = getattr(component, 'CONFLICTS_WITH', [])
        for conflict in conflicts_with:
            if conflict in config:
                result.add_error(u"Component {} cannot be used together with component {}"
                                 u"".format(domain, conflict), [domain])
                success = False
        if not success:
            skip_paths.append([domain])
            continue

        esp_platforms = getattr(component, 'ESP_PLATFORMS', ESP_PLATFORMS)
        if CORE.esp_platform not in esp_platforms:
            result.add_error(u"Component {} doesn't support {}.".format(domain, CORE.esp_platform),
                             [domain])
            skip_paths.append([domain])
            continue

        if not hasattr(component, 'PLATFORM_SCHEMA'):
            continue

        result.remove_domain([domain], domain)

        if not isinstance(conf, list) and conf:
            result[domain] = conf = [conf]

        for i, p_config in enumerate(conf):
            if not isinstance(p_config, dict):
                result.add_error(u"Platform schemas must have 'platform:' key", [domain, i])
                skip_paths.append([domain, i])
                continue
            p_name = p_config.get('platform')
            if p_name is None:
                result.add_error(u"No platform specified for {}".format(domain), [domain, i])
                skip_paths.append([domain, i])
                continue
            p_domain = u'{}.{}'.format(domain, p_name)
            result.add_domain([domain, i], p_domain)
            platform = get_platform(domain, p_name)
            if platform is None:
                result.add_error(u"Platform not found: '{}'".format(p_domain), [domain, i])
                skip_paths.append([domain, i])
                continue

            success = True
            dependencies = getattr(platform, 'DEPENDENCIES', [])
            for dependency in dependencies:
                if dependency not in config:
                    result.add_error(u"Platform {} requires component {}"
                                     u"".format(p_domain, dependency), [domain, i])
                    success = False
            if not success:
                skip_paths.append([domain, i])
                continue

            success = True
            conflicts_with = getattr(platform, 'CONFLICTS_WITH', [])
            for conflict in conflicts_with:
                if conflict in config:
                    result.add_error(u"Platform {} cannot be used together with component {}"
                                     u"".format(p_domain, conflict), [domain, i])
                    success = False
            if not success:
                skip_paths.append([domain, i])
                continue

            esp_platforms = getattr(platform, 'ESP_PLATFORMS', ESP_PLATFORMS)
            if CORE.esp_platform not in esp_platforms:
                result.add_error(u"Platform {} doesn't support {}."
                                 u"".format(p_domain, CORE.esp_platform), [domain, i])
                skip_paths.append([domain, i])
                continue

    # Step 2: Validate configuration
    try:
        result[CONF_ESPHOME] = core_config.CONFIG_SCHEMA(result[CONF_ESPHOME])
    except vol.Invalid as ex:
        _comp_error(ex, [CONF_ESPHOME])

    for domain, conf in result.items():
        domain = str(domain)
        if [domain] in skip_paths:
            continue
        component = get_component(domain)

        if hasattr(component, 'CONFIG_SCHEMA'):
            multi_conf = getattr(component, 'MULTI_CONF', False)

            if multi_conf:
                for i, conf_ in enumerate(conf):
                    try:
                        validated = component.CONFIG_SCHEMA(conf_)
                        result[domain][i] = validated
                    except vol.Invalid as ex:
                        _comp_error(ex, [domain, i])
            else:
                try:
                    validated = component.CONFIG_SCHEMA(conf)
                    result[domain] = validated
                except vol.Invalid as ex:
                    _comp_error(ex, [domain])
                    continue

        if not hasattr(component, 'PLATFORM_SCHEMA'):
            continue

        for i, p_config in enumerate(conf):
            if [domain, i] in skip_paths:
                continue
            p_name = p_config['platform']
            platform = get_platform(domain, p_name)

            if hasattr(platform, 'PLATFORM_SCHEMA'):
                try:
                    p_validated = platform.PLATFORM_SCHEMA(p_config)
                except vol.Invalid as ex:
                    _comp_error(ex, [domain, i])
                    continue
                result[domain][i] = p_validated

    if not result.errors:
        # Only parse IDs if no validation error. Otherwise
        # user gets confusing messages
        do_id_pass(result)
    return result


def _nested_getitem(data, path):
    for item_index in path:
        try:
            data = data[item_index]
        except (KeyError, IndexError, TypeError):
            return None
    return data


def humanize_error(config, validation_error):
    offending_item_summary = _nested_getitem(config, validation_error.path)
    if isinstance(offending_item_summary, dict):
        try:
            offending_item_summary = json.dumps(offending_item_summary)
        except (TypeError, ValueError):
            pass
    validation_error = text_type(validation_error)
    m = re.match(r'^(.*?)\s*(?:for dictionary value )?@ data\[.*$', validation_error)
    if m is not None:
        validation_error = m.group(1)
    validation_error = validation_error.strip()
    if not validation_error.endswith(u'.'):
        validation_error += u'.'
    if offending_item_summary is None:
        return validation_error
    return u"{} Got '{}'".format(validation_error, offending_item_summary)


def _format_vol_invalid(ex, config, path, domain):
    # type: (vol.Invalid, ConfigType, ConfigPath, basestring) -> unicode
    message = u''
    if u'extra keys not allowed' in ex.error_message:
        try:
            paren = ex.path[-2]
        except IndexError:
            paren = domain
        message += u'[{}] is an invalid option for [{}].'.format(ex.path[-1], paren)
    elif u'required key not provided' in ex.error_message:
        try:
            paren = ex.path[-2]
        except IndexError:
            paren = domain
        message += u"'{}' is a required option for [{}].".format(ex.path[-1], paren)
    else:
        message += humanize_error(_nested_getitem(config, path), ex)

    return message


def load_config():
    try:
        config = yaml_util.load_yaml(CORE.config_path)
    except OSError:
        raise EsphomeError(u"Invalid YAML at {}".format(CORE.config_path))
    CORE.raw_config = config
    config = substitutions.do_substitution_pass(config)
    core_config.preload_core_config(config)

    try:
        result = validate_config(config)
    except EsphomeError:
        raise
    except Exception:
        _LOGGER.error(u"Unexpected exception while reading configuration:")
        raise

    return result


def line_info(obj, highlight=True):
    """Display line config source."""
    if not highlight:
        return None
    if hasattr(obj, '__config_file__'):
        return color('cyan', "[source {}:{}]"
                     .format(obj.__config_file__, obj.__line__ or '?'))
    return None


def _print_on_next_line(obj):
    if isinstance(obj, (list, tuple, dict)):
        return True
    if isinstance(obj, str):
        return len(obj) > 80
    if isinstance(obj, core.Lambda):
        return len(obj.value) > 80
    return False


def dump_dict(config, path, at_root=True):
    # type: (Config, ConfigPath, bool) -> Tuple[unicode, bool]
    conf = config.nested_item(path)
    ret = u''
    multiline = False

    if at_root:
        error = config.get_error_for_path(path)
        if error is not None:
            ret += u'\n' + color('bold_red', error) + u'\n'

    if isinstance(conf, (list, tuple)):
        multiline = True
        if not conf:
            ret += u'[]'
            multiline = False

        for i in range(len(conf)):
            path_ = path + [i]
            error = config.get_error_for_path(path_)
            if error is not None:
                ret += u'\n' + color('bold_red', error) + u'\n'

            sep = u'- '
            if config.is_in_error_path(path_):
                sep = color('red', sep)
            msg, _ = dump_dict(config, path_, at_root=False)
            msg = indent(msg)
            inf = line_info(config.nested_item(path_), highlight=config.is_in_error_path(path_))
            if inf is not None:
                msg = inf + u'\n' + msg
            elif msg:
                msg = msg[2:]
            ret += sep + msg + u'\n'
    elif isinstance(conf, dict):
        multiline = True
        if not conf:
            ret += u'{}'
            multiline = False

        for k in conf.keys():
            path_ = path + [k]
            error = config.get_error_for_path(path_)
            if error is not None:
                ret += u'\n' + color('bold_red', error) + u'\n'

            st = u'{}: '.format(k)
            if config.is_in_error_path(path_):
                st = color('red', st)
            msg, m = dump_dict(config, path_, at_root=False)

            inf = line_info(config.nested_item(path_), highlight=config.is_in_error_path(path_))
            if m:
                msg = u'\n' + indent(msg)

            if inf is not None:
                if m:
                    msg = u' ' + inf + msg
                else:
                    msg = msg + u' ' + inf
            ret += st + msg + u'\n'
    elif isinstance(conf, str):
        if not conf:
            conf += u"''"

        if len(conf) > 80:
            conf = u'|-\n' + indent(conf)
        error = config.get_error_for_path(path)
        col = 'bold_red' if error else 'white'
        ret += color(col, text_type(conf))
    elif isinstance(conf, core.Lambda):
        conf = u'!lambda |-\n' + indent(text_type(conf.value))
        error = config.get_error_for_path(path)
        col = 'bold_red' if error else 'white'
        ret += color(col, conf)
    elif conf is None:
        pass
    else:
        error = config.get_error_for_path(path)
        col = 'bold_red' if error else 'white'
        ret += color(col, text_type(conf))
        multiline = u'\n' in ret

    return ret, multiline


def strip_default_ids(config):
    if isinstance(config, list):
        to_remove = []
        for i, x in enumerate(config):
            x = config[i] = strip_default_ids(x)
            if isinstance(x, core.ID) and not x.is_manual:
                to_remove.append(x)
        for x in to_remove:
            config.remove(x)
    elif isinstance(config, dict):
        to_remove = []
        for k, v in config.items():
            v = config[k] = strip_default_ids(v)
            if isinstance(v, core.ID) and not v.is_manual:
                to_remove.append(k)
        for k in to_remove:
            config.pop(k)
    return config


def read_config(verbose):
    _LOGGER.info("Reading configuration...")
    try:
        res = load_config()
    except EsphomeError as err:
        _LOGGER.error(u"Error while reading config: %s", err)
        return None
    if res.errors:
        if not verbose:
            res = strip_default_ids(res)

        safe_print(color('bold_red', u"Failed config"))
        safe_print('')
        for path, domain in res.domains:
            if not res.is_in_error_path(path):
                continue

            safe_print(color('bold_red', u'{}:'.format(domain)) + u' ' +
                       (line_info(res.nested_item(path)) or u''))
            safe_print(indent(dump_dict(res, path)[0]))
        return None
    return OrderedDict(res)
