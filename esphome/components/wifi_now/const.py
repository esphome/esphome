# Constants used in Wifi Now

from esphome.const import CONF_ID, CONF_SERVICE, CONF_CHANNEL, CONF_INITIAL_VALUE, \
    CONF_BSSID, CONF_VALUE, CONF_TYPE, CONF_PASSWORD , CONF_TYPE_ID, CONF_TRIGGER_ID, \
    CONF_ON_PRESS, CONF_ON_RELEASE, CONF_ON_CLICK, CONF_ON_DOUBLE_CLICK, \
    CONF_ON_MULTI_CLICK, CONF_BINARY_SENSOR

CONF_AESKEY = 'aeskey'
CONF_MAX_RETRYS = 'max_retrys'
CONF_ON_FAIL = 'on_fail'
CONF_ON_RECEIVE = 'on_receive'
CONF_ON_SUCCESS = 'on_success'
CONF_PAYLOAD_ID = 'payload_id'
CONF_PAYLOADS = 'payloads'
CONF_PEERID = 'peerid'
CONF_PEERS = 'peers'
CONF_SENSOR_ID = 'sensor_id'
CONF_SERVICEKEY = 'servicekey'
CONF_WIFI_NOW = 'wifi_now'

PAYLOAD_BINARY_SENSOR_EVENT = 'binary_sensor_event'
PAYLOAD_BOOL = 'bool'
PAYLOAD_FLOAT = 'float'
PAYLOAD_PAYLOAD = 'payload'
PAYLOAD_STRING = 'string'
PAYLOAD_VECTOR = 'vector'

ACTION_ABORT = 'wifi_now.abort'
ACTION_INJECT = 'wifi_now.inject'
ACTION_RETRY_SEND = 'wifi_now.retry_send'
ACTION_SEND = 'wifi_now.send'
ACTION_SEND_BOOLEAN = 'wifi_now.send_boolean'
ACTION_SEND_EVENT = 'wifi_now.send_event'
ACTION_SEND_FLOAT = 'wifi_now.send_float'
ACTION_SEND_PAYLOAD = 'wifi_now.send_payload'
ACTION_SEND_TEXT = 'wifi_now.send_text'
