from datetime import datetime
import functools
import logging
import socket
import threading
import time

from typing import Optional, Tuple

from google.protobuf import message

from esphomeyaml import const
import esphomeyaml.api.api_pb2 as pb
from esphomeyaml.const import CONF_PORT, CONF_PASSWORD
from esphomeyaml.core import EsphomeyamlError
from esphomeyaml.helpers import resolve_ip_address
from esphomeyaml.util import safe_print

_LOGGER = logging.getLogger(__name__)


class APIConnectionError(EsphomeyamlError):
    pass


MESSAGE_TYPE_TO_PROTO = {
    1: pb.HelloRequest,
    2: pb.HelloResponse,
    3: pb.ConnectRequest,
    4: pb.ConnectResponse,
    5: pb.DisconnectRequest,
    6: pb.DisconnectResponse,
    7: pb.PingRequest,
    8: pb.PingResponse,
    9: pb.DeviceInfoRequest,
    10: pb.DeviceInfoResponse,
    11: pb.ListEntitiesRequest,
    12: pb.ListEntitiesBinarySensorResponse,
    13: pb.ListEntitiesCoverResponse,
    14: pb.ListEntitiesFanResponse,
    15: pb.ListEntitiesLightResponse,
    16: pb.ListEntitiesSensorResponse,
    17: pb.ListEntitiesSwitchResponse,
    18: pb.ListEntitiesTextSensorResponse,
    19: pb.ListEntitiesDoneResponse,
    20: pb.SubscribeStatesRequest,
    21: pb.BinarySensorStateResponse,
    22: pb.CoverStateResponse,
    23: pb.FanStateResponse,
    24: pb.LightStateResponse,
    25: pb.SensorStateResponse,
    26: pb.SwitchStateResponse,
    27: pb.TextSensorStateResponse,
    28: pb.SubscribeLogsRequest,
    29: pb.SubscribeLogsResponse,
    30: pb.CoverCommandRequest,
    31: pb.FanCommandRequest,
    32: pb.LightCommandRequest,
    33: pb.SwitchCommandRequest,
}


def _varuint_to_bytes(value):
    if value <= 0x7F:
        return chr(value)

    ret = bytes()
    while value:
        temp = value & 0x7F
        value >>= 7
        if value:
            ret += chr(temp | 0x80)
        else:
            ret += chr(temp)

    return ret


def _bytes_to_varuint(value):
    result= 0
    bitpos = 0
    for c in value:
        val = ord(c)
        result |= (val & 0x7F) << bitpos
        bitpos += 7
        if (val & 0x80) == 0:
            return result
    return None


