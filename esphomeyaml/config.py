from __future__ import print_function

import importlib
import logging
from collections import OrderedDict

import voluptuous as vol
from voluptuous.humanize import humanize_error

import esphomeyaml.config_validation as cv
from esphomeyaml import helpers, yaml_util
from esphomeyaml.const import CONF_BOARD, CONF_ESPHOMEYAML, CONF_LIBRARY_URI, CONF_MQTT, \
    CONF_NAME, \
    CONF_PLATFORM, CONF_SIMPLIFY, CONF_WIFI, ESP_PLATFORMS, ESP_PLATFORM_ESP32, \
    ESP_PLATFORM_ESP8266
from esphomeyaml.core import ESPHomeYAMLError
from esphomeyaml.helpers import App, add, add_task, color

_LOGGER = logging.getLogger(__name__)

DEFAULT_LIBRARY_URI = u'https://github.com/OttoWinter/esphomelib.git#v1.2.1'

CORE_SCHEMA = vol.Schema({
    vol.Required(CONF_NAME): cv.valid_name,
    vol.Required(CONF_PLATFORM): vol.All(
        vol.Upper, vol.Any(ESP_PLATFORM_ESP32, ESP_PLATFORM_ESP8266)),
    vol.Required(CONF_BOARD): cv.string,
    vol.Optional(CONF_LIBRARY_URI, default=DEFAULT_LIBRARY_URI): cv.string,
    vol.Optional(CONF_SIMPLIFY, default=True): cv.boolean,
})

REQUIRED_COMPONENTS = [
    CONF_ESPHOMEYAML, CONF_WIFI, CONF_MQTT
]

_COMPONENT_CACHE = {}
_ALL_COMPONENTS = []


def core_to_code(config):
    add(App.set_name(config[CONF_NAME]))


def get_component(domain):
    if domain in _COMPONENT_CACHE:
        return _COMPONENT_CACHE[domain]

    path = 'esphomeyaml.components.{}'.format(domain)
    try:
        module = importlib.import_module(path)
    except ImportError as err:
        _LOGGER.debug(err)
        pass
    else:
        _COMPONENT_CACHE[domain] = module
        return module

    _LOGGER.error("Unable to find component %s", domain)
    return None


def get_platform(domain, platform):
    return get_component("{}.{}".format(domain, platform))


def is_platform_component(component):
    return hasattr(component, 'PLATFORM_SCHEMA')


def validate_schema(config, schema):
    return schema(config)


class Config(OrderedDict):
    def __init__(self):
        super(Config, self).__init__()
        self.errors = []

    def add_error(self, message, domain=None, config=None):
        if not isinstance(message, unicode):
            message = unicode(message)
        self.errors.append((message, domain, config))


def validate_config(config):
    global _ALL_COMPONENTS

    for req in REQUIRED_COMPONENTS:
        if req not in config:
            raise ESPHomeYAMLError("Component %s is required for esphomeyaml.", req)

    _ALL_COMPONENTS = list(config.keys())

    result = Config()

    def _comp_error(ex, domain, config):
        result.add_error(_format_config_error(ex, domain, config), domain, config)

    try:
        result[CONF_ESPHOMEYAML] = validate_schema(config[CONF_ESPHOMEYAML], CORE_SCHEMA)
    except vol.Invalid as ex:
        _comp_error(ex, CONF_ESPHOMEYAML, config)

    for domain, conf in config.iteritems():
        if domain == CONF_ESPHOMEYAML:
            continue
        if conf is None:
            conf = {}
        component = get_component(domain)
        if component is None:
            result.add_error(u"Component not found: {}".format(domain))
            continue

        esp_platforms = getattr(component, 'ESP_PLATFORMS', ESP_PLATFORMS)
        if cv.ESP_PLATFORM not in esp_platforms:
            result.add_error(u"Component {} doesn't support {}.".format(domain, cv.ESP_PLATFORM))
            continue

        success = True
        dependencies = getattr(component, 'DEPENDENCIES', [])
        for dependency in dependencies:
            if dependency not in _ALL_COMPONENTS:
                result.add_error(u"Component {} requires {}".format(domain, dependency))
                success = False
        if not success:
            continue

        if hasattr(component, 'CONFIG_SCHEMA'):
            try:
                validated = component.CONFIG_SCHEMA(conf)
                result[domain] = validated
            except vol.Invalid as ex:
                _comp_error(ex, domain, config)
                continue

        if not hasattr(component, 'PLATFORM_SCHEMA'):
            continue

        platforms = []
        for i, p_config in enumerate(conf):
            if not isinstance(p_config, dict):
                result.add_error(u"Platform schemas mus have 'platform:' key")
                continue
            p_name = p_config.get(u'platform')
            if p_name is None:
                result.add_error(u"No platform specified for {}".format(domain))
                continue
            platform = get_platform(domain, p_name)
            if platform is None:
                result.add_error(u"Platform not found: {}.{}")
                continue

            if hasattr(platform, u'PLATFORM_SCHEMA'):
                try:
                    p_validated = platform.PLATFORM_SCHEMA(p_config)
                except vol.Invalid as ex:
                    _comp_error(ex, u'{}.{}'.format(domain, p_name), p_config)
                    continue
                platforms.append(p_validated)
        result[domain] = platforms
    return result


