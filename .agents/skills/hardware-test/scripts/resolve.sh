#!/bin/bash
# resolve.sh NAME [TRIES]: print the address of NAME.local once it resolves and answers a ping, retrying every 3 s
# (default 30 tries). Exits 1 when it never does.
for _ in $(seq 1 "${2:-30}"); do
  IP=$(python3 -c 'import socket, sys; print(socket.gethostbyname(sys.argv[1]))' "$1.local" 2>/dev/null) &&
    ping -c 1 "$IP" > /dev/null 2>&1 && { echo "$IP"; exit 0; }
  sleep 3
done
echo "$1.local did not resolve or answer" >&2
exit 1
