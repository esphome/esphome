from __future__ import print_function

import importlib
import logging
from collections import OrderedDict

import voluptuous as vol
from voluptuous.humanize import humanize_error

from esphomeyaml import core, yaml_util, core_config
from esphomeyaml.const import CONF_ESPHOMEYAML, CONF_PLATFORM, CONF_WIFI, ESP_PLATFORMS
from esphomeyaml.core import ESPHomeYAMLError
from esphomeyaml.helpers import color

_LOGGER = logging.getLogger(__name__)

REQUIRED_COMPONENTS = [
    CONF_ESPHOMEYAML, CONF_WIFI
]
_COMPONENT_CACHE = {}
_ALL_COMPONENTS = []


def get_component(domain):
    if domain in _COMPONENT_CACHE:
        return _COMPONENT_CACHE[domain]

    path = 'esphomeyaml.components.{}'.format(domain)
    try:
        module = importlib.import_module(path)
    except ImportError as err:
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
    for domain, conf in config.iteritems():
        if domain == CONF_ESPHOMEYAML:
            continue
        component = get_component(domain)
        yield domain, component, conf
        if is_platform_component(component):
            for p_config in conf:
                p_name = u"{}.{}".format(domain, p_config[CONF_PLATFORM])
                platform = get_component(p_name)
                yield p_name, platform, p_config


class Config(OrderedDict):
    def __init__(self):
        super(Config, self).__init__()
        self.errors = []

    def add_error(self, message, domain=None, config=None):
        if not isinstance(message, unicode):
            message = unicode(message)
        self.errors.append((message, domain, config))


def iter_ids(config, prefix=None, parent=None):
    prefix = prefix or []
    parent = parent or {}
    if isinstance(config, core.ID):
        yield config, prefix, parent
    elif isinstance(config, core.Lambda):
        for id in config.requires_ids:
            yield id, prefix, parent
    elif isinstance(config, list):
        for i, item in enumerate(config):
            for result in iter_ids(item, prefix + [str(i)], config):
                yield result
    elif isinstance(config, dict):
        for key, value in config.iteritems():
            for result in iter_ids(value, prefix + [str(key)], config):
                yield result


def do_id_pass(result):
    declare_ids = []
    searching_ids = []
    for id, prefix, config in iter_ids(result):
        if id.is_declaration:
            if id.id is not None and any(v[0].id == id.id for v in declare_ids):
                result.add_error("ID {} redefined!".format(id.id), '.'.join(prefix), config)
                continue
            declare_ids.append((id, prefix, config))
        else:
            searching_ids.append((id, prefix, config))
    # Resolve default ids after manual IDs
    for id, _, _ in declare_ids:
        id.resolve([v[0].id for v in declare_ids])

    # Check searched IDs
    for id, prefix, config in searching_ids:
        if id.id is not None and not any(v[0].id == id.id for v in declare_ids):
            result.add_error("Couldn't find ID {}".format(id.id), '.'.join(prefix), config)
        if id.id is None and id.type is not None:
            id.id = next((v[0].id for v in declare_ids if v[0].type == id.type), None)
            if id.id is None:
                result.add_error("Couldn't resolve ID for type {}".format(id.type),
                                 '.'.join(prefix), config)