class APIClient(threading.Thread):
    def __init__(self, address, port, password):
        threading.Thread.__init__(self)
        self._address = address  # type: str
        self._port = port  # type: int
        self._password = password  # type: Optional[str]
        self._socket = None  # type: Optional[socket.socket]
        self._connected = False
        self._authenticated = False
        self._message_handlers = []
        self._keepalive = 10
        self._ping_timer = None
        self._refresh_ping()

        self.on_disconnect = None
        self.on_connect = None
        self.on_login = None
        self.auto_reconnect = False
        self._running = False
        self._stop_event = threading.Event()
        self._socket_open = False

    @property
    def stopped(self):
        return self._stop_event.is_set()

    def _refresh_ping(self):
        if self._ping_timer is not None:
            self._ping_timer.cancel()
            self._ping_timer = None

        def func():
            self._ping_timer = None

            if self._connected:
                try:
                    self.ping()
                except APIConnectionError:
                    self._on_error()
            self._refresh_ping()

        self._ping_timer = threading.Timer(self._keepalive, func)
        self._ping_timer.start()

    def stop(self, force=False):
        if self.stopped:
            raise ValueError

        if self._connected and not force:
            try:
                self.disconnect()
            except APIConnectionError:
                pass
        if self._socket is not None:
            self._socket.close()
            self._socket = None

        self._stop_event.set()
        if self._ping_timer is not None:
            self._ping_timer.cancel()
            self._ping_timer = None
        if not force:
            self.join()

    def connect(self):
        if not self._running:
            raise APIConnectionError("You need to call start() first!")

        if self._connected:
            raise APIConnectionError("Already connected!")

        self._message_handlers = []
        self._socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._socket.settimeout(10.0)
        self._socket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

        try:
            ip = resolve_ip_address(self._address)
        except EsphomeyamlError as err:
            _LOGGER.warning("Error resolving IP address of %s. Is it connected to WiFi?",
                            self._address)
            _LOGGER.warning("(If this error persists, please set a static IP address: "
                            "https://esphomelib.com/esphomeyaml/components/wifi.html#manual-ips)")
            raise APIConnectionError(err)

        _LOGGER.info("Connecting to %s:%s (%s)", self._address, self._port, ip)
        try:
            self._socket.connect((ip, self._port))
        except socket.error as err:
            self._on_error()
            raise APIConnectionError("Error connecting to {}: {}".format(ip, err))
        self._socket_open = True

        self._socket.settimeout(0.1)

        hello = pb.HelloRequest()
        hello.client_info = 'esphomeyaml v{}'.format(const.__version__)
        try:
            resp = self._send_message_await_response(hello, pb.HelloResponse)
        except APIConnectionError as err:
            self._on_error()
            raise err
        _LOGGER.debug("Successfully connected to %s ('%s' API=%s.%s)", self._address,
                      resp.server_info, resp.api_version_major, resp.api_version_minor)
        self._connected = True
        if self.on_connect is not None:
            self.on_connect()

    def _check_connected(self):
        if not self._connected:
            self._on_error()
            raise APIConnectionError("Must be connected!")

    def login(self):
        self._check_connected()
        if self._authenticated:
            raise APIConnectionError("Already logged in!")

        connect = pb.ConnectRequest()
        if self._password is not None:
            connect.password = self._password
        resp = self._send_message_await_response(connect, pb.ConnectResponse)
        if resp.invalid_password:
            raise APIConnectionError("Invalid password!")

        self._authenticated = True
        if self.on_login is not None:
            self.on_login()

    def _on_error(self):
        if self._connected and self.on_disconnect is not None:
            self.on_disconnect()

        if self._socket is not None:
            self._socket.close()
            self._socket = None
            self._socket_open = False

        self._connected = False
        self._authenticated = False

    def _write(self, data):  # type: (bytes) -> None
        _LOGGER.debug("Write: %s", ' '.join('{:02X}'.format(ord(x)) for x in data))
        try:
            self._socket.sendall(data)
        except socket.error as err:
            self._on_error()
            raise APIConnectionError("Error while writing data: {}".format(err))

    def _send_message(self, msg):
        # type: (message.Message) -> None
        for message_type, klass in MESSAGE_TYPE_TO_PROTO.iteritems():
            if isinstance(msg, klass):
                break
        else:
            raise ValueError

        encoded = msg.SerializeToString()
        _LOGGER.debug("Sending %s: %s", type(message), unicode(message))
        req = chr(0x00)
        req += _varuint_to_bytes(len(encoded))
        req += _varuint_to_bytes(message_type)
        req += encoded
        self._write(req)
        self._refresh_ping()

    def _send_message_await_response_complex(self, send_msg, do_append, do_stop, timeout=1):
        event = threading.Event()
        responses = []

        def on_message(resp):
            if do_append(resp):
                responses.append(resp)
            if do_stop(resp):
                event.set()

        self._message_handlers.append(on_message)
        self._send_message(send_msg)
        ret = event.wait(timeout)
        try:
            self._message_handlers.remove(on_message)
        except ValueError:
            pass
        if not ret:
            raise APIConnectionError("Timeout while waiting for message response!")
        return responses

    def _send_message_await_response(self, send_msg, response_type, timeout=1):
        def is_response(msg):
            return isinstance(msg, response_type)
        return self._send_message_await_response_complex(send_msg, is_response, is_response,
                                                         timeout)[0]

    def device_info(self):
        self._check_connected()
        return self._send_message_await_response(pb.DeviceInfoRequest(), pb.DeviceInfoResponse)

    def ping(self):
        self._check_connected()
        return self._send_message_await_response(pb.PingRequest(), pb.PingResponse)

    def disconnect(self):
        self._check_connected()

        try:
            self._send_message_await_response(pb.DisconnectRequest(), pb.DisconnectResponse)
        except APIConnectionError:
            pass
        if self._socket is not None:
            self._socket.close()
            self._socket = None
            self._socket_open = False
            self._connected = False
        if self.on_disconnect is not None:
            self.on_disconnect()

    def _check_authenticated(self):
        if not self._authenticated:
            raise APIConnectionError("Must login first!")

    def list_entities(self):
        self._check_authenticated()
        response_types = (
            pb.ListEntitiesBinarySensorResponse,
            pb.ListEntitiesCoverResponse,
            pb.ListEntitiesFanResponse,
            pb.ListEntitiesLightResponse,
            pb.ListEntitiesSensorResponse,
            pb.ListEntitiesSwitchResponse,
            pb.ListEntitiesTextSensorResponse,
        )

        def do_append(msg):
            return isinstance(msg, response_types)

        def do_stop(msg):
            return isinstance(msg, pb.ListEntitiesDoneResponse)

        entities = self._send_message_await_response_complex(
            pb.ListEntitiesRequest(), do_append, do_stop, timeout=5)
        return entities

    def subscribe_states(self, on_state):
        self._check_authenticated()

        response_types = (
            pb.BinarySensorStateResponse,
            pb.CoverStateResponse,
            pb.FanStateResponse,
            pb.LightStateResponse,
            pb.SensorStateResponse,
            pb.SwitchStateResponse,
            pb.TextSensorStateResponse,
        )

        def on_msg(msg):
            if isinstance(msg, response_types):
                on_state(msg)

        self._message_handlers.append(on_msg)
        self._send_message(pb.SubscribeStatesRequest())

    def subscribe_logs(self, on_log, log_level=None):
        self._check_authenticated()

        def on_msg(msg):
            if isinstance(msg, pb.SubscribeLogsResponse):
                on_log(msg)

        self._message_handlers.append(on_msg)
        req = pb.SubscribeLogsRequest()
        if log_level is not None:
            req.level = log_level
        self._send_message(req)

    def cover_command(self,
                      key,  # type: int
                      command  # type: pb.CoverCommandRequest.CoverCommand
                      ):
        # type: (...) -> None
        self._check_authenticated()

        req = pb.CoverCommandRequest()
        req.key = key
        req.has_state = True
        req.command = command
        self._send_message(req)

    def fan_command(self,
                    key,  # type: int
                    state=None,  # type: Optional[bool]
                    speed=None,  # type: Optional[pb.FanSpeed]
                    oscillating=None  # type: Optional[bool]
                    ):
        # type: (...) -> None
        self._check_authenticated()

        req = pb.FanCommandRequest()
        req.key = key
        if state is not None:
            req.has_state = True
            req.state = state
        if speed is not None:
            req.has_speed = True
            req.speed = speed
        if oscillating is not None:
            req.has_oscillating = True
            req.oscillating = oscillating
        self._send_message(req)

    def light_command(self,
                      key,  # type: int
                      state=None,  # type: Optional[bool]
                      brightness=None,  # type: Optional[float]
                      rgb=None,  # type: Optional[Tuple[float, float, float]]
                      white=None,  # type: Optional[float]
                      color_temperature=None,  # type: Optional[float]
                      transition_length=None,  # type: Optional[int]
                      flash_length=None,  # type: Optional[int]
                      effect=None  # type: Optional[str]
                      ):
        # type: (...) -> None
        self._check_authenticated()

        req = pb.LightCommandRequest()
        req.key = key
        if state is not None:
            req.has_state = True
            req.state = state
        if brightness is not None:
            req.has_brightness = True
            req.brightness = brightness
        if rgb is not None:
            req.has_rgb = True
            req.red = rgb[0]
            req.green = rgb[1]
            req.blue = rgb[2]
        if white is not None:
            req.has_white = True
            req.white = white
        if color_temperature is not None:
            req.has_color_temperature = True
            req.color_temperature = color_temperature
        if transition_length is not None:
            req.has_transition_length = True
            req.transition_length = transition_length
        if flash_length is not None:
            req.has_flash_length = True
            req.flash_length = flash_length
        if effect is not None:
            req.has_effect = True
            req.effect = effect
        self._send_message(req)

    def switch_command(self,
                       key,  # type: int
                       state  # type: bool
                       ):
        # type: (...) -> None
        self._check_authenticated()

        req = pb.SwitchCommandRequest()
        req.key = key
        req.state = state
        self._send_message(req)

    def _recv(self, amount):
        ret = bytes()
        if amount == 0:
            return ret

        while len(ret) < amount:
            if self.stopped:
                raise APIConnectionError("Stopped!")
            if self._socket is None or not self._socket_open:
                raise APIConnectionError("No socket!")
            try:
                val = self._socket.recv(amount - len(ret))
            except socket.timeout:
                continue
            except socket.error as err:
                raise APIConnectionError("Error while receiving data: {}".format(err))
            ret += val
        return ret

    def _recv_varint(self):
        raw = bytes()
        while not raw or ord(raw[-1]) & 0x80:
            raw += self._recv(1)
        return _bytes_to_varuint(raw)

    def _run_once(self):
        if self._socket is None or not self._socket_open:
            time.sleep(0.1)
            return

        # Preamble
        if ord(self._recv(1)[0]) != 0x00:
            raise APIConnectionError("Invalid preamble")

        length = self._recv_varint()
        msg_type = self._recv_varint()

        raw_msg = self._recv(length)
        msg = MESSAGE_TYPE_TO_PROTO[msg_type]()
        msg.ParseFromString(raw_msg)
        _LOGGER.debug("Got message of type %s: %s", type(msg), msg)
        for msg_handler in self._message_handlers[:]:
            msg_handler(msg)
        self._handle_internal_messages(msg)
        self._refresh_ping()

    def run(self):
        self._running = True
        while not self.stopped:
            try:
                self._run_once()
            except APIConnectionError as err:
                if self.stopped:
                    break
                if self._connected:
                    _LOGGER.error("Error while reading incoming messages: %s", err)
                    self._on_error()
        self._running = False

    def _handle_internal_messages(self, msg):
        if isinstance(msg, pb.DisconnectRequest):
            self._send_message(pb.DisconnectResponse())
            if self._socket is not None:
                self._socket.close()
                self._socket = None
            self._connected = False
            self._socket_open = False
            if self.on_disconnect is not None:
                self.on_disconnect()
        elif isinstance(msg, pb.PingRequest):
            self._send_message(pb.PingResponse())


