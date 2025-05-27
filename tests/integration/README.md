# ESPHome Integration Tests

This directory contains end-to-end integration tests for ESPHome, focusing on testing the complete flow from YAML configuration to running devices with API connections.

## Structure

- `conftest.py` - Common fixtures and utilities
- `const.py` - Constants used throughout the integration tests
- `types.py` - Type definitions for fixtures and functions
- `fixtures/` - YAML configuration files for tests
- `test_*.py` - Individual test files

## How it works

### Automatic YAML Loading

The `yaml_config` fixture automatically loads YAML configurations based on the test name:
- It looks for a file named after the test function (e.g., `test_host_mode_basic` → `fixtures/host_mode_basic.yaml`)
- The fixture file must exist or the test will fail with a clear error message
- The fixture automatically injects a dynamic port number into the API configuration

### Key Fixtures

- `run_compiled` - Combines write, compile, and run operations into a single context manager
- `api_client_connected` - Creates an API client that automatically connects using ReconnectLogic
- `reserved_tcp_port` - Reserves a TCP port by holding the socket open until ESPHome needs it
- `unused_tcp_port` - Provides the reserved port number for each test

### Writing Tests

The simplest way to write a test is to use the `run_compiled` and `api_client_connected` fixtures:

```python
@pytest.mark.asyncio
async def test_my_feature(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    # Write, compile and run the ESPHome device, then connect to API
    async with run_compiled(yaml_config), api_client_connected() as client:
        # Test your feature using the connected client
        device_info = await client.device_info()
        assert device_info is not None
```

### Creating YAML Fixtures

Create a YAML file in the `fixtures/` directory with the same name as your test function (without the `test_` prefix):

```yaml
# fixtures/my_feature.yaml
esphome:
  name: my-test-device
host:
api:  # Port will be automatically injected
logger:
# Add your components here
```

## Running Tests

```bash
# Run all integration tests
script/integration_test

# Run a specific test
pytest -vv tests/integration/test_host_mode_basic.py

# Debug compilation errors or see ESPHome output
pytest -s tests/integration/test_host_mode_basic.py
```

## Implementation Details

- Tests automatically wait for the API port to be available before connecting
- Process cleanup is handled automatically, with graceful shutdown using SIGINT
- Each test gets its own temporary directory and unique port
- Port allocation minimizes race conditions by holding the socket until just before ESPHome starts
- Output from ESPHome processes is displayed for debugging