def validate_config(config):
    global _ALL_COMPONENTS

    for req in REQUIRED_COMPONENTS:
        if req not in config:
            raise ESPHomeYAMLError("Component {} is required for esphomeyaml.".format(req))

    _ALL_COMPONENTS = list(config.keys())

    result = Config()

    def _comp_error(ex, domain, config):
        result.add_error(_format_config_error(ex, domain, config), domain, config)

    try:
        result[CONF_ESPHOMEYAML] = core_config.CONFIG_SCHEMA(config[CONF_ESPHOMEYAML])
    except vol.Invalid as ex:
        _comp_error(ex, CONF_ESPHOMEYAML, config[CONF_ESPHOMEYAML])

    for domain, conf in config.iteritems():
        domain = str(domain)
        if domain == CONF_ESPHOMEYAML or domain.startswith('.'):
            continue
        if conf is None:
            conf = {}
        component = get_component(domain)
        if component is None:
            result.add_error(u"Component not found: {}".format(domain), domain, conf)
            continue

        esp_platforms = getattr(component, 'ESP_PLATFORMS', ESP_PLATFORMS)
        if core.ESP_PLATFORM not in esp_platforms:
            result.add_error(u"Component {} doesn't support {}.".format(domain, core.ESP_PLATFORM),
                             domain, conf)
            continue

        success = True
        dependencies = getattr(component, 'DEPENDENCIES', [])
        for dependency in dependencies:
            if dependency not in _ALL_COMPONENTS:
                result.add_error(u"Component {} requires component {}".format(domain, dependency),
                                 domain, conf)
                success = False
        if not success:
            continue

        if hasattr(component, 'CONFIG_SCHEMA'):
            try:
                validated = component.CONFIG_SCHEMA(conf)
                result[domain] = validated
            except vol.Invalid as ex:
                _comp_error(ex, domain, conf)
                continue

        if not hasattr(component, 'PLATFORM_SCHEMA'):
            continue

        platforms = []
        for p_config in conf:
            if not isinstance(p_config, dict):
                result.add_error(u"Platform schemas must have 'platform:' key", )
                continue
            p_name = p_config.get(u'platform')
            if p_name is None:
                result.add_error(u"No platform specified for {}".format(domain))
                continue
            p_domain = u'{}.{}'.format(domain, p_name)
            platform = get_platform(domain, p_name)
            if platform is None:
                result.add_error(u"Platform not found: {}".format(p_domain), p_domain, p_config)
                continue

            success = True
            dependencies = getattr(platform, 'DEPENDENCIES', [])
            for dependency in dependencies:
                if dependency not in _ALL_COMPONENTS:
                    result.add_error(
                        u"Platform {} requires component {}".format(p_domain, dependency),
                        p_domain, p_config)
                    success = False
            if not success:
                continue

            esp_platforms = getattr(platform, 'ESP_PLATFORMS', ESP_PLATFORMS)
            if core.ESP_PLATFORM not in esp_platforms:
                result.add_error(
                    u"Platform {} doesn't support {}.".format(p_domain, core.ESP_PLATFORM),
                    p_domain, p_config)
                continue

            if hasattr(platform, u'PLATFORM_SCHEMA'):
                try:
                    p_validated = platform.PLATFORM_SCHEMA(p_config)
                except vol.Invalid as ex:
                    _comp_error(ex, p_domain, p_config)
                    continue
                platforms.append(p_validated)
        result[domain] = platforms

    do_id_pass(result)
    return result


REQUIRED = ['esphomeyaml', 'wifi']


def _format_config_error(ex, domain, config):
    message = u"Invalid config for [{}]: ".format(domain)
    if u'extra keys not allowed' in ex.error_message:
        message += u'[{}] is an invalid option for [{}]. Check: {}->{}.' \
            .format(ex.path[-1], domain, domain,
                    u'->'.join(str(m) for m in ex.path))
    else:
        message += u'{}.'.format(humanize_error(config, ex))

    if isinstance(config, list):
        return message

    domain_config = config.get(domain, config)
    message += u" (See {}, line {}). ".format(
        getattr(domain_config, '__config_file__', '?'),
        getattr(domain_config, '__line__', '?'))

    return message


def load_config(path):
    try:
        config = yaml_util.load_yaml(path)
    except OSError:
        raise ESPHomeYAMLError(u"Could not read configuration file at {}".format(path))
    core.RAW_CONFIG = config
    core_config.preload_core_config(config)

    try:
        result = validate_config(config)
    except ESPHomeYAMLError:
        raise
    except Exception:
        _LOGGER.error(u"Unexpected exception while reading configuration:")
        raise

    return result


def line_info(obj, **kwargs):
    """Display line config source."""
    if hasattr(obj, '__config_file__'):
        return color('cyan', "[source {}:{}]"
                     .format(obj.__config_file__, obj.__line__ or '?'),
                     **kwargs)
    return '?'


def dump_dict(layer, indent_count=3, listi=False, **kwargs):
    def sort_dict_key(val):
        """Return the dict key for sorting."""
        key = str.lower(val[0])
        return '0' if key == 'platform' else key

    indent_str = indent_count * ' '
    if listi or isinstance(layer, list):
        indent_str = indent_str[:-1] + '-'
    if isinstance(layer, dict):
        for key, value in sorted(layer.items(), key=sort_dict_key):
            if isinstance(value, (dict, list)):
                print(indent_str, key + ':', line_info(value, **kwargs))
                dump_dict(value, indent_count + 2)
            else:
                print(indent_str, key + ':', value)
            indent_str = indent_count * ' '
    if isinstance(layer, (list, tuple)):
        for i in layer:
            if isinstance(i, dict):
                dump_dict(i, indent_count + 2, True)
            else:
                print(' ', indent_str, i)


def read_config(path):
    _LOGGER.info("Reading configuration...")
    try:
        res = load_config(path)
    except ESPHomeYAMLError as err:
        _LOGGER.error(u"Error while reading config: %s", err)
        return None
    excepts = {}
    for message, domain, config in res.errors:
        domain = domain or u"General Error"
        excepts.setdefault(domain, []).append(message)
        if config is not None:
            excepts[domain].append(config)

    if excepts:
        print(color('bold_white', u"Failed config"))
        for domain, config in excepts.iteritems():
            print(' ', color('bold_red', domain + ':'), color('red', '', reset='red'))
            dump_dict(config, reset='red')
            print(color('reset'))
        return None
    return OrderedDict(res)
