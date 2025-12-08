# ESPHome Compile Backend Service - Proxmox LXC Quick Deploy

## 🚀 One-Command Installation

For a Proxmox LXC container running Debian 11+ or Ubuntu 20.04+:

```bash
bash <(curl -s https://raw.githubusercontent.com/esphome/esphome/dev/install.sh)
```

## 📋 What Gets Installed

✅ Complete ESPHome environment with all dependencies  
✅ Python 3.11+ with virtual environment  
✅ PlatformIO for firmware compilation  
✅ AWS S3 integration (boto3)  
✅ Systemd service configuration  
✅ Management CLI (`esphome-compile`)  
✅ Automatic updates support  

## 🎯 Use Cases

- **Automated Firmware Compilation**: Download configs from S3, compile, upload results
- **CI/CD Integration**: Webhook-based compilation pipelines
- **Multi-Device Management**: Batch compilation for multiple devices
- **Custom Firmware Distribution**: Automated firmware builds and distribution

## 💻 System Requirements

| Resource | Minimum | Recommended |
|----------|---------|-------------|
| CPU | 1 core | 2-4 cores |
| RAM | 2GB | 4GB |
| Disk | 10GB | 20GB |
| OS | Debian 11+ / Ubuntu 20.04+ | Debian 12 / Ubuntu 22.04 |

## 📦 Quick Setup (3 Steps)

### 1. Create LXC Container in Proxmox

**Via Web UI:**
- Container ID: `100` (or your choice)
- Hostname: `esphome-compile`
- Template: Debian 12 or Ubuntu 22.04
- Disk: 20GB
- CPU: 2 cores
- Memory: 4096MB
- Network: Bridged (DHCP or Static IP)

**Via CLI:**
```bash
pct create 100 \
  local:vztmpl/debian-12-standard_12.2-1_amd64.tar.zst \
  --hostname esphome-compile \
  --memory 4096 \
  --swap 512 \
  --cores 2 \
  --rootfs local-lvm:20 \
  --net0 name=eth0,bridge=vmbr0,ip=dhcp \
  --unprivileged 1 \
  --features nesting=1

pct start 100
```

### 2. Run Installer in Container

```bash
# Enter container
pct enter 100

# Run installer
bash <(curl -s https://raw.githubusercontent.com/esphome/esphome/dev/install.sh)
```

### 3. Configure and Test

```bash
# Configure AWS credentials
esphome-compile config

# Run tests
esphome-compile test

# Compile a configuration
esphome-compile compile https://s3.amazonaws.com/bucket/config.yaml firmware-bucket
```

## 🛠️ Management Commands

| Command | Description |
|---------|-------------|
| `esphome-compile compile <url> <bucket>` | Compile configuration from S3 |
| `esphome-compile config` | Edit configuration file |
| `esphome-compile logs` | View service logs |
| `esphome-compile status` | Check service status |
| `esphome-compile update` | Update ESPHome |
| `esphome-compile test` | Run tests |
| `esphome-compile help` | Show all commands |

## ⚙️ Configuration

Configuration file: `/opt/esphome/compile-backend.env`

**Required:**
```bash
AWS_ACCESS_KEY_ID=your-key
AWS_SECRET_ACCESS_KEY=your-secret
S3_UPLOAD_BUCKET=firmware-bucket
```

**Optional:**
```bash
S3_UPLOAD_PREFIX=firmware/
STATUS_CALLBACK_URL=https://api.example.com/status
COMPLETION_CALLBACK_URL=https://api.example.com/complete
AWS_DEFAULT_REGION=us-east-1
LOG_LEVEL=INFO
```

## 🔄 Workflow

```
┌─────────────────────────────────────────────────────────────┐
│  1. Download YAML from S3                                   │
│     ↓                                                        │
│  2. Validate configuration                                  │
│     ↓                                                        │
│  3. Compile firmware (ESPHome + PlatformIO)                 │
│     ↓                                                        │
│  4. Upload firmware.factory.bin to S3                       │
│     ↓                                                        │
│  5. Send completion callback                                │
└─────────────────────────────────────────────────────────────┘
```

## 📖 Example Usage

### Basic Compilation

