= Wireguard component
Allows connecting esphome devices to VPN managed by https://www.wireguard.com/

```yaml
# example configuration:

wireguard:
  address: 10.0.0.1
  private_key: private_key=
  peer_key: public_key=
  peer_endpoint: wg.server.example

  # optional
  peer_port: 12345
  preshared_key: preshared_key=
  netmask: 255.255.255.0

  # optional (only for esp-idf framework)
  keepalive: 25
```
