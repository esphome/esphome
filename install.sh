#!/usr/bin/env bash

# ESPHome Compile Backend Service - LXC Installation Script
# This script installs and configures the ESPHome compile backend service
# Designed for Proxmox LXC containers running Debian/Ubuntu

set -euo pipefail

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
ESPHOME_USER="esphome"
ESPHOME_HOME="/opt/esphome"
ESPHOME_WORKSPACE="/var/lib/esphome"
SERVICE_NAME="esphome-compile-backend"
PYTHON_VERSION="3.11"

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Error handler
error_exit() {
    log_error "$1"
    exit 1
}

# Check if running as root
check_root() {
    if [[ $EUID -ne 0 ]]; then
        error_exit "This script must be run as root"
    fi
}

# Detect OS
detect_os() {
    if [[ -f /etc/os-release ]]; then
        . /etc/os-release
        OS=$ID
        VER=$VERSION_ID
        log_info "Detected OS: $OS $VER"
    else
        error_exit "Cannot detect OS. /etc/os-release not found"
    fi
}

# Update system
update_system() {
    log_info "Updating system packages..."
    apt-get update -qq
    apt-get upgrade -y -qq
    log_success "System updated"
}

# Install system dependencies
install_dependencies() {
    log_info "Installing system dependencies..."
    
    DEBIAN_FRONTEND=noninteractive apt-get install -y -qq \
        python3 \
        python3-pip \
        python3-venv \
        python3-dev \
        git \
        curl \
        wget \
        build-essential \
        gcc \
        g++ \
        make \
        cmake \
        libffi-dev \
        libssl-dev \
        libbz2-dev \
        libreadline-dev \
        libsqlite3-dev \
        zlib1g-dev \
        liblzma-dev \
        libncurses5-dev \
        libncursesw5-dev \
        xz-utils \
        tk-dev \
        libxml2-dev \
        libxmlsec1-dev \
        ca-certificates \
        gnupg \
        lsb-release \
        udev \
        software-properties-common
    
    log_success "System dependencies installed"
}

# Create esphome user
create_user() {
    if id "$ESPHOME_USER" &>/dev/null; then
        log_info "User $ESPHOME_USER already exists"
    else
        log_info "Creating user $ESPHOME_USER..."
        useradd -r -s /bin/bash -d "$ESPHOME_HOME" -m "$ESPHOME_USER"
        log_success "User $ESPHOME_USER created"
    fi
}

# Create directory structure
create_directories() {
    log_info "Creating directory structure..."
    
    mkdir -p "$ESPHOME_HOME"
    mkdir -p "$ESPHOME_WORKSPACE"
    mkdir -p /var/log/esphome
    
    chown -R "$ESPHOME_USER:$ESPHOME_USER" "$ESPHOME_HOME"
    chown -R "$ESPHOME_USER:$ESPHOME_USER" "$ESPHOME_WORKSPACE"
    chown -R "$ESPHOME_USER:$ESPHOME_USER" /var/log/esphome
    
    log_success "Directories created"
}

# Clone ESPHome repository
clone_repository() {
    log_info "Cloning ESPHome repository..."
    
    if [[ -d "$ESPHOME_HOME/esphome" ]]; then
        log_info "Repository already exists, pulling latest changes..."
        cd "$ESPHOME_HOME/esphome"
        sudo -u "$ESPHOME_USER" git pull
    else
        cd "$ESPHOME_HOME"
        sudo -u "$ESPHOME_USER" git clone https://github.com/esphome/esphome.git
        cd esphome
    fi
    
    log_success "Repository ready"
}

# Setup Python virtual environment
setup_venv() {
    log_info "Setting up Python virtual environment..."
    
    cd "$ESPHOME_HOME"
    
    if [[ -d "$ESPHOME_HOME/venv" ]]; then
        log_info "Virtual environment already exists"
    else
        sudo -u "$ESPHOME_USER" python3 -m venv venv
        log_success "Virtual environment created"
    fi
    
    # Activate and upgrade pip
    sudo -u "$ESPHOME_USER" bash -c "source venv/bin/activate && pip install --upgrade pip setuptools wheel"
    
    log_success "Virtual environment ready"
}

# Install ESPHome and dependencies
install_esphome() {
    log_info "Installing ESPHome and dependencies..."
    
    cd "$ESPHOME_HOME/esphome"
    
    # Install ESPHome in editable mode
    sudo -u "$ESPHOME_USER" bash -c "source $ESPHOME_HOME/venv/bin/activate && pip install -e ."
    
    # Ensure boto3 and requests are installed (for compile backend service)
    sudo -u "$ESPHOME_USER" bash -c "source $ESPHOME_HOME/venv/bin/activate && pip install boto3 requests"
    
    log_success "ESPHome installed"
}

