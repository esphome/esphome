import re
from datetime import datetime
from ssl import create_default_context
from socket import create_connection
from hashlib import sha1, sha256, md5
from base64 import b64encode, b64decode
from time import time, strptime, mktime
from urllib import parse
from hmac import HMAC
import requests
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID
from esphome.core import coroutine_with_priority, CORE

DEPENDENCIES = ['network']

# Required configuration variables
CONF_HUB_NAME = 'hub_name'
CONF_DEVICE_ID = 'device_id'
CONF_DEVICE_KEY = 'device_key'

# Optional
# api version. Can normally be omitted by required for IoT Edge connection
CONF_API_VERSION = 'api_version'

# if set to false, *.azure-devices.net SSL certificate will not be validated
CONF_VALIDATE_SSL = 'validate_ssl'

# SSL certificate fingerprint to expect from *.azure-devices.net.
# If not supplied and validate_ssl is true, it is auto generated
CONF_SSL_SHA1_FINGERPRINT = 'ssl_sha1_fingerprint'

# Expiration in seconds of generated token. Will be
# reduced to SSL certificate expiration if validate_ssl is enabled. Default is -1 which sets it to 10 years or ssl
# expiration (whichever is shorter)
CONF_TOKEN_EXPIRATION_SECONDS = 'token_expiration_seconds'

# If enabled (default yes), an empty message will
# be sent to IoT hub to verify that device is valid
CONF_VALIDATE_IOT_HUB_CONNECTION = 'validate_iot_hub_connection'


def validate_config(value):
    out = value.copy()
    if CONF_SSL_SHA1_FINGERPRINT in value:
        # SSL fingerprint defined. Do basic check for format
        out[CONF_SSL_SHA1_FINGERPRINT] = validate_fingerprint(value[CONF_SSL_SHA1_FINGERPRINT])
    out[CONF_HUB_NAME] = validate_hub_name(value[CONF_HUB_NAME])
    return out


def validate_fingerprint(value):
    value = cv.string(value)
    if value != '' and re.match(r'^[0-9a-f]{40}$', value) is None:
        raise cv.Invalid(CONF_SSL_SHA1_FINGERPRINT + ' must be a valid SHA1 hash (if supplied)')
    return value


def validate_hub_name(value):
    value = cv.string(value)
    if not value:
        raise cv.Invalid(CONF_HUB_NAME + ' is required')
    if '/' in value or '\\' in value or ':' in value or '.' in value:
        raise cv.Invalid(CONF_HUB_NAME + ' should only contain the name of the Azure IoT Hub')
    return value


CONFIG_SCHEMA = cv.All(cv.Schema({
    cv.Required(CONF_HUB_NAME): cv.string,
    cv.Required(CONF_DEVICE_ID): cv.string,
    cv.Required(CONF_DEVICE_KEY): cv.string,
    cv.Optional(CONF_API_VERSION, default='2018-06-30'): cv.string,
    cv.Optional(CONF_VALIDATE_SSL, default=True): cv.boolean,
    cv.Optional(CONF_SSL_SHA1_FINGERPRINT, default=None): cv.string,
    cv.Optional(CONF_TOKEN_EXPIRATION_SECONDS, default='AUTO'): cv.Any(cv.int_, cv.one_of("AUTO", upper=True)),
    cv.Optional(CONF_VALIDATE_IOT_HUB_CONNECTION, default=True): cv.boolean
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


# based on example from https://www.solrac.nl/retrieve-thumbprint-ssltls-python/
def retrieve_ssl_certificate_fingerprint_and_expiration(host_name, port):
    context = create_default_context()
    with create_connection((host_name, port)) as sock:
        sock.settimeout(1)
        with context.wrap_socket(sock, server_hostname=host_name) as wrappedSocket:
            ssl_date_fmt = r'%b %d %H:%M:%S %Y %Z'
            der_cert_bin = wrappedSocket.getpeercert(True)
            expiration = strptime(wrappedSocket.getpeercert()['notAfter'], ssl_date_fmt)

            # Thumbprint
            thumb_md5 = md5(der_cert_bin).hexdigest()
            thumb_sha1 = sha1(der_cert_bin).hexdigest()
            thumb_sha256 = sha256(der_cert_bin).hexdigest()

            return expiration, thumb_sha1


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)

    iot_hub_uri = f'https://{config[CONF_HUB_NAME].strip()}.azure-devices.net/devices/{config[CONF_DEVICE_ID].strip()}/messages/events '
    if config[CONF_API_VERSION]:
        iot_hub_uri += f'?api-version={config[CONF_API_VERSION].strip()}'

    cg.add(var.set_iot_hub_rest_url(iot_hub_uri))
    cg.add(var.set_iot_hub_device_id(config[CONF_DEVICE_ID]))

    # figure out expiration of SSL certificate
    token_expiration = time() + 100 * 365 * 24 * 60 * 60
    if config[CONF_VALIDATE_SSL]:
        expiration, ssl_hash = retrieve_ssl_certificate_fingerprint_and_expiration(
            f'{config[CONF_HUB_NAME].strip()}.azure-devices.net', 443)
        cg.add(var.set_iot_hub_ssl_sha1_fingerprint(config[CONF_SSL_SHA1_FINGERPRINT] if config[CONF_SSL_SHA1_FINGERPRINT] else ssl_hash))
        token_expiration = min(token_expiration, mktime(expiration))
    else:
        cg.add(var.set_iot_hub_ssl_sha1_fingerprint(''))

    if config[CONF_TOKEN_EXPIRATION_SECONDS] != 'AUTO' and config[CONF_TOKEN_EXPIRATION_SECONDS] > 0:
        token_expiration = min(token_expiration, time() + config[CONF_TOKEN_EXPIRATION_SECONDS])

    sas_token = generate_iot_hub_sas_token(
        f'{config[CONF_HUB_NAME].strip()}.azure-devices.net/devices/{config[CONF_DEVICE_ID].strip()}',
        config[CONF_DEVICE_KEY], token_expiration)

    cg.add(var.set_iot_hub_sas_token(sas_token))

    # put string representation of expiration for debugging
    dt = datetime.fromtimestamp(sas_token)
    cg.add(var.set_iot_hub_sas_token_expiration_string(dt.isoformat()))


