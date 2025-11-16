---
description: "Security best practices for ESPHome devices and networks."
title: "Security Best Practices"
params:
  seo:
    description: Security best practices for ESPHome devices and networks.
    image: shield-alt.svg
---

This guide provides security recommendations for ESPHome users to help protect their devices and networks.

## Threat Model

ESPHome is designed for deployment on **trusted networks** such as home or business networks. The security model assumes:

- Devices are protected by network perimeter security (firewalls, network segmentation, VLANs)
- Devices are **not directly exposed** to untrusted networks or the internet
- Physical access to devices is controlled

ESPHome devices should not be considered hardened for hostile network environments. If you need to deploy devices in hostile or untrusted environments, additional security measures beyond ESPHome's built-in features are required.

## Core Security Features

ESPHome provides three primary security features that should **always** be enabled:

### 1. API Encryption

The [native API](/components/api) is the primary communication method between ESPHome devices and Home Assistant or other clients.

**Enable API encryption:**

```yaml
api:
  encryption:
    key: !secret device_name_api_key

```

**Best practices:**

- Generate a unique encryption key for each device - see the [API component documentation](/components/api#configuration-variables) for an on-demand key generator
- Store keys in `secrets.yaml` (never commit this file to version control)
- Never reuse encryption keys across devices
- If a device is compromised, regenerate its key immediately

**Without API encryption:** Anyone on your local network can:

- Read all sensor data from your devices
- Control switches, lights, and other entities
- Execute services on your devices
- Potentially extract sensitive information

### 2. Web Server Authentication

If you enable the [web server](/components/web_server) for device monitoring and control, always set a password:

```yaml
web_server:
  port: 80
  auth:
    username: !secret device_name_web_username
    password: !secret device_name_web_password

```

**Best practices:**

- Use strong, unique passwords
- Store credentials in `secrets.yaml`
- Consider disabling the web server entirely if you don't need it
- If you only need logs, use Home Assistant or the native API instead

**Without web server authentication:** Anyone on your local network can:

- View device status and sensor data
- Control switches, buttons, and other entities via the web interface
- Potentially interfere with device operation

### 3. OTA Password Protection

[OTA (Over-The-Air)](/components/ota) updates allow you to update firmware wirelessly. Protect this with a password:

```yaml
ota:
  - platform: esphome
    password: !secret device_name_ota_password

```

**Best practices:**

- Use strong, unique passwords
- Store passwords in `secrets.yaml`
- Never use the same OTA password across multiple devices
- Rotate passwords periodically or after suspected compromise

**Without OTA password:** Anyone on your local network can:

- Upload malicious firmware to your devices
- Completely compromise device functionality
- Use your devices as a pivot point to attack other network resources

## Network Security

### Network Segmentation

**Important consideration:** ESPHome devices use mDNS for discovery, which does not work across VLANs. For most home users, placing ESPHome devices on the **same network as Home Assistant** is the simplest and recommended approach.

<details>
<summary>Advanced: VLAN Isolation (for advanced users only)</summary>

**For advanced users wanting VLAN isolation:**

The recommended approach is to connect Home Assistant to **both networks** (dual-homing) rather than using an mDNS reflector, which is unreliable:

```text

Internet → Firewall → VLAN 10 (Trusted - Home Assistant management interface)
                   → VLAN 30 (IoT - ESPHome devices)
                   → VLAN 20 (Guest Network)

Home Assistant with two network interfaces:
  - eth0 or wlan0: VLAN 10 (192.168.10.x) - Management and user access
  - eth1 or wlan1: VLAN 30 (192.168.30.x) - IoT device communication

```

**Dual-homing setup:**

- Home Assistant can discover ESPHome devices via mDNS on VLAN 30
- User access to Home Assistant remains on VLAN 10
- No unreliable mDNS reflector needed
- Requires Home Assistant host with two network interfaces (physical, USB Ethernet, or VLANs on single interface)

**Alternative: Static IP without mDNS:**

- Configure ESPHome devices with static IPs
- Disable mDNS on devices
- Manually configure device addresses in Home Assistant
- More maintenance overhead but works with single interface

</details>

### WiFi Security

For full WiFi configuration options, see the [WiFi component](/components/wifi) documentation.

**Prefer Ethernet when possible:**

For devices that support it, use Ethernet instead of WiFi for better security and reliability:

- No wireless encryption vulnerabilities
- Better performance and lower latency
- Not susceptible to WiFi attacks (deauth, jamming, etc.)
- Reduces wireless network congestion

For a list of supported Ethernet components and compatible hardware, see the [Ethernet component documentation](/components/ethernet).

**WiFi Configuration ([WiFi component](/components/wifi)):**

```yaml
wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

  # Set minimum WiFi security - reject connections to networks with weaker security
  # Default is WPA2 on ESP32, WPA on ESP8266 (will change to WPA2 in 2026.6.0)
  min_auth_mode: WPA2  # Or WPA3 for ESP32 if all your networks support it

  # Optional: Fallback AP with password (only if needed)
  ap:
    ssid: "Fallback-AP"
    password: !secret fallback_password

```

**Best practices:**

- **ESP32 users:** The default `min_auth_mode: WPA2` is secure and allows WPA2 and WPA3 networks - only set `min_auth_mode: WPA3` if you want to restrict to WPA3-only networks
- **ESP8266 users:** Explicitly set `min_auth_mode: WPA2` to avoid the insecure WPA default
- **WPA (TKIP) has known vulnerabilities** - only use if you have a legacy router that can't be upgraded
- Never use open or WEP-encrypted networks
- Use WPA2 (minimum) or WPA3 (recommended) on your router
- Use strong WiFi passwords
- Disable WPS on your router (vulnerable to brute force attacks)
- Consider hiding SSID broadcast (provides limited security but reduces visibility)

### mDNS Security

ESPHome uses [mDNS](/components/mdns) for device discovery. Be aware:

- mDNS broadcasts device names on your local network
- Malicious actors on the same network can discover devices

**Disabling mDNS is NOT recommended** for most users as it makes devices very difficult to manage. You would need to manually track static IP addresses for all devices and reconfigure Home Assistant if IPs change. Only disable mDNS if you require extreme security and are willing to accept the significant management overhead.

<details>
<summary>Advanced: Disabling mDNS (not recommended for most users)</summary>

```yaml
# Only for extreme security requirements - makes management very difficult
wifi:
  manual_ip:
    static_ip: 192.168.30.10
    gateway: 192.168.30.1
    subnet: 255.255.255.0

# Disable mDNS (requires manually configuring static IPs in Home Assistant)
mdns:
  disabled: true

```

</details>

## Physical Security

### Device Access

Physical access to an ESPHome device allows an attacker to:

- Flash new firmware via USB/serial connection
- Extract encryption keys and passwords from flash memory
- Replace the device entirely

**Mitigation strategies:**

- Install devices in secure locations (locked cabinets, above ceiling tiles)
- Use tamper-evident seals on enclosures
- Consider devices installed in public areas as potentially compromised

### USB/Serial Protection

<details>
<summary>⚠️ WARNING: PERMANENT AND IRREVERSIBLE - Click to expand only if you need extreme security</summary>

**WARNING: The following methods are PERMANENT and IRREVERSIBLE. Do not do this unless you fully understand the consequences.**

If you have extreme security requirements, you can physically disable USB/serial interfaces after initial deployment:

- Fill USB ports with epoxy (PERMANENT - device cannot be recovered if it fails)
- Cut serial header pins (PERMANENT - device cannot be reflashed via serial)
- Disable bootloader access via UART using eFuses on ESP32 (PERMANENT and IRREVERSIBLE - blocks all serial flashing)
- Use devices in hard-to-access locations (reversible)

**Important considerations:**

- Once you epoxy, cut pins, or burn eFuses, the device can ONLY be updated via OTA
- If OTA fails or the device becomes unresponsive, the device is permanently bricked
- You will not be able to troubleshoot connection issues or recover from bad firmware
- eFuses cannot be reset - once blown, they are permanent for the life of the chip
- This is only appropriate for extremely high-security environments where physical access is a critical threat

</details>

**For most users:** Simply installing devices in secure, hard-to-access locations provides sufficient physical security without the risk of permanently bricking your devices.

## Secrets Management

### Using secrets.yaml

Use `secrets.yaml` to avoid storing sensitive information in your configuration files, especially if you share your configs in a public Git repository:

```yaml
# Example: device1.yaml
api:
  encryption:
    key: !secret device1_api_key

ota:
  - platform: esphome
    password: !secret device1_ota_password

```

**secrets.yaml:**

```yaml
# Each device should have unique keys and passwords
device1_api_key: "uKh1234567890abcdefghijklmnopqrstuvwxyz="
device1_ota_password: "strong-unique-password-device1"
device1_web_username: "device1_admin"
device1_web_password: "strong-unique-web-password-device1"

device2_api_key: "aBc9876543210xyzqrstuvwxyzabcdefghijkl="
device2_ota_password: "strong-unique-password-device2"
device2_web_username: "device2_admin"
device2_web_password: "strong-unique-web-password-device2"

# WiFi credentials can be shared across devices
wifi_ssid: "YourNetworkName"
wifi_password: "your-wifi-password"

```

**Important:** Even when using `secrets.yaml`, **each device must have unique API encryption keys, OTA passwords, and web server credentials**. Never reuse these across devices. Only WiFi credentials can be shared.

### Version Control

<details>
<summary>If using Git or other version control</summary>

**Add to `.gitignore`:**

```txt
secrets.yaml
*.backup
```

**Verify secrets.yaml is not tracked:**

```bash
git status  # secrets.yaml should not appear
git log --all --full-history -- secrets.yaml  # Should return nothing
```

**If you accidentally committed secrets:**

1. Rotate all compromised credentials immediately
1. Use `git filter-branch` or `BFG Repo-Cleaner` to remove from history
1. Force-push the cleaned repository
1. Notify anyone who cloned the repository

</details>

## Update Management

### Keep ESPHome Updated

Security vulnerabilities are discovered and fixed regularly. Keep your ESPHome installation up to date by following the [installation instructions](/guides/installing_esphome).

**Best practices:**

- Subscribe to ESPHome release notifications on GitHub
- Review changelogs for security fixes
- Test updates in a non-production environment first
- Update devices regularly (monthly rolling release cycle)

### Firmware Updates

**Verify you're updating the correct device:**

- Check device hostname and IP address before OTA update
- Use unique, descriptive device names
- Maintain an inventory of devices and their configurations

**OTA security:**

- OTA updates are performed over your local network
- Enable OTA password protection (see above)
- Monitor device logs during updates for unexpected behavior

## Logging and Monitoring

### Sensitive Data in Logs

**Be cautious about what you log.** See the [logger component](/components/logger) for more details on log configuration.

```yaml
logger:
  level: INFO  # Don't use DEBUG in production
  logs:
    # Reduce verbosity for components that might log sensitive data
    wifi: WARN
    api: WARN

```

**Avoid logging:**

- WiFi passwords (may be logged at DEBUG/VERBOSE levels - keep log level at WARNING or higher)
- API encryption keys
- User credentials
- Personal information from sensors (e.g., GPS coordinates)

### Log Review

Regularly review device logs for:

- Unexpected API connections
- Failed authentication attempts
- Unusual sensor readings or component behavior
- Memory or crash dumps (may contain sensitive data)

Access logs via:

- `esphome logs <config>.yaml` command
- ESPHome Device Builder web interface
- Serial console (USB connection)

## Specific Component Security

### WiFi Fallback Hotspot

The [WiFi component](/components/wifi) can create a fallback AP if it can't connect to WiFi (when `ap:` is configured):

```yaml
wifi:
  # ... your normal wifi config ...
  ap:
    ssid: "Device-Fallback"
    password: !secret device_name_fallback_password  # ALWAYS SET THIS

```

**Without a password:** Anyone nearby can connect when your WiFi is down and potentially:

- Access the device's web server
- Flash new firmware via OTA
- Extract configuration data

**Best practices:**

- Always set a fallback AP password
- Use strong, unique passwords
- Consider disabling fallback AP in production by removing the `ap:` section entirely

### MQTT

If using [MQTT](/components/mqtt) instead of the native API:

```yaml
mqtt:
  broker: !secret mqtt_broker
  username: !secret mqtt_username
  password: !secret mqtt_password
  # For ESP8266: Use TLS with SSL fingerprints
  # ssl_fingerprints:
  #   - "SHA1_FINGERPRINT_HERE"
  # For ESP32 with esp-idf: Use TLS with certificate authority
  # certificate_authority: ca_cert.pem

```

**Best practices:**

- Enable MQTT authentication on your broker
- Use TLS encryption if possible (check broker support)
- Use unique MQTT credentials per device
- Segment MQTT topics by device/function

### External Components

Custom/external components are **out of scope** for ESPHome security support:

```yaml
external_components:
  - source: github://someone/custom-component

```

**Risks:**

- May contain vulnerabilities or malicious code
- Not reviewed by ESPHome maintainers
- May not follow security best practices

**Best practices:**

- Only use external components from trusted sources
- Review source code before using
- Keep external components updated
- Consider the maintenance status and community trust

## Secure Configuration Examples

### Minimal Secure Configuration

```yaml
esphome:
  name: secure-device
  friendly_name: Secure Device

esp32:
  board: esp32dev

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

  # Fallback hotspot with password
  ap:
    ssid: "Secure-Device-Fallback"
    password: !secret secure_device_fallback_password

# API with encryption (REQUIRED)
api:
  encryption:
    key: !secret secure_device_api_key

# OTA with password (REQUIRED)
ota:
  - platform: esphome
    password: !secret secure_device_ota_password

# Disable web server if not needed
# web_server:
#   port: 80
#   auth:
#     username: !secret secure_device_web_username
#     password: !secret secure_device_web_password

logger:
  level: INFO

```

### Production-Grade Secure Configuration

```yaml
esphome:
  name: production-device
  friendly_name: Production Device

esp32:
  board: esp32dev
  framework:
    type: esp-idf  # ESP-IDF generally has better security updates

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

  # Use static IP to reduce mDNS dependency
  manual_ip:
    static_ip: !secret production_device_ip
    gateway: !secret gateway_ip
    subnet: 255.255.255.0

  # Disable fallback AP in production - remove ap: section entirely

# API with encryption
api:
  encryption:
    key: !secret production_device_api_key
  # Optional: Restrict to specific Home Assistant instance
  # reboot_timeout: 15min

# OTA with password
ota:
  - platform: esphome
    password: !secret production_device_ota_password
  # Optional: Require safe_mode for troubleshooting
  # safe_mode: true

# Web server disabled (use Home Assistant for monitoring)
# web_server:

logger:
  level: INFO
  logs:
    wifi: WARN
    api: WARN

# Optional: Disable mDNS for additional security
# mdns:
#   disabled: true

```

## Compliance and Regulations

Depending on your jurisdiction and use case, you may need to comply with:

- **GDPR** (EU): If devices collect personal data
- **CCPA** (California): Consumer privacy protection
- **HIPAA** (USA): If used in healthcare settings
- **Industry standards**: IEC 62443 (industrial automation), UL 2900 (IoT security)

**ESPHome does not guarantee compliance** with any specific regulations. Consult legal and compliance experts for your specific requirements.

## Incident Response

### If You Suspect a Compromise

1. **Isolate** the device immediately (disconnect from network/power)
1. **Document** what you observed (logs, unusual behavior, timestamps)
1. **Investigate** other devices on the same network
1. **Rotate credentials**:

   - API encryption keys
   - OTA passwords
   - Web server credentials
   - WiFi passwords (if device had access)

1. **Flash fresh firmware** (via USB/serial, not OTA)
1. **Monitor** for continued suspicious activity

### Reporting Security Vulnerabilities

If you discover a security vulnerability in ESPHome itself:

- **DO NOT** create a public GitHub issue
- See the [ESPHome Security Policy](https://github.com/esphome/esphome/security) for reporting guidelines

## Additional Resources

- [Home Assistant Security Checklist](https://www.home-assistant.io/docs/configuration/securing/) - Complementary security guidance
- [OWASP IoT Security](https://owasp.org/www-project-internet-of-things/) - General IoT security best practices

## Disclaimer

As an open source project, ESPHome is provided "as is" without any security guarantees or warranties. Users are responsible for:

- Properly configuring security features
- Maintaining network and physical security
- Keeping software updated
- Implementing appropriate security controls for their environment

Following these best practices significantly improves security but cannot eliminate all risks. Security is a shared responsibility between the ESPHome project and its users.
