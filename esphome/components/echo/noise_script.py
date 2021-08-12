import socket
from noise.connection import NoiseConnection

proto = NoiseConnection.from_name(b"Noise_NNpsk0_25519_ChaChaPoly_SHA256")
proto.set_as_initiator()
proto.set_psks(
    b"\xC1\xD5\xE0\x72\xE7\x77\x58\x02\x45\xCB\x3A\x81\x04\x1B\x2D\x90"
    b"\x3A\x0F\x0E\xC7\x9C\xFC\xB4\x2A\x50\xC0\xE6\x35\xA1\x54\x18\x12"
)
proto.start_handshake()
# sys.exit(1)

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
print("[x] Connecting...")
sock.connect(("192.168.178.154", 6056))
print("[x] Connected!")


def write(msg):
    print(f"[x] Writing msg {msg}")
    l = len(msg)
    buf = bytes(
        [
            (l >> 8) & 0xFF,
            (l >> 0) & 0xFF,
        ]
    )
    buf += msg
    sock.sendall(buf)


def recv():
    buf = b""
    while len(buf) < 2:
        buf += sock.recv(2 - len(buf))
    l = (buf[0] << 8) | buf[1]
    buf = buf[2:]
    while len(buf) < l:
        buf += sock.recv(l - len(buf))

    print(f"[x] Received message {buf}")
    return buf


do_write = True
while not proto.handshake_finished:
    if do_write:
        msg = proto.write_message()
        write(msg)
    else:
        msg = recv()
        proto.read_message(msg)

    do_write = not do_write

print(f"[x] Handshake done!")

while True:
    msg = input().encode()
    buf = proto.encrypt(msg)
    write(buf)
    buf = recv()
    msg2 = proto.decrypt(buf)
    print(msg2)