def run_logs(config, address):
    conf = config['api']
    port = conf[CONF_PORT]
    password = conf[CONF_PASSWORD]
    _LOGGER.info("Starting log output from %s using esphomelib API", address)

    cli = APIClient(address, port, password)
    stopping = False
    retry_timer = []

    def try_connect(tries=0, is_disconnect=True):
        if stopping:
            return

        if is_disconnect:
            _LOGGER.warning(u"Disconnected from API.")

        while retry_timer:
            retry_timer.pop(0).cancel()

        error = None
        try:
            cli.connect()
            cli.login()
        except APIConnectionError as error:
            pass

        if error is None:
            _LOGGER.info("Successfully connected to %s", address)
            return

        wait_time = min(2**tries, 300)
        _LOGGER.warning(u"Couldn't connect to API. Trying to reconnect in %s seconds", wait_time)
        timer = threading.Timer(wait_time, functools.partial(try_connect, tries + 1, is_disconnect))
        timer.start()
        retry_timer.append(timer)

    def on_log(msg):
        time_ = datetime.now().time().strftime(u'[%H:%M:%S]')
        message = time_ + msg.message
        safe_print(message)

    def on_login():
        try:
            cli.subscribe_logs(on_log)
        except APIConnectionError:
            cli.disconnect()

    cli.on_disconnect = try_connect
    cli.on_login = on_login
    cli.start()

    try:
        try_connect(is_disconnect=False)
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        stopping = True
        cli.stop()
        while retry_timer:
            retry_timer.pop(0).cancel()
    return 0
