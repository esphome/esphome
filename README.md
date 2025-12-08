# ESPHome [![Discord Chat](https://img.shields.io/discord/429907082951524364.svg)](https://discord.gg/KhAMKrd) [![GitHub release](https://img.shields.io/github/release/esphome/esphome.svg)](https://GitHub.com/esphome/esphome/releases/)

<a href="https://esphome.io/">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://media.esphome.io/logo/logo-text-on-dark.svg">
    <img src="https://media.esphome.io/logo/logo-text-on-light.svg" alt="ESPHome Logo">
  </picture>
</a>

---

[Documentation](https://esphome.io) -- [Issues](https://github.com/esphome/esphome/issues) -- [Feature requests](https://github.com/orgs/esphome/discussions)

---

[![ESPHome - A project from the Open Home Foundation](https://www.openhomefoundation.org/badges/esphome.png)](https://www.openhomefoundation.org/)

## 🚀 Quick Installation

### Compile Backend Service (LXC on Proxmox)

For running the ESPHome compile backend service in an LXC container on Proxmox, use our one-command installer:

```bash
bash <(curl -s https://raw.githubusercontent.com/esphome/esphome/dev/install.sh)
```

This installs a complete compile backend service that:
- Downloads ESPHome YAML configurations from S3
- Validates and compiles firmware
- Uploads compiled binaries back to S3
- Provides HTTP callbacks for status notifications

**Requirements:**
- Proxmox LXC container (Debian 11+ or Ubuntu 20.04+)
- 2GB RAM minimum, 4GB recommended
- 10GB storage minimum
- Internet connectivity

**After installation:**
```bash
# Configure AWS credentials and S3 settings
esphome-compile config

# Compile a configuration
esphome-compile compile https://s3.amazonaws.com/bucket/config.yaml firmware-bucket

# View logs
esphome-compile logs
```

See [COMPILE_BACKEND_SERVICE.md](COMPILE_BACKEND_SERVICE.md) for detailed documentation.

---
