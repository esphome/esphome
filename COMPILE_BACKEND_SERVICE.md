# ESPHome Compile Backend Service

## Overview

The `compile_backend_service.py` script provides an automated system for downloading ESPHome YAML configurations from S3, validating them, compiling firmware, and uploading the resulting binary back to S3. It also provides HTTP callback notifications at each stage of the process.

## Workflow

The service follows this workflow:

1. **Download Configuration**: Downloads a YAML configuration file from a public S3 URL
2. **Validate Configuration**: Runs `esphome config` to validate the YAML
   - On failure: Calls status callback API with validation error and CLI output
   - On success: Calls status callback API to notify that processing will start
3. **Compile Firmware**: Runs `esphome compile` to build the firmware
   - On failure: Calls status callback API with compilation error and CLI output
   - On success: Calls status callback API to notify compilation success
4. **Upload Binary**: 
   - Locates the `firmware.factory.bin` file in the build directory
   - Renames it based on the YAML filename (removes trailing `1` if present)
     - Example: `xyz1111.yaml` → `xyz111.bin`
   - Uploads to the specified S3 bucket
5. **Completion Notification**: Calls completion callback API with success status and uploaded file details
6. **Cleanup**: Removes temporary files and build artifacts (keeps cached packages)

## Usage

### Basic Command

```bash
python3 script/compile_backend_service.py \
  "https://s3.amazonaws.com/my-bucket/configs/device123.yaml" \
  "my-output-bucket" \
  --status-callback "https://api.example.com/status" \
  --completion-callback "https://api.example.com/complete" \
  --aws-region "us-east-1"
```

### With Custom S3 Endpoint (e.g., Beeceptor)

```bash
python3 script/compile_backend_service.py \
  "http://my-s3-mock.local/bucket/config.yaml" \
  "output-bucket" \
  --s3-endpoint "http://my-s3-mock.local" \
  --s3-path-style \
  --s3-unsigned \
  --status-callback "https://api.example.com/status" \
  --completion-callback "https://api.example.com/complete"
```

### All Options

- `config_url` (required): Public S3 URL to the YAML configuration file
- `upload_bucket` (required): S3 bucket name where compiled firmware will be uploaded
- `--upload-prefix`: Prefix for the uploaded binary key (e.g., `firmware/` results in `firmware/device.bin`)
- `--status-callback`: HTTP endpoint to receive validation and compilation status updates
- `--completion-callback`: HTTP endpoint to receive final completion notification
- `--workspace`: Working directory for builds (default: current directory)
- `--aws-region`: AWS region for S3 operations
- `--s3-endpoint`: Custom S3-compatible endpoint URL
- `--s3-path-style`: Use path-style S3 addressing (for some mock servers)
- `--s3-unsigned`: Disable AWS signature (for public/mock endpoints)
- `--log-level`: Logging level (DEBUG, INFO, WARNING, ERROR)

## API Callback Payloads

### Status Callback (Validation Stage)

**Success:**
```json
{
  "config_url": "https://s3.amazonaws.com/bucket/config.yaml",
  "stage": "validation",
  "success": true,
  "output": "Configuration valid!\n..."
}
```

**Failure:**
```json
{
  "config_url": "https://s3.amazonaws.com/bucket/config.yaml",
  "stage": "validation",
  "success": false,
  "output": "Error: Invalid configuration\n..."
}
```

### Status Callback (Compilation Stage)

**Success:**
```json
{
  "config_url": "https://s3.amazonaws.com/bucket/config.yaml",
  "stage": "compile",
  "success": true,
  "output": "Compilation complete!\n..."
}
```

**Failure:**
```json
{
  "config_url": "https://s3.amazonaws.com/bucket/config.yaml",
  "stage": "compile",
  "success": false,
  "output": "Error: Compilation failed\n..."
}
```

### Completion Callback

**Success:**
```json
{
  "config_url": "https://s3.amazonaws.com/bucket/config.yaml",
  "stage": "complete",
  "success": true,
  "output": "Compilation complete!\n...",
  "uploaded_key": "device123.bin",
  "bucket": "my-output-bucket"
}
```

## File Naming

The service automatically derives the output binary name from the YAML filename:

- `device123.yaml` → `device123.bin`
- `xyz1111.yaml` → `xyz111.bin` (trailing `1` is removed)
- `test.yaml` → `test.bin`

This naming logic is implemented in the `_derive_output_name()` function.

## Error Handling

The service handles errors at each stage:

1. **Download Failure**: If the S3 URL is invalid or inaccessible
2. **Validation Failure**: Calls status callback with error details, then exits
3. **Compilation Failure**: Calls status callback with error details, then exits
4. **Missing Artifact**: If `firmware.factory.bin` cannot be found after compilation
5. **Upload Failure**: If S3 upload fails due to credentials or network issues

All errors are logged and relevant callbacks are invoked before the process terminates.

## Testing

The service includes comprehensive tests in `tests/test_compile_backend_service.py`:

```bash
# Run all tests
python3 -m pytest tests/test_compile_backend_service.py -v

# Run specific test
python3 -m pytest tests/test_compile_backend_service.py::test_process_configuration_end_to_end -v
```

The tests use moto to mock S3 and verify:
- File naming logic
- S3 upload with various configurations
- End-to-end workflow with callbacks

## Dependencies

Required Python packages:
- `boto3`: AWS SDK for S3 operations
- `botocore`: Low-level AWS client library
- `requests`: HTTP client for callbacks

For testing:
- `pytest`: Test framework
- `moto[s3]`: S3 mocking library

## Environment Variables

The service uses boto3's standard credential chain:
- AWS credentials from environment (`AWS_ACCESS_KEY_ID`, `AWS_SECRET_ACCESS_KEY`)
- AWS credentials from `~/.aws/credentials`
- IAM role (when running on EC2/ECS)

For unsigned/public access, use `--s3-unsigned` flag.

## Integration Example

Here's a typical integration workflow:

1. **User uploads YAML** → Store in S3 with public-read ACL
2. **Trigger compilation** → Call this service with the S3 URL
3. **Receive validation callback** → Update UI to show "Processing..."
4. **Receive compilation callback** → Update UI to show "Compiling..."
5. **Receive completion callback** → Update UI with download link to the binary

## Security Considerations

- The service requires read access to the source S3 bucket (for downloading YAML)
- The service requires write access to the destination S3 bucket (for uploading binaries)
- Status and completion callbacks should use HTTPS
- The YAML configuration URL must be publicly accessible or the service needs appropriate S3 credentials
- The service executes ESPHome commands which will install dependencies - run in an isolated environment

## Troubleshooting

### "Unable to locate credentials"
- Ensure AWS credentials are configured
- Or use `--s3-unsigned` for public S3 access

### "HTTP Error 403: FORBIDDEN" when downloading
- Ensure the S3 object has public-read ACL
- Or configure appropriate AWS credentials

### "Unable to locate firmware.factory.bin"
- Check that the YAML configuration specifies a valid platform (esp32, esp8266, etc.)
- Ensure the compilation succeeded without errors
- Check build logs in the workspace directory

### Callback timeouts
- Ensure callback URLs are accessible from the service
- Check for firewall rules blocking outbound HTTP/HTTPS
