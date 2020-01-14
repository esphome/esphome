from base64 import b64encode, b64decode
from datetime import datetime, timedelta
from hashlib import sha1, sha256
from hmac import HMAC
from socket import create_connection
from ssl import create_default_context
from time import time, strptime, mktime
from urllib import parse
from urllib.request import urlopen

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CORE, coroutine_with_priority

DEPENDENCIES = ['network']
AUTO_LOAD = ['json', 'http_request']


azure_iot_hub_ns = cg.esphome_ns.namespace('azure_iot_hub')
AzureIoTHub = azure_iot_hub_ns.class_('AzureIoTHub', cg.Component, cg.Controller)


# Required configuration variables
CONF_HUB_NAME = 'hub_name'
CONF_DEVICE_ID = 'device_id'
CONF_DEVICE_KEY = 'device_key'

# Optional
# api version. Can normally be omitted but required for IoT Edge connection
CONF_API_VERSION = 'api_version'

# Expiration in seconds of generated token. Will be
# reduced to SSL certificate expiration if insecure_ssl is disabled.
# Default is AUTO which sets it to 100 years or ssl
# expiration (whichever is shorter)
CONF_TOKEN_EXPIRATION_SECONDS = 'token_expiration_seconds'


def validate_config(value):
    out = value.copy()
    out[CONF_HUB_NAME] = validate_hub_name(value[CONF_HUB_NAME])
    return out


def validate_hub_name(value):
    value = cv.string(value)
    if not value:
        raise cv.Invalid(CONF_HUB_NAME + ' is required')
    if '/' in value or '\\' in value or ':' in value or '.' in value:
        raise cv.Invalid(CONF_HUB_NAME + ' should only contain the name of the Azure IoT Hub')
    return value


CONFIG_SCHEMA = cv.All(cv.Schema({
    cv.GenerateID(): cv.declare_id(AzureIoTHub),
    cv.Required(CONF_HUB_NAME): cv.string,
    cv.Required(CONF_DEVICE_ID): cv.string,
    cv.Required(CONF_DEVICE_KEY): cv.string,
    cv.Optional(CONF_API_VERSION, default='2018-06-30'): cv.string,
    cv.Optional(CONF_TOKEN_EXPIRATION_SECONDS, default='AUTO'):
        cv.Any(cv.int_, cv.one_of("AUTO", upper=True))
}).extend(cv.COMPONENT_SCHEMA), validate_config)


# from https://docs.microsoft.com/en-us/azure/iot-hub/iot-hub-devguide-security
def generate_iot_hub_sas_token(uri, key, expiry_seconds):
    ttl = time() + expiry_seconds
    sign_key = "%s\n%d" % ((parse.quote_plus(uri)), int(ttl))
    signature = b64encode(HMAC(b64decode(key), sign_key.encode('utf-8'), sha256).digest())

    raw_token = {
        'sr': uri,
        'sig': signature,
        'se': str(int(ttl))
    }

    return 'SharedAccessSignature ' + parse.urlencode(raw_token)


@coroutine_with_priority(40.0)
def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)

    iot_hub_uri = f'https://{config[CONF_HUB_NAME].strip()}.azure-devices.net' \
                  + f'/devices/{config[CONF_DEVICE_ID].strip()}/messages/events'
    if config[CONF_API_VERSION]:
        iot_hub_uri = iot_hub_uri + f'?api-version={config[CONF_API_VERSION].strip()}'

    cg.add(var.set_iot_hub_rest_url(iot_hub_uri))
    cg.add(var.set_iot_hub_device_id(config[CONF_DEVICE_ID]))

    # set default expiration to approximately 100 years
    token_expiration = (datetime.utcnow() + timedelta(days=100*365)).timestamp()
    if config[CONF_TOKEN_EXPIRATION_SECONDS] != 'AUTO' \
            and config[CONF_TOKEN_EXPIRATION_SECONDS] > 0:
        token_expiration = min(token_expiration, time() + config[CONF_TOKEN_EXPIRATION_SECONDS])

    sas_token = generate_iot_hub_sas_token(
        f'{config[CONF_HUB_NAME].strip()}.azure-devices.net'
        + f'/devices/{config[CONF_DEVICE_ID].strip()}',
        config[CONF_DEVICE_KEY], token_expiration)

    cg.add(var.set_iot_hub_sas_token(sas_token))

    # put string representation of expiration for debugging
    dt = datetime.fromtimestamp(token_expiration)
    cg.add(var.set_iot_hub_sas_token_expiration_string(dt.isoformat()))
