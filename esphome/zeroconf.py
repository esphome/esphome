# Custom zeroconf implementation based on python-zeroconf
# (https://github.com/jstasiak/python-zeroconf) that supports Python 2

import errno
import logging
import select
import socket
import struct
import sys
import threading
import time

import ifaddr

log = logging.getLogger(__name__)

# Some timing constants

_LISTENER_TIME = 200

# Some DNS constants

_MDNS_ADDR = "224.0.0.251"
_MDNS_PORT = 5353

_MAX_MSG_ABSOLUTE = 8966

_FLAGS_QR_MASK = 0x8000  # query response mask
_FLAGS_QR_QUERY = 0x0000  # query
_FLAGS_QR_RESPONSE = 0x8000  # response

_FLAGS_AA = 0x0400  # Authoritative answer
_FLAGS_TC = 0x0200  # Truncated
_FLAGS_RD = 0x0100  # Recursion desired
_FLAGS_RA = 0x8000  # Recursion available

_FLAGS_Z = 0x0040  # Zero
_FLAGS_AD = 0x0020  # Authentic data
_FLAGS_CD = 0x0010  # Checking disabled

_CLASS_IN = 1
_CLASS_CS = 2
_CLASS_CH = 3
_CLASS_HS = 4
_CLASS_NONE = 254
_CLASS_ANY = 255
_CLASS_MASK = 0x7FFF
_CLASS_UNIQUE = 0x8000

_TYPE_A = 1
_TYPE_NS = 2
_TYPE_MD = 3
_TYPE_MF = 4
_TYPE_CNAME = 5
_TYPE_SOA = 6
_TYPE_MB = 7
_TYPE_MG = 8
_TYPE_MR = 9
_TYPE_NULL = 10
_TYPE_WKS = 11
_TYPE_PTR = 12
_TYPE_HINFO = 13
_TYPE_MINFO = 14
_TYPE_MX = 15
_TYPE_TXT = 16
_TYPE_AAAA = 28
_TYPE_SRV = 33
_TYPE_ANY = 255

# Mapping constants to names
int2byte = struct.Struct(">B").pack


# Exceptions
class Error(Exception):
    pass


class IncomingDecodeError(Error):
    pass


# pylint: disable=no-init
class QuietLogger:
    _seen_logs = {}

    @classmethod
    def log_exception_warning(cls, logger_data=None):
        exc_info = sys.exc_info()
        exc_str = str(exc_info[1])
        if exc_str not in cls._seen_logs:
            # log at warning level the first time this is seen
            cls._seen_logs[exc_str] = exc_info
            logger = log.warning
        else:
            logger = log.debug
        if logger_data is not None:
            logger(*logger_data)
        logger("Exception occurred:", exc_info=True)

    @classmethod
    def log_warning_once(cls, *args):
        msg_str = args[0]
        if msg_str not in cls._seen_logs:
            cls._seen_logs[msg_str] = 0
            logger = log.warning
        else:
            logger = log.debug
        cls._seen_logs[msg_str] += 1
        logger(*args)


class DNSEntry:
    """A DNS entry"""

    def __init__(self, name, type_, class_):
        self.key = name.lower()
        self.name = name
        self.type = type_
        self.class_ = class_ & _CLASS_MASK
        self.unique = (class_ & _CLASS_UNIQUE) != 0


class DNSQuestion(DNSEntry):
    """A DNS question entry"""

    def __init__(self, name, type_, class_):
        DNSEntry.__init__(self, name, type_, class_)

    def answered_by(self, rec):
        """Returns true if the question is answered by the record"""
        return (
            self.class_ == rec.class_
            and (self.type == rec.type or self.type == _TYPE_ANY)
            and self.name == rec.name
        )


class DNSRecord(DNSEntry):
    """A DNS record - like a DNS entry, but has a TTL"""

    def __init__(self, name, type_, class_, ttl):
        DNSEntry.__init__(self, name, type_, class_)
        self.ttl = 15
        self.created = time.time()

    def write(self, out):
        """Abstract method"""
        raise NotImplementedError

    def is_expired(self, now):
        return self.created + self.ttl <= now

    def is_removable(self, now):
        return self.created + self.ttl * 2 <= now


class DNSAddress(DNSRecord):
    """A DNS address record"""

    def __init__(self, name, type_, class_, ttl, address):
        DNSRecord.__init__(self, name, type_, class_, ttl)
        self.address = address

    def write(self, out):
        """Used in constructing an outgoing packet"""
        out.write_string(self.address)