```bash
esphome-compile compile \
  https://s3.amazonaws.com/configs/esp32-device.yaml \
  firmware-output-bucket
```

### With Callbacks

```bash
esphome-compile compile \
  https://s3.amazonaws.com/configs/device.yaml \
  firmware-bucket \
  --status-callback https://api.example.com/webhook/status \
  --completion-callback https://api.example.com/webhook/done
```

### Custom S3 Endpoint (MinIO, DigitalOcean Spaces)

Edit config:
```bash
esphome-compile config
```

Set:
```bash
S3_ENDPOINT=https://nyc3.digitaloceanspaces.com
```

## 🔒 Security Best Practices

✅ Service runs as non-root user (`esphome`)  
✅ Unprivileged LXC container recommended  
✅ Use HTTPS for all S3 and callback URLs  
✅ Restrict AWS credentials to minimum required permissions  
✅ Enable resource limits (CPU, Memory) in systemd service  
✅ Regular updates with `esphome-compile update`  

**AWS IAM Policy Example:**
```json
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Effect": "Allow",
      "Action": [
        "s3:GetObject"
      ],
      "Resource": "arn:aws:s3:::config-bucket/*"
    },
    {
      "Effect": "Allow",
      "Action": [
        "s3:PutObject"
      ],
      "Resource": "arn:aws:s3:::firmware-bucket/*"
    }
  ]
}
```

## 🐛 Troubleshooting

### Installation fails
```bash
apt-get update && apt-get upgrade -y
# Re-run installer
```

### Cannot access S3
```bash
# Test network
ping google.com

# Test DNS
nslookup s3.amazonaws.com

# Check credentials
esphome-compile config
```

### Out of memory during compilation
```bash
# Increase container RAM in Proxmox to 4GB+
# Or reduce concurrent compilations
```

### Build artifacts filling disk
```bash
# Clean old builds
sudo -u esphome rm -rf /var/lib/esphome/*/build

# Check disk usage
df -h
```

## 📊 Monitoring

```bash
# Real-time logs
esphome-compile logs

# System resources
htop

# Service status
systemctl status esphome-compile-backend

# Recent compilation logs
journalctl -u esphome-compile-backend -n 100
```

## 🔄 Updates

```bash
# Update ESPHome
esphome-compile update

# Update system packages
apt-get update && apt-get upgrade -y

# Update installer script (if available)
bash <(curl -s https://raw.githubusercontent.com/esphome/esphome/dev/install.sh)
```

## 🗂️ Directory Structure

```
/opt/esphome/
├── esphome/              # ESPHome source
├── venv/                 # Python virtualenv
├── compile-backend.env   # Configuration
└── service-wrapper.sh    # Service wrapper

/var/lib/esphome/         # Build workspace
/var/log/esphome/         # Logs (via journald)
/usr/local/bin/esphome-compile  # CLI tool
```

## 🔗 API Integration

### Callback Payload Examples

**Validation Success:**
```json
{
  "config_url": "https://s3.../config.yaml",
  "stage": "validation",
  "success": true,
  "output": "Configuration valid!"
}
```

**Compilation Complete:**
```json
{
  "config_url": "https://s3.../config.yaml",
  "stage": "complete",
  "success": true,
  "uploaded_key": "firmware/device123.bin",
  "bucket": "firmware-bucket"
}
```

## 📚 Documentation

- **Detailed Installation**: [LXC_INSTALLATION.md](LXC_INSTALLATION.md)
- **Service Documentation**: [COMPILE_BACKEND_SERVICE.md](COMPILE_BACKEND_SERVICE.md)
- **Quick Start**: [QUICKSTART_COMPILE_SERVICE.md](QUICKSTART_COMPILE_SERVICE.md)
- **ESPHome Docs**: https://esphome.io

## 💬 Support

- GitHub Issues: https://github.com/esphome/esphome/issues
- Discord: https://discord.gg/KhAMKrd
- Forum: https://community.home-assistant.io/c/esphome

## 📄 License

MIT License - See LICENSE file for details

---

**Similar to Proxmox Helper Scripts?** This installer follows the same pattern as tteck's Proxmox scripts - one command, fully automated, best practices included.
