# Quick Start Guide: ESPHome Compile Backend Service

## Installation

### Proxmox LXC (Recommended)

One-command installation:

```bash
bash <(curl -s https://raw.githubusercontent.com/esphome/esphome/dev/install.sh)
```

See [PROXMOX_QUICKSTART.md](PROXMOX_QUICKSTART.md) for detailed LXC setup instructions.

### Manual Installation

If you prefer manual installation or are not using Proxmox:

1. Clone the repository:
```bash
git clone https://github.com/esphome/esphome.git
cd esphome
```

2. Install dependencies:
```bash
pip install -e .
pip install boto3 requests
```

3. Configure PlatformIO:
```bash
platformio settings set enable_telemetry No
```

## What It Does

This service automates the complete ESPHome firmware compilation workflow:

```
S3 YAML Download → Validate → Compile → Rename → Upload to S3 → Notify
```

## Quick Example

### 1. Basic Usage

```bash
python3 script/compile_backend_service.py \
  "https://my-bucket.s3.amazonaws.com/configs/device123.yaml" \
  "my-firmware-bucket" \
  --status-callback "https://api.example.com/status" \
  --completion-callback "https://api.example.com/complete" \
  --aws-region "us-east-1"
```

### 2. What Happens

1. **Download**: Fetches `device123.yaml` from S3
2. **Validate**: Runs `esphome config device123.yaml`
   - ✅ Success → Calls status API: `{"stage": "validation", "success": true, ...}`
   - ❌ Failure → Calls status API with error, then exits
3. **Compile**: Runs `esphome compile device123.yaml`
   - ✅ Success → Calls status API: `{"stage": "compile", "success": true, ...}`
   - ❌ Failure → Calls status API with error, then exits
4. **Upload**: Finds `firmware.factory.bin`, renames to `device123.bin`, uploads to S3
5. **Complete**: Calls completion API: `{"stage": "complete", "uploaded_key": "device123.bin", ...}`
6. **Cleanup**: Removes temporary files

### 3. File Naming Logic

The service automatically derives the output name:

| Input YAML | Output Binary | Note |
|------------|---------------|------|
| `device123.yaml` | `device123.bin` | Standard naming |
| `xyz1111.yaml` | `xyz111.bin` | Trailing `1` removed |
| `test.yaml` | `test.bin` | No change needed |

## API Callback Examples

### Validation Success
```json
POST https://api.example.com/status
{
  "config_url": "https://s3.../device123.yaml",
  "stage": "validation",
  "success": true,
  "output": "Configuration is valid!\n..."
}
```

### Compilation Failure
```json
POST https://api.example.com/status
{
  "config_url": "https://s3.../device123.yaml",
  "stage": "compile",
  "success": false,
  "output": "ERROR: Compilation failed\n..."
}
```

### Upload Complete
```json
POST https://api.example.com/complete
{
  "config_url": "https://s3.../device123.yaml",
  "stage": "complete",
  "success": true,
  "output": "Compilation complete!\n...",
  "uploaded_key": "device123.bin",
  "bucket": "my-firmware-bucket"
}
```

## Testing

### Run Unit Tests
```bash
python3 -m pytest tests/test_compile_backend_service.py -v
```

### Expected Output
```
test_derive_output_name_trims_trailing_one PASSED
test_upload_factory_image_supports_unsigned_path_style PASSED
test_process_configuration_end_to_end PASSED
```

## Common Use Cases

### Case 1: User Uploads YAML to Web Interface

1. User uploads YAML file
2. Web backend stores it in S3 with public-read ACL
3. Web backend triggers this service with the S3 URL
4. Service validates, compiles, and uploads firmware
5. Web backend receives callbacks and updates UI

### Case 2: Automated Build Pipeline

1. CI/CD system commits YAML to repository
2. CI/CD uploads YAML to S3
3. CI/CD triggers this service
4. Service compiles and uploads firmware
5. CI/CD receives completion callback

### Case 3: Webhook Integration

1. External service sends webhook with YAML URL
2. Your API receives webhook
3. API triggers this service
4. Service handles compilation
5. API receives callbacks and responds to webhook

## Command-Line Options Reference

```bash
# Required
config_url              # S3 URL to YAML file
upload_bucket          # S3 bucket for firmware

# Optional
--upload-prefix        # Prefix for uploaded file (e.g., "firmware/")
--status-callback      # URL for validation/compile notifications
--completion-callback  # URL for final completion notification
--workspace           # Build directory (default: current dir)
--aws-region          # AWS region (e.g., "us-east-1")
--s3-endpoint         # Custom S3 endpoint for testing
--s3-path-style       # Use path-style addressing
--s3-unsigned         # Disable request signing
--log-level           # DEBUG, INFO, WARNING, ERROR
```

## Troubleshooting

### "Unable to locate credentials"
```bash
# Set AWS credentials
export AWS_ACCESS_KEY_ID="your-key"
export AWS_SECRET_ACCESS_KEY="your-secret"

# Or use unsigned access for public S3
--s3-unsigned
```

### "HTTP Error 403: FORBIDDEN"
```bash
# Ensure S3 object has public-read ACL
aws s3api put-object-acl \
  --bucket my-bucket \
  --key configs/device.yaml \
  --acl public-read
```

### "Unable to locate firmware.factory.bin"
- Check that YAML specifies a valid platform (esp32, esp8266, rp2040)
- Verify compilation succeeded without errors
- Check workspace directory for build logs

## Security Notes

⚠️ **Important Security Considerations:**

1. The service downloads and executes YAML configurations - validate YAML sources
2. The service runs `esphome compile` which downloads dependencies - use isolated environments
3. Use HTTPS for callback URLs to prevent data interception
4. Ensure S3 credentials have minimal required permissions
5. Consider rate limiting to prevent abuse

## Full Documentation

For complete documentation, see [COMPILE_BACKEND_SERVICE.md](COMPILE_BACKEND_SERVICE.md)