class DNSText(DNSRecord):
    """A DNS text record"""

    def __init__(self, name, type_, class_, ttl, text):
        assert isinstance(text, (bytes, type(None)))
        DNSRecord.__init__(self, name, type_, class_, ttl)
        self.text = text

    def write(self, out):
        """Used in constructing an outgoing packet"""
        out.write_string(self.text)


class DNSIncoming(QuietLogger):
    """Object representation of an incoming DNS packet"""

    def __init__(self, data):
        """Constructor from string holding bytes of packet"""
        self.offset = 0
        self.data = data
        self.questions = []
        self.answers = []
        self.id = 0
        self.flags = 0  # type: int
        self.num_questions = 0
        self.num_answers = 0
        self.num_authorities = 0
        self.num_additionals = 0
        self.valid = False

        try:
            self.read_header()
            self.read_questions()
            self.read_others()
            self.valid = True

        except (IndexError, struct.error, IncomingDecodeError):
            self.log_exception_warning(
                ("Choked at offset %d while unpacking %r", self.offset, data)
            )

    def unpack(self, format_):
        length = struct.calcsize(format_)
        info = struct.unpack(format_, self.data[self.offset : self.offset + length])
        self.offset += length
        return info

    def read_header(self):
        """Reads header portion of packet"""
        (
            self.id,
            self.flags,
            self.num_questions,
            self.num_answers,
            self.num_authorities,
            self.num_additionals,
        ) = self.unpack(b"!6H")

    def read_questions(self):
        """Reads questions section of packet"""
        for _ in range(self.num_questions):
            name = self.read_name()
            type_, class_ = self.unpack(b"!HH")

            question = DNSQuestion(name, type_, class_)
            self.questions.append(question)

    def read_character_string(self):
        """Reads a character string from the packet"""
        length = self.data[self.offset]
        self.offset += 1
        return self.read_string(length)

    def read_string(self, length):
        """Reads a string of a given length from the packet"""
        info = self.data[self.offset : self.offset + length]
        self.offset += length
        return info

    def read_unsigned_short(self):
        """Reads an unsigned short from the packet"""
        return self.unpack(b"!H")[0]

    def read_others(self):
        """Reads the answers, authorities and additionals section of the
        packet"""
        n = self.num_answers + self.num_authorities + self.num_additionals
        for _ in range(n):
            domain = self.read_name()
            type_, class_, ttl, length = self.unpack(b"!HHiH")

            rec = None
            if type_ == _TYPE_A:
                rec = DNSAddress(domain, type_, class_, ttl, self.read_string(4))
            elif type_ == _TYPE_TXT:
                rec = DNSText(domain, type_, class_, ttl, self.read_string(length))
            elif type_ == _TYPE_AAAA:
                rec = DNSAddress(domain, type_, class_, ttl, self.read_string(16))
            else:
                # Try to ignore types we don't know about
                # Skip the payload for the resource record so the next
                # records can be parsed correctly
                self.offset += length

            if rec is not None:
                self.answers.append(rec)

    def is_query(self):
        """Returns true if this is a query"""
        return (self.flags & _FLAGS_QR_MASK) == _FLAGS_QR_QUERY

    def is_response(self):
        """Returns true if this is a response"""
        return (self.flags & _FLAGS_QR_MASK) == _FLAGS_QR_RESPONSE

    def read_utf(self, offset, length):
        """Reads a UTF-8 string of a given length from the packet"""
        return str(self.data[offset : offset + length], "utf-8", "replace")

    def read_name(self):
        """Reads a domain name from the packet"""
        result = ""
        off = self.offset
        next_ = -1
        first = off

        while True:
            length = self.data[off]
            off += 1
            if length == 0:
                break
            t = length & 0xC0
            if t == 0x00:
                result = "".join((result, self.read_utf(off, length) + "."))
                off += length
            elif t == 0xC0:
                if next_ < 0:
                    next_ = off + 1
                off = ((length & 0x3F) << 8) | self.data[off]
                if off >= first:
                    raise IncomingDecodeError(f"Bad domain name (circular) at {off}")
                first = off
            else:
                raise IncomingDecodeError(f"Bad domain name at {off}")

        if next_ >= 0:
            self.offset = next_
        else:
            self.offset = off

        return result


