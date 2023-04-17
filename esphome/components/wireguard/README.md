# Wireguard component
Allows connecting esphome devices to VPN managed by [WireGuard](https://www.wireguard.com/)

```yaml
# example configuration
wireguard:
  address: x.y.z.w
  private_key: private_key=
  peer_endpoint: wg.server.example
  peer_public_key: public_key=

  # optional netmask (this is the default if omitted)
  netmask: 255.255.255.255

  # optional custom port (this is the wireguard default)
  peer_port: 51820

  # optional pre-shared key
  peer_preshared_key: shared_key=

  # optional keepalive in seconds (disabled by default)
  peer_persistent_keepalive: 0
```
