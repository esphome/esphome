# ESPHome Compile Backend Service - LXC Installation Guide

## Overview

This guide helps you set up the ESPHome compile backend service in a Proxmox LXC container. The service automatically downloads YAML configurations from S3, validates them, compiles firmware, and uploads the results back to S3.

## Table of Contents

- [Prerequisites](#prerequisites)
- [LXC Container Setup](#lxc-container-setup)
- [One-Command Installation](#one-command-installation)
- [Configuration](#configuration)
- [Usage](#usage)
- [Advanced Configuration](#advanced-configuration)
- [Troubleshooting](#troubleshooting)
- [Maintenance](#maintenance)

## Prerequisites

### Proxmox Requirements
- Proxmox VE 7.0 or later
- Available storage for LXC container
- Network connectivity for the container

### Container Resources (Recommended)
- **CPU**: 2 cores minimum, 4 cores recommended
- **RAM**: 2GB minimum, 4GB recommended
- **Disk**: 10GB minimum, 20GB recommended
- **Network**: Bridged networking with internet access

### External Services
- AWS S3 (or S3-compatible service) for storing configurations and firmware
- Optional: HTTP endpoints for status callbacks

## LXC Container Setup

### Option 1: Using Proxmox Web UI

1. **Create LXC Container**
   - Navigate to your Proxmox node in the web interface
   - Click "Create CT" (Create Container)
   
2. **General Settings**
   - **Hostname**: `esphome-compile` (or your preferred name)
   - **Password**: Set a root password
   - **Unprivileged container**: Yes (recommended for security)

3. **Template**
   - **Template**: Select "Debian 12" or "Ubuntu 22.04"
   - Download template if not already available

4. **Root Disk**
   - **Disk size**: 20 GB (minimum 10GB)
   - **Storage**: Select your preferred storage

5. **CPU**
   - **Cores**: 2 (minimum 1, recommended 4)

6. **Memory**
   - **Memory (RAM)**: 4096 MB (minimum 2048 MB)
   - **Swap**: 512 MB

7. **Network**
   - **Bridge**: vmbr0 (or your network bridge)
   - **IPv4**: DHCP or static IP
   - **IPv6**: DHCP or disable

8. **DNS**
   - Use defaults or specify your DNS servers

9. **Confirm and Create**
   - Review settings and click "Finish"
   - Start the container

### Option 2: Using Proxmox CLI

```bash
# On your Proxmox host, run:
pct create 100 \
  local:vztmpl/debian-12-standard_12.2-1_amd64.tar.zst \
  --hostname esphome-compile \
  --memory 4096 \
  --swap 512 \
  --cores 2 \
  --rootfs local-lvm:20 \
  --net0 name=eth0,bridge=vmbr0,ip=dhcp \
  --unprivileged 1 \
  --features nesting=1 \
  --password

# Start the container
pct start 100

# Enter the container
pct enter 100
```

**Note**: Replace `100` with your desired container ID, and adjust storage paths as needed.

## One-Command Installation

Once your LXC container is running, install the ESPHome compile backend service:

### Step 1: Enter the Container

```bash
# From Proxmox host
pct enter 100  # Replace 100 with your container ID

# Or SSH into the container if you've set up SSH
ssh root@<container-ip>
```

### Step 2: Run the Installer

```bash
bash <(curl -s https://raw.githubusercontent.com/esphome/esphome/dev/install.sh)
```

The installer will:
- Update system packages
- Install all required dependencies (Python, build tools, etc.)
- Create a dedicated `esphome` user
- Clone the ESPHome repository
- Set up Python virtual environment
- Install ESPHome and all dependencies
- Configure PlatformIO
- Create systemd service
- Install management CLI tools

**Installation takes approximately 10-15 minutes** depending on your internet speed and container resources.

### Step 3: Verify Installation

```bash
esphome-compile help
```

You should see the management CLI help output.

## Configuration

### Configure AWS Credentials and S3 Settings

```bash
esphome-compile config
```

This opens the configuration file in your default editor (nano). Set the following:

```bash
# AWS Configuration
AWS_ACCESS_KEY_ID=your-access-key-id
AWS_SECRET_ACCESS_KEY=your-secret-access-key
AWS_DEFAULT_REGION=us-east-1

# S3 Configuration
S3_UPLOAD_BUCKET=your-firmware-bucket
S3_UPLOAD_PREFIX=firmware/

# Optional: Custom S3 endpoint (for S3-compatible services)
# S3_ENDPOINT=https://s3.example.com
# S3_PATH_STYLE=true
# S3_UNSIGNED=false

# HTTP Callbacks (optional)
STATUS_CALLBACK_URL=https://api.example.com/status
COMPLETION_CALLBACK_URL=https://api.example.com/complete

# Logging
LOG_LEVEL=INFO
```

**Required Settings:**
- `AWS_ACCESS_KEY_ID` - Your AWS access key
- `AWS_SECRET_ACCESS_KEY` - Your AWS secret key
- `S3_UPLOAD_BUCKET` - S3 bucket where compiled firmware will be uploaded

**Optional Settings:**
- `S3_UPLOAD_PREFIX` - Prefix for uploaded files (e.g., `firmware/`)
- `STATUS_CALLBACK_URL` - HTTP endpoint for validation/compile status
- `COMPLETION_CALLBACK_URL` - HTTP endpoint for completion notification
- `AWS_DEFAULT_REGION` - AWS region (default: `us-east-1`)
- `LOG_LEVEL` - Logging level: DEBUG, INFO, WARNING, ERROR

Save and exit (Ctrl+X, then Y, then Enter in nano).

## Usage

### Basic Compilation

Compile an ESPHome configuration from S3:

```bash
esphome-compile compile https://s3.amazonaws.com/my-bucket/configs/device.yaml my-firmware-bucket
```

### With Custom Options

```bash
esphome-compile compile \
  https://s3.amazonaws.com/my-bucket/configs/device.yaml \
  my-firmware-bucket \
  --upload-prefix firmware/ \
  --status-callback https://api.example.com/status \
  --completion-callback https://api.example.com/complete
```

### View Logs

```bash
# Follow logs in real-time
esphome-compile logs

# Or use journalctl directly
journalctl -u esphome-compile-backend -f
```

### Check Service Status

```bash
esphome-compile status
```

### Run Tests

```bash
esphome-compile test
```

### Update ESPHome

```bash
esphome-compile update
```

## Advanced Configuration

### Custom S3 Endpoint (MinIO, DigitalOcean Spaces, etc.)

Edit the configuration:

```bash
esphome-compile config
```

Add or modify:

```bash
S3_ENDPOINT=https://nyc3.digitaloceanspaces.com
S3_PATH_STYLE=false
AWS_DEFAULT_REGION=us-east-1
```

### Using the Service Programmatically

You can integrate the service into your own scripts:

```bash
#!/bin/bash
# Your custom automation script

CONFIG_URL="https://s3.amazonaws.com/bucket/device-${DEVICE_ID}.yaml"
BUCKET="firmware-bucket"

# Run compilation
sudo -u esphome /opt/esphome/service-wrapper.sh "$CONFIG_URL" "$BUCKET"
```

### Systemd Service (Advanced)

The systemd service is created but requires manual configuration for automatic operation. For most use cases, use the `esphome-compile` CLI instead.

To create a custom systemd service instance:

```bash
# Create a custom service file
sudo cp /etc/systemd/system/esphome-compile-backend.service \
        /etc/systemd/system/esphome-compile-backend@.service

# Edit to parameterize the ExecStart line
# Then enable and start with:
sudo systemctl enable esphome-compile-backend@instance1
sudo systemctl start esphome-compile-backend@instance1
```

## Troubleshooting

### Installation Issues

**Problem**: Installation fails with package errors

**Solution**: 
```bash
apt-get update
apt-get upgrade -y
# Then re-run the installer
```

**Problem**: Python version too old

**Solution**: The installer uses the system Python 3. Ensure your container template is Debian 11+ or Ubuntu 20.04+.

### Compilation Issues

**Problem**: "Unable to locate credentials"

**Solution**: 
1. Verify AWS credentials are set in the config file
2. Check credentials are valid: `aws s3 ls` (if AWS CLI is installed)
3. For public S3 access, use `--s3-unsigned` flag

**Problem**: "HTTP Error 403: FORBIDDEN" when downloading

**Solution**: 
1. Ensure the S3 object has public-read ACL
2. Or configure appropriate AWS credentials
3. Check bucket permissions

**Problem**: "Unable to locate firmware.factory.bin"

**Solution**: 
1. Check that the YAML specifies a valid platform (esp32, esp8266, rp2040)
2. Review compilation logs: `esphome-compile logs`
3. Ensure the workspace has sufficient disk space

### Network Issues

**Problem**: Cannot access S3

**Solution**: 
1. Verify container has internet access: `ping google.com`
2. Check DNS resolution: `nslookup s3.amazonaws.com`
3. Verify firewall rules on Proxmox host

**Problem**: Callbacks timing out

**Solution**: 
1. Verify callback URLs are accessible from the container
2. Test with curl: `curl -X POST https://api.example.com/status -d '{}'`
3. Check for firewall rules blocking outbound HTTPS

### Resource Issues

**Problem**: Out of memory during compilation

**Solution**: 
1. Increase container RAM to 4GB or more
2. Monitor with: `free -h`
3. Reduce concurrent compilations

**Problem**: Out of disk space

**Solution**: 
```bash
# Check disk usage
df -h

# Clean build artifacts
rm -rf /var/lib/esphome/*/build

# Increase container disk size in Proxmox
```

## Maintenance

### Regular Updates

```bash
# Update ESPHome to the latest version
esphome-compile update

# Update system packages
apt-get update && apt-get upgrade -y
```

### Backup

Important directories to backup:
- `/opt/esphome/compile-backend.env` - Configuration file
- `/opt/esphome/esphome/` - ESPHome repository (optional, can be re-cloned)

```bash
# Backup configuration
cp /opt/esphome/compile-backend.env /root/compile-backend.env.backup

# Or use Proxmox backup
# In Proxmox web UI: Backup > Backup now
```

### Monitoring

Monitor service health:

```bash
# Check if service is running
systemctl status esphome-compile-backend

# Check recent logs
journalctl -u esphome-compile-backend -n 100

# Monitor resource usage
top
htop  # If installed
```

### Log Rotation

Logs are managed by systemd's journal. Configure retention:

```bash
# Edit journal configuration
nano /etc/systemd/journald.conf

# Set retention (e.g., 1 week)
SystemMaxUse=500M
MaxRetentionSec=1week

# Restart journald
systemctl restart systemd-journald
```

### Cleanup

Remove old build artifacts:

```bash
# As root or esphome user
sudo -u esphome bash -c 'rm -rf /var/lib/esphome/*/build'

# This keeps cached PlatformIO packages but removes build outputs
```

## Directory Structure

After installation:

```
/opt/esphome/
├── esphome/                    # ESPHome repository
│   ├── script/                 # Contains compile_backend_service.py
│   ├── esphome/                # ESPHome Python package
│   └── ...
├── venv/                       # Python virtual environment
├── compile-backend.env         # Your configuration (create from .example)
├── compile-backend.env.example # Configuration template
├── service-wrapper.sh          # Service wrapper script
└── README.md                   # Quick reference

/var/lib/esphome/               # Working directory for builds
/var/log/esphome/               # Log files
/usr/local/bin/esphome-compile  # Management CLI
```

## Security Considerations

1. **Run as Non-Root**: The service runs as the `esphome` user (not root)
2. **Credential Security**: 
   - Never commit credentials to version control
   - Restrict access to `/opt/esphome/compile-backend.env`
   - Consider using IAM roles if running on AWS EC2
3. **Network Security**: 
   - Use HTTPS for S3 and callback URLs
   - Consider VPN or private networking for sensitive deployments
4. **Input Validation**: 
   - The service executes YAML configurations - only process trusted sources
   - Consider implementing allowlist for config URLs
5. **Resource Limits**: 
   - Systemd service includes CPU and memory limits
   - Adjust in `/etc/systemd/system/esphome-compile-backend.service` if needed

## Performance Tuning

### For High-Volume Operations

1. **Increase Resources**:
   ```bash
   # In Proxmox, increase container resources
   # CPU: 4+ cores
   # RAM: 8GB+
   # Disk: NVMe/SSD recommended
   ```

2. **Optimize PlatformIO Cache**:
   ```bash
   # PlatformIO packages are cached in /opt/esphome/.platformio
   # Ensure this directory persists across updates
   ```

3. **Parallel Compilations**:
   - Run multiple instances with different config files
   - Use a queue system (e.g., Celery, RabbitMQ) for production

## Integration Examples

### Webhook Integration

```python
# Example Flask webhook endpoint
from flask import Flask, request
import subprocess

app = Flask(__name__)

@app.route('/compile', methods=['POST'])
def compile_firmware():
    data = request.json
    config_url = data['config_url']
    bucket = data['upload_bucket']
    
    # Trigger compilation
    result = subprocess.run([
        'sudo', '-u', 'esphome',
        '/opt/esphome/service-wrapper.sh',
        config_url,
        bucket
    ], capture_output=True)
    
    return {'success': result.returncode == 0}
```

### API Integration

```bash
# Example API call that triggers compilation
curl -X POST https://your-api.com/webhook \
  -H "Content-Type: application/json" \
  -d '{
    "config_url": "https://s3.amazonaws.com/bucket/config.yaml",
    "device_id": "esp32-device-001"
  }'
```

## Support and Documentation

- **Full Documentation**: [COMPILE_BACKEND_SERVICE.md](COMPILE_BACKEND_SERVICE.md)
- **Quick Start**: [QUICKSTART_COMPILE_SERVICE.md](QUICKSTART_COMPILE_SERVICE.md)
- **ESPHome Docs**: https://esphome.io
- **GitHub Issues**: https://github.com/esphome/esphome/issues
- **Discord**: https://discord.gg/KhAMKrd

## License

ESPHome is licensed under the MIT License. See LICENSE file for details.