class DNSOutgoing:
    """Object representation of an outgoing packet"""

    def __init__(self, flags):
        self.finished = False
        self.id = 0
        self.flags = flags
        self.names = {}
        self.data = []
        self.size = 12
        self.state = False

        self.questions = []
        self.answers = []

    def add_question(self, record):
        """Adds a question"""
        self.questions.append(record)

    def pack(self, format_, value):
        self.data.append(struct.pack(format_, value))
        self.size += struct.calcsize(format_)

    def write_byte(self, value):
        """Writes a single byte to the packet"""
        self.pack(b"!c", int2byte(value))

    def insert_short(self, index, value):
        """Inserts an unsigned short in a certain position in the packet"""
        self.data.insert(index, struct.pack(b"!H", value))
        self.size += 2

    def write_short(self, value):
        """Writes an unsigned short to the packet"""
        self.pack(b"!H", value)

    def write_int(self, value):
        """Writes an unsigned integer to the packet"""
        self.pack(b"!I", int(value))

    def write_string(self, value):
        """Writes a string to the packet"""
        assert isinstance(value, bytes)
        self.data.append(value)
        self.size += len(value)

    def write_utf(self, s):
        """Writes a UTF-8 string of a given length to the packet"""
        utfstr = s.encode("utf-8")
        length = len(utfstr)
        self.write_byte(length)
        self.write_string(utfstr)

    def write_character_string(self, value):
        assert isinstance(value, bytes)
        length = len(value)
        self.write_byte(length)
        self.write_string(value)

    def write_name(self, name):
        # split name into each label
        parts = name.split(".")
        if not parts[-1]:
            parts.pop()

        # construct each suffix
        name_suffices = [".".join(parts[i:]) for i in range(len(parts))]

        # look for an existing name or suffix
        for count, sub_name in enumerate(name_suffices):
            if sub_name in self.names:
                break
        else:
            count = len(name_suffices)

        # note the new names we are saving into the packet
        name_length = len(name.encode("utf-8"))
        for suffix in name_suffices[:count]:
            self.names[suffix] = (
                self.size + name_length - len(suffix.encode("utf-8")) - 1
            )

        # write the new names out.
        for part in parts[:count]:
            self.write_utf(part)

        # if we wrote part of the name, create a pointer to the rest
        if count != len(name_suffices):
            # Found substring in packet, create pointer
            index = self.names[name_suffices[count]]
            self.write_byte((index >> 8) | 0xC0)
            self.write_byte(index & 0xFF)
        else:
            # this is the end of a name
            self.write_byte(0)

    def write_question(self, question):
        self.write_name(question.name)
        self.write_short(question.type)
        self.write_short(question.class_)

    def packet(self):
        if not self.state:
            for question in self.questions:
                self.write_question(question)
            self.state = True

            self.insert_short(0, 0)  # num additionals
            self.insert_short(0, 0)  # num authorities
            self.insert_short(0, 0)  # num answers
            self.insert_short(0, len(self.questions))
            self.insert_short(0, self.flags)  # _FLAGS_QR_QUERY
            self.insert_short(0, 0)
        return b"".join(self.data)


class Engine(threading.Thread):
    def __init__(self, zc):
        threading.Thread.__init__(self, name="zeroconf-Engine")
        self.daemon = True
        self.zc = zc
        self.readers = {}
        self.timeout = 5
        self.condition = threading.Condition()
        self.start()

    def run(self):
        while not self.zc.done:
            # pylint: disable=len-as-condition
            with self.condition:
                rs = self.readers.keys()
                if len(rs) == 0:
                    # No sockets to manage, but we wait for the timeout
                    # or addition of a socket
                    self.condition.wait(self.timeout)

            if len(rs) != 0:
                try:
                    rr, _, _ = select.select(rs, [], [], self.timeout)
                    if not self.zc.done:
                        for socket_ in rr:
                            reader = self.readers.get(socket_)
                            if reader:
                                reader.handle_read(socket_)

                except OSError as e:
                    # If the socket was closed by another thread, during
                    # shutdown, ignore it and exit
                    if e.args[0] != socket.EBADF or not self.zc.done:
                        raise

    def add_reader(self, reader, socket_):
        with self.condition:
            self.readers[socket_] = reader
            self.condition.notify()

    def del_reader(self, socket_):
        with self.condition:
            del self.readers[socket_]
            self.condition.notify()