# Configure PlatformIO
configure_platformio() {
    log_info "Configuring PlatformIO..."
    
    sudo -u "$ESPHOME_USER" bash -c "source $ESPHOME_HOME/venv/bin/activate && \
        platformio settings set enable_telemetry No && \
        platformio settings set check_platformio_interval 1000000"
    
    log_success "PlatformIO configured"
}

# Create systemd service file
create_systemd_service() {
    log_info "Creating systemd service file..."
    
    cat > /etc/systemd/system/${SERVICE_NAME}.service <<EOF
[Unit]
Description=ESPHome Compile Backend Service
After=network.target
Wants=network-online.target

[Service]
Type=simple
User=${ESPHOME_USER}
Group=${ESPHOME_USER}
WorkingDirectory=${ESPHOME_WORKSPACE}
Environment="PATH=${ESPHOME_HOME}/venv/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
Environment="VIRTUAL_ENV=${ESPHOME_HOME}/venv"

# Service will be started manually with appropriate arguments
# Example: systemctl start ${SERVICE_NAME}@config-url@upload-bucket
# Or managed through a wrapper script

# Note: This service requires command-line arguments to run
# See /opt/esphome/service-wrapper.sh for a management wrapper

ExecStart=${ESPHOME_HOME}/venv/bin/python3 ${ESPHOME_HOME}/esphome/script/compile_backend_service.py

# Restart on failure
Restart=on-failure
RestartSec=10s

# Resource limits
LimitNOFILE=65536
MemoryMax=2G
CPUQuota=80%

# Logging
StandardOutput=journal
StandardError=journal
SyslogIdentifier=${SERVICE_NAME}

[Install]
WantedBy=multi-user.target
EOF
    
    log_success "Systemd service file created"
}

# Create environment file template
create_env_template() {
    log_info "Creating environment configuration template..."
    
    cat > "$ESPHOME_HOME/compile-backend.env.example" <<'EOF'
# ESPHome Compile Backend Service Configuration
# Copy this file to compile-backend.env and customize it

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

# HTTP Callbacks
STATUS_CALLBACK_URL=https://api.example.com/status
COMPLETION_CALLBACK_URL=https://api.example.com/complete

# Logging
LOG_LEVEL=INFO
EOF
    
    chown "$ESPHOME_USER:$ESPHOME_USER" "$ESPHOME_HOME/compile-backend.env.example"
    
    log_success "Environment template created at $ESPHOME_HOME/compile-backend.env.example"
}