REQUIRED = ['esphomeyaml', 'wifi', 'mqtt']


def _format_config_error(ex, domain, config):
    message = u"Invalid config for [{}]: ".format(domain)
    if u'extra keys not allowed' in ex.error_message:
        message += u'[{}] is an invalid option for [{}]. Check: {}->{}.' \
            .format(ex.path[-1], domain, domain,
                    u'->'.join(str(m) for m in ex.path))
    else:
        message += u'{}.'.format(humanize_error(config, ex))

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

    esp_platform = unicode(config.get(CONF_ESPHOMEYAML, {}).get(CONF_PLATFORM, u""))
    esp_platform = esp_platform.upper()
    if esp_platform not in (ESP_PLATFORM_ESP32, ESP_PLATFORM_ESP8266):
        raise ESPHomeYAMLError(u"Invalid ESP Platform {}".format(esp_platform))
    cv.ESP_PLATFORM = esp_platform
    cv.BOARD = unicode(config.get(CONF_ESPHOMEYAML, {}).get(CONF_BOARD, u""))
    helpers.SIMPLIFY = cv.boolean(config.get(CONF_SIMPLIFY, True))

    try:
        result = validate_config(config)
    except Exception as e:
        print(u"Unexpected exception while reading configuration:")
        raise

    return result


def add_platform_task(domain, config):
    platform_ = config[CONF_PLATFORM]
    platform = get_platform(domain, platform_)
    if not hasattr(platform, 'to_code'):
        raise ESPHomeYAMLError(u"Platform '{}.{}' doesn't have to_code.".format(domain, platform_))
    add_task(platform.to_code, config)


def add_component_task(domain, config):
    if domain == CONF_ESPHOMEYAML:
        add_task(core_to_code, config)
        return
    component = get_component(domain)
    if is_platform_component(component):
        for conf in config:
            add_platform_task(domain, conf)
    else:
        if not hasattr(component, 'to_code'):
            raise ESPHomeYAMLError(u"Component '{}' doesn't have to_code.".format(domain))
        add_task(component.to_code, config)


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
    _LOGGER.debug("Reading configuration...")
    try:
        res = load_config(path)
    except ESPHomeYAMLError as e:
        _LOGGER.error(u"Error while reading config: %s", e)
        return None
    excepts = {}
    for err in res.errors:
        domain = err[1] or u"General Error"
        excepts.setdefault(domain, []).append(err[0])
        if err[2] is not None:
            excepts[domain].append(err[2])

    if excepts:
        print(color('bold_white', u"Failed config"))
        for domain, config in excepts.iteritems():
            print(' ', color('bold_red', domain + ':'), color('red', '', reset='red'))
            dump_dict(config, reset='red')
            print(color('reset'))
        return None
    return dict(**res)