class Listener(QuietLogger):
    def __init__(self, zc):
        self.zc = zc
        self.data = None

    def handle_read(self, socket_):
        try:
            data, (addr, port) = socket_.recvfrom(_MAX_MSG_ABSOLUTE)
        except Exception:  # pylint: disable=broad-except
            self.log_exception_warning()
            return

        log.debug("Received from %r:%r: %r ", addr, port, data)

        self.data = data
        msg = DNSIncoming(data)
        if not msg.valid or msg.is_query():
            pass
        else:
            self.zc.handle_response(msg)


class RecordUpdateListener:
    def update_record(self, zc, now, record):
        raise NotImplementedError()


class HostResolver(RecordUpdateListener):
    def __init__(self, name):
        self.name = name
        self.address = None

    def update_record(self, zc, now, record):
        if record is None:
            return
        if record.type == _TYPE_A:
            assert isinstance(record, DNSAddress)
            if record.name == self.name:
                self.address = record.address

    def request(self, zc, timeout):
        now = time.time()
        delay = 0.2
        next_ = now + delay
        last = now + timeout

        try:
            zc.add_listener(self)
            while self.address is None:
                if last <= now:
                    # Timeout
                    return False
                if next_ <= now:
                    out = DNSOutgoing(_FLAGS_QR_QUERY)
                    out.add_question(DNSQuestion(self.name, _TYPE_A, _CLASS_IN))
                    zc.send(out)
                    next_ = now + delay
                    delay *= 2

                zc.wait(min(next_, last) - now)
                now = time.time()
        finally:
            zc.remove_listener(self)

        return True


class DashboardStatus(RecordUpdateListener, threading.Thread):
    def __init__(self, zc, on_update):
        threading.Thread.__init__(self)
        self.zc = zc
        self.query_hosts = set()
        self.key_to_host = {}
        self.cache = {}
        self.stop_event = threading.Event()
        self.query_event = threading.Event()
        self.on_update = on_update

    def update_record(self, zc, now, record):
        if record is None:
            return
        if record.type in (_TYPE_A, _TYPE_AAAA, _TYPE_TXT):
            assert isinstance(record, DNSEntry)
            if record.name in self.query_hosts:
                self.cache.setdefault(record.name, []).insert(0, record)
            self.purge_cache()

    def purge_cache(self):
        new_cache = {}
        for host, records in self.cache.items():
            if host not in self.query_hosts:
                continue
            new_records = [rec for rec in records if not rec.is_removable(time.time())]
            if new_records:
                new_cache[host] = new_records
        self.cache = new_cache
        self.on_update({key: self.host_status(key) for key in self.key_to_host})

    def request_query(self, hosts):
        self.query_hosts = set(hosts.values())
        self.key_to_host = hosts
        self.query_event.set()

    def stop(self):
        self.stop_event.set()
        self.query_event.set()

    def host_status(self, key):
        return self.key_to_host.get(key) in self.cache

    def run(self):
        self.zc.add_listener(self)
        while not self.stop_event.is_set():
            self.purge_cache()
            for host in self.query_hosts:
                if all(
                    record.is_expired(time.time())
                    for record in self.cache.get(host, [])
                ):
                    out = DNSOutgoing(_FLAGS_QR_QUERY)
                    out.add_question(DNSQuestion(host, _TYPE_A, _CLASS_IN))
                    self.zc.send(out)
            self.query_event.wait()
            self.query_event.clear()
        self.zc.remove_listener(self)


def get_all_addresses():
    return list(
        {
            addr.ip
            for iface in ifaddr.get_adapters()
            for addr in iface.ips
            if addr.is_IPv4
            and addr.network_prefix != 32  # Host only netmask 255.255.255.255
        }
    )


def new_socket():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    # SO_REUSEADDR should be equivalent to SO_REUSEPORT for
    # multicast UDP sockets (p 731, "TCP/IP Illustrated,
    # Volume 2"), but some BSD-derived systems require
    # SO_REUSEPORT to be specified explicitly.  Also, not all
    # versions of Python have SO_REUSEPORT available.
    # Catch OSError and socket.error for kernel versions <3.9 because lacking
    # SO_REUSEPORT support.
    try:
        reuseport = socket.SO_REUSEPORT
    except AttributeError:
        pass
    else:
        try:
            s.setsockopt(socket.SOL_SOCKET, reuseport, 1)
        except OSError as err:
            # OSError on python 3, socket.error on python 2
            if err.errno != errno.ENOPROTOOPT:
                raise

    # OpenBSD needs the ttl and loop values for the IP_MULTICAST_TTL and
    # IP_MULTICAST_LOOP socket options as an unsigned char.
    ttl = struct.pack(b"B", 255)
    s.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, ttl)
    loop = struct.pack(b"B", 1)
    s.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_LOOP, loop)

    s.bind(("", _MDNS_PORT))
    return s