# Create service wrapper script
create_wrapper_script() {
    log_info "Creating service wrapper script..."
    
    cat > "$ESPHOME_HOME/service-wrapper.sh" <<'EOF'
#!/usr/bin/env bash
# ESPHome Compile Backend Service Wrapper
# This script simplifies running the compile backend service

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ESPHOME_DIR="$SCRIPT_DIR/esphome"
VENV_DIR="$SCRIPT_DIR/venv"
WORKSPACE="/var/lib/esphome"
ENV_FILE="$SCRIPT_DIR/compile-backend.env"

# Load environment variables if env file exists
if [[ -f "$ENV_FILE" ]]; then
    source "$ENV_FILE"
fi

# Check required arguments
if [[ $# -lt 2 ]]; then
    echo "Usage: $0 <config_url> <upload_bucket> [additional options]"
    echo ""
    echo "Examples:"
    echo "  $0 https://s3.amazonaws.com/bucket/config.yaml my-firmware-bucket"
    echo "  $0 https://s3.amazonaws.com/bucket/config.yaml my-firmware-bucket --upload-prefix firmware/"
    echo ""
    echo "Environment variables can be set in: $ENV_FILE"
    exit 1
fi

CONFIG_URL="$1"
UPLOAD_BUCKET="$2"
shift 2

# Build command with optional environment variables
CMD_ARGS=(
    "$CONFIG_URL"
    "$UPLOAD_BUCKET"
)

# Add optional arguments from environment
[[ -n "${S3_UPLOAD_PREFIX:-}" ]] && CMD_ARGS+=("--upload-prefix" "$S3_UPLOAD_PREFIX")
[[ -n "${STATUS_CALLBACK_URL:-}" ]] && CMD_ARGS+=("--status-callback" "$STATUS_CALLBACK_URL")
[[ -n "${COMPLETION_CALLBACK_URL:-}" ]] && CMD_ARGS+=("--completion-callback" "$COMPLETION_CALLBACK_URL")
[[ -n "${AWS_DEFAULT_REGION:-}" ]] && CMD_ARGS+=("--aws-region" "$AWS_DEFAULT_REGION")
[[ -n "${S3_ENDPOINT:-}" ]] && CMD_ARGS+=("--s3-endpoint" "$S3_ENDPOINT")
[[ -n "${S3_PATH_STYLE:-}" ]] && [[ "$S3_PATH_STYLE" == "true" ]] && CMD_ARGS+=("--s3-path-style")
[[ -n "${S3_UNSIGNED:-}" ]] && [[ "$S3_UNSIGNED" == "true" ]] && CMD_ARGS+=("--s3-unsigned")
[[ -n "${LOG_LEVEL:-}" ]] && CMD_ARGS+=("--log-level" "$LOG_LEVEL")

# Add any additional command-line arguments
CMD_ARGS+=("$@")

# Run the service
source "$VENV_DIR/bin/activate"
exec python3 "$ESPHOME_DIR/script/compile_backend_service.py" \
    --workspace "$WORKSPACE" \
    "${CMD_ARGS[@]}"
EOF
    
    chmod +x "$ESPHOME_HOME/service-wrapper.sh"
    chown "$ESPHOME_USER:$ESPHOME_USER" "$ESPHOME_HOME/service-wrapper.sh"
    
    log_success "Service wrapper script created at $ESPHOME_HOME/service-wrapper.sh"
}

# Create management CLI
create_management_cli() {
    log_info "Creating management CLI..."
    
    cat > /usr/local/bin/esphome-compile <<'EOF'
#!/usr/bin/env bash
# ESPHome Compile Backend Management CLI

set -euo pipefail

ESPHOME_USER="esphome"
ESPHOME_HOME="/opt/esphome"
SERVICE_NAME="esphome-compile-backend"

case "${1:-help}" in
    compile)
        shift
        sudo -u "$ESPHOME_USER" "$ESPHOME_HOME/service-wrapper.sh" "$@"
        ;;
    
    status)
        systemctl status "$SERVICE_NAME" || true
        ;;
    
    logs)
        journalctl -u "$SERVICE_NAME" -f
        ;;
    
    update)
        echo "Updating ESPHome..."
        cd "$ESPHOME_HOME/esphome"
        sudo -u "$ESPHOME_USER" git pull
        sudo -u "$ESPHOME_USER" bash -c "source $ESPHOME_HOME/venv/bin/activate && pip install -e ."
        echo "Update complete!"
        ;;
    
    config)
        if [[ -f "$ESPHOME_HOME/compile-backend.env" ]]; then
            "${EDITOR:-nano}" "$ESPHOME_HOME/compile-backend.env"
        else
            echo "Configuration file not found. Creating from template..."
            cp "$ESPHOME_HOME/compile-backend.env.example" "$ESPHOME_HOME/compile-backend.env"
            chown "$ESPHOME_USER:$ESPHOME_USER" "$ESPHOME_HOME/compile-backend.env"
            "${EDITOR:-nano}" "$ESPHOME_HOME/compile-backend.env"
        fi
        ;;
    
    test)
        echo "Running tests..."
        cd "$ESPHOME_HOME/esphome"
        sudo -u "$ESPHOME_USER" bash -c "source $ESPHOME_HOME/venv/bin/activate && python3 -m pytest tests/test_compile_backend_service.py -v"
        ;;
    
    help|*)
        cat <<HELP
ESPHome Compile Backend Management CLI

Usage: esphome-compile <command> [options]

Commands:
  compile <config_url> <upload_bucket> [opts]
                    Compile an ESPHome configuration from S3
                    Example: esphome-compile compile https://s3.../config.yaml my-bucket
  
  status            Show service status
  logs              Show and follow service logs
  update            Update ESPHome to latest version
  config            Edit configuration file
  test              Run compile backend tests
  help              Show this help message

Configuration:
  Edit /opt/esphome/compile-backend.env to set default options

Examples:
  # Compile a configuration
  esphome-compile compile https://s3.amazonaws.com/bucket/device.yaml firmware-bucket

  # Compile with custom options
  esphome-compile compile https://s3.../config.yaml my-bucket --upload-prefix firmware/

  # Check logs
  esphome-compile logs

  # Update ESPHome
  esphome-compile update

