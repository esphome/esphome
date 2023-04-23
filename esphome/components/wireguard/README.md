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

If you give an `id` to the wireguard component you can manually create some sensors:

```yaml
wireguard:
  id: vpn
  [...]

# a binary sensor to check if the remote peer is online
binary_sensor:
  - platform: template
    name: 'WireGuard Status'
    device_class: connectivity
    lambda: |-
      return id(vpn).is_peer_up();

# a sensor to retrive the timestamp of the latest handshake
sensor:
  - platform: template
    name: 'WireGuard Latest Handshake'
    device_class: timestamp
    lambda: |-
      static time_t latest_handshake;
      latest_handshake = id(vpn).get_latest_handshake();
      return (latest_handshake > 0) ? latest_handshake : NAN;
```

Integrated sensors are under development...