class Zeroconf(QuietLogger):
    def __init__(self):
        # hook for threads
        self._GLOBAL_DONE = False

        self._listen_socket = new_socket()
        interfaces = get_all_addresses()

        self._respond_sockets = []

        for i in interfaces:
            try:
                _value = socket.inet_aton(_MDNS_ADDR) + socket.inet_aton(i)
                self._listen_socket.setsockopt(
                    socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, _value
                )
            except OSError as e:
                _errno = e.args[0]
                if _errno == errno.EADDRINUSE:
                    log.info(
                        "Address in use when adding %s to multicast group, "
                        "it is expected to happen on some systems",
                        i,
                    )
                elif _errno == errno.EADDRNOTAVAIL:
                    log.info(
                        "Address not available when adding %s to multicast "
                        "group, it is expected to happen on some systems",
                        i,
                    )
                    continue
                elif _errno == errno.EINVAL:
                    log.info(
                        "Interface of %s does not support multicast, "
                        "it is expected in WSL",
                        i,
                    )
                    continue

                else:
                    raise

            respond_socket = new_socket()
            respond_socket.setsockopt(
                socket.IPPROTO_IP, socket.IP_MULTICAST_IF, socket.inet_aton(i)
            )

            self._respond_sockets.append(respond_socket)

        self.listeners = []

        self.condition = threading.Condition()

        self.engine = Engine(self)
        self.listener = Listener(self)
        self.engine.add_reader(self.listener, self._listen_socket)

    @property
    def done(self):
        return self._GLOBAL_DONE

    def wait(self, timeout):
        """Calling thread waits for a given number of milliseconds or
        until notified."""
        with self.condition:
            self.condition.wait(timeout)

    def notify_all(self):
        """Notifies all waiting threads"""
        with self.condition:
            self.condition.notify_all()

    def resolve_host(self, host, timeout=3.0):
        info = HostResolver(host)
        if info.request(self, timeout):
            return socket.inet_ntoa(info.address)
        return None

    def add_listener(self, listener):
        self.listeners.append(listener)
        self.notify_all()

    def remove_listener(self, listener):
        """Removes a listener."""
        try:
            self.listeners.remove(listener)
            self.notify_all()
        except Exception as e:  # pylint: disable=broad-except
            log.exception("Unknown error, possibly benign: %r", e)

    def update_record(self, now, rec):
        """Used to notify listeners of new information that has updated
        a record."""
        for listener in self.listeners:
            listener.update_record(self, now, rec)
        self.notify_all()

    def handle_response(self, msg):
        """Deal with incoming response packets.  All answers
        are held in the cache, and listeners are notified."""
        now = time.time()
        for record in msg.answers:
            self.update_record(now, record)

    def send(self, out):
        """Sends an outgoing packet."""
        packet = out.packet()
        log.debug("Sending %r (%d bytes) as %r...", out, len(packet), packet)
        for s in self._respond_sockets:
            if self._GLOBAL_DONE:
                return
            try:
                bytes_sent = s.sendto(packet, 0, (_MDNS_ADDR, _MDNS_PORT))
            except Exception:  # pylint: disable=broad-except
                # on send errors, log the exception and keep going
                self.log_exception_warning()
            else:
                if bytes_sent != len(packet):
                    self.log_warning_once(
                        "!!! sent %d out of %d bytes to %r"
                        % (bytes_sent, len(packet), s)
                    )

    def close(self):
        """Ends the background threads, and prevent this instance from
        servicing further queries."""
        if not self._GLOBAL_DONE:
            self._GLOBAL_DONE = True
            # shutdown recv socket and thread
            self.engine.del_reader(self._listen_socket)
            self._listen_socket.close()
            self.engine.join()

            # shutdown the rest
            self.notify_all()
            for s in self._respond_sockets:
                s.close()