For more information, see /opt/esphome/esphome/COMPILE_BACKEND_SERVICE.md
HELP
        ;;
esac
EOF
    
    chmod +x /usr/local/bin/esphome-compile
    
    log_success "Management CLI created at /usr/local/bin/esphome-compile"
}

# Create README
create_readme() {
    log_info "Creating installation README..."
    
    cat > "$ESPHOME_HOME/README.md" <<'EOF'
# ESPHome Compile Backend Service

This installation provides the ESPHome compile backend service for processing
ESPHome configurations from S3.

## Quick Start

### 1. Configure the Service

Edit the configuration file:
```bash
esphome-compile config
```

Set your AWS credentials and S3 bucket information.

### 2. Compile a Configuration

```bash
esphome-compile compile https://s3.amazonaws.com/bucket/config.yaml my-firmware-bucket
```

### 3. Monitor Logs

```bash
esphome-compile logs
```

## Management Commands

- `esphome-compile compile <url> <bucket>` - Compile configuration
- `esphome-compile status` - Check service status
- `esphome-compile logs` - View logs
- `esphome-compile config` - Edit configuration
- `esphome-compile update` - Update ESPHome
- `esphome-compile test` - Run tests

## Configuration

Configuration file: `/opt/esphome/compile-backend.env`

Required settings:
- `AWS_ACCESS_KEY_ID` - AWS access key
- `AWS_SECRET_ACCESS_KEY` - AWS secret key
- `S3_UPLOAD_BUCKET` - S3 bucket for compiled firmware

Optional settings:
- `S3_UPLOAD_PREFIX` - Prefix for uploaded files
- `STATUS_CALLBACK_URL` - HTTP endpoint for status updates
- `COMPLETION_CALLBACK_URL` - HTTP endpoint for completion notifications
- `AWS_DEFAULT_REGION` - AWS region (default: us-east-1)
- `LOG_LEVEL` - Logging level (DEBUG, INFO, WARNING, ERROR)

## Directory Structure

- `/opt/esphome/` - ESPHome installation directory
- `/opt/esphome/esphome/` - ESPHome source code
- `/opt/esphome/venv/` - Python virtual environment
- `/var/lib/esphome/` - Working directory for builds
- `/var/log/esphome/` - Log files

## Documentation

Full documentation: `/opt/esphome/esphome/COMPILE_BACKEND_SERVICE.md`

## Support

For issues and questions, see: https://github.com/esphome/esphome
EOF
    
    chown "$ESPHOME_USER:$ESPHOME_USER" "$ESPHOME_HOME/README.md"
    
    log_success "README created"
}

# Display post-installation instructions
show_instructions() {
    cat <<EOF

${GREEN}═══════════════════════════════════════════════════════════════${NC}
${GREEN}     ESPHome Compile Backend Service Installed Successfully!     ${NC}
${GREEN}═══════════════════════════════════════════════════════════════${NC}

${BLUE}Installation Details:${NC}
  - User: ${ESPHOME_USER}
  - Home: ${ESPHOME_HOME}
  - Workspace: ${ESPHOME_WORKSPACE}
  - Logs: /var/log/esphome

${BLUE}Next Steps:${NC}

  1. ${YELLOW}Configure the service:${NC}
     esphome-compile config

  2. ${YELLOW}Set your AWS credentials and S3 bucket${NC}

  3. ${YELLOW}Test the installation:${NC}
     esphome-compile test

  4. ${YELLOW}Compile a configuration:${NC}
     esphome-compile compile https://s3.../config.yaml my-bucket

${BLUE}Management Commands:${NC}
  esphome-compile help     - Show all available commands
  esphome-compile config   - Edit configuration
  esphome-compile logs     - View logs
  esphome-compile update   - Update ESPHome

${BLUE}Documentation:${NC}
  ${ESPHOME_HOME}/README.md
  ${ESPHOME_HOME}/esphome/COMPILE_BACKEND_SERVICE.md

${GREEN}═══════════════════════════════════════════════════════════════${NC}

EOF
}

# Main installation function
main() {
    log_info "Starting ESPHome Compile Backend Service installation..."
    
    check_root
    detect_os
    update_system
    install_dependencies
    create_user
    create_directories
    clone_repository
    setup_venv
    install_esphome
    configure_platformio
    create_systemd_service
    create_env_template
    create_wrapper_script
    create_management_cli
    create_readme
    
    # Reload systemd
    systemctl daemon-reload
    
    log_success "Installation completed successfully!"
    show_instructions
}

# Run main function
main "$@"
