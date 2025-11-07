# Binary Storage Automation Examples

## New Features Added

1. **Mode Selection** - Choose how to use your storage device:
   - `raw` - Binary read/write only (no filesystem)
   - `littlefs` - LittleFS filesystem only
   - `both` - Both binary and filesystem access

2. **Automation Actions** - Control storage from YAML automations:
   - `binary_storage.read` - Read bytes from address
   - `binary_storage.write` - Write bytes to address
   - `binary_storage.write_byte` - Write single byte
   - `binary_storage.write_string` - Write string (null-terminated)
   - `binary_storage.fill` - Fill entire storage with value

3. **Conditions** - Check storage state:
   - `binary_storage.is_ready` - Check if device is ready

---

## Mode Examples

### Mode: RAW (Binary Access Only)

```yaml
binary_storage:
  - type: FRAM
    id: my_fram
    model: MB85RC256
    address: 0x50
    mode: raw  # No filesystem, binary access only
```

**Use when:**
- You only need simple read/write operations
- Don't want filesystem overhead
- Storing simple values, counters, flags

### Mode: LITTLEFS (Filesystem Only)

```yaml
binary_storage:
  - type: EEPROM
    id: my_eeprom
    model: AT24C512
    address: 0x50
    mode: littlefs  # Filesystem access only
    mount_path: /config
    auto_format: true
```

**Use when:**
- You want standard file I/O
- Storing configuration files
- Need directory structure
- Want to use with existing code that uses fopen/fwrite

### Mode: BOTH (Binary + Filesystem)

```yaml
binary_storage:
  - type: FRAM
    id: my_fram
    model: MB85RC256
    address: 0x50
    mode: both  # Both binary and filesystem access
    mount_path: /fram
    auto_format: true
```

**Use when:**
- Need both binary and file access
- Want flexibility
- Different parts of code use different access methods

---

## Automation Action Examples

### 1. Write Byte on Button Press

```yaml
binary_button:
  - platform: gpio
    pin: GPIO0
    name: "Save Counter"
    on_press:
      - binary_storage.write_byte:
          id: my_fram
          address: 0x0000
          value: 42
```

### 2. Write Multiple Bytes (Array)

```yaml
on_...:
  - binary_storage.write:
      id: my_fram
      address: 0x0100
      data: [0x01, 0x02, 0x03, 0x04, 0x05]
```

### 3. Write String

```yaml
on_...:
  - binary_storage.write_string:
      id: my_fram
      address: 0x0200
      value: "ESPHome rocks!"
```

### 4. Increment Counter (Complex Example)

```yaml
globals:
  - id: boot_counter
    type: int
    initial_value: '0'

on_boot:
  priority: -100
  then:
    # Read counter from FRAM (would need lambda for this)
    - lambda: |-
        auto *storage = id(my_fram);
        uint8_t counter;
        storage->read(0x0000, &counter, 1);
        id(boot_counter) = counter + 1;

    # Write incremented counter back
    - binary_storage.write_byte:
        id: my_fram
        address: 0x0000
        value: !lambda 'return id(boot_counter);'
```

### 5. Clear FRAM on Button Hold

```yaml
binary_button:
  - platform: gpio
    pin: GPIO0
    name: "Clear FRAM"
    on_click:
      min_length: 3s
      max_length: 10s
      then:
        - logger.log: "Clearing FRAM..."
        - binary_storage.fill:
            id: my_fram
            value: 0x00  # Fill with zeros
        - logger.log: "FRAM cleared!"
```

### 6. Conditional Write (If Storage Ready)

```yaml
on_...:
  - if:
      condition:
        binary_storage.is_ready:
          id: my_fram
      then:
        - binary_storage.write_string:
            id: my_fram
            address: 0x0000
            value: "Ready!"
      else:
        - logger.log: "Storage not ready!"
```

### 7. Templated Address and Value

```yaml
globals:
  - id: storage_address
    type: int
    initial_value: '0'

on_...:
  - binary_storage.write_byte:
      id: my_fram
      address: !lambda 'return id(storage_address);'
      value: !lambda 'return millis() & 0xFF;'  # Write timestamp byte
```

---

## Complete Real-World Examples

### Example 1: Boot Counter with FRAM

```yaml
i2c:
  sda: GPIO21
  scl: GPIO22

binary_storage:
  - type: FRAM
    id: boot_fram
    model: MB85RC64
    address: 0x50
    mode: raw  # Just binary access for simple counter

globals:
  - id: boot_count
    type: int
    restore_value: no

on_boot:
  priority: -100
  then:
    # Read boot counter from FRAM
    - lambda: |-
        auto *fram = id(boot_fram);
        uint32_t count;
        fram->read(0x0000, (uint8_t*)&count, sizeof(count));
        id(boot_count) = count + 1;
        fram->write(0x0000, (uint8_t*)&count, sizeof(count));
        ESP_LOGI("boot", "Boot count: %d", id(boot_count));

sensor:
  - platform: template
    name: "Boot Counter"
    lambda: 'return id(boot_count);'
    update_interval: never
```

### Example 2: Data Logger with EEPROM + Filesystem

```yaml
binary_storage:
  - type: EEPROM
    id: data_eeprom
    model: AT24C512
    address: 0x50
    mode: littlefs  # Use filesystem for logs
    mount_path: /logs
    auto_format: true

on_boot:
  then:
    - lambda: |-
        // Create log file with timestamp
        FILE *f = fopen("/logs/boot.log", "a");
        if (f) {
          fprintf(f, "Booted at: %lu\n", millis());
          fclose(f);
        }

interval:
  - interval: 60s
    then:
      - lambda: |-
          // Log temperature every minute
          FILE *f = fopen("/logs/temp.csv", "a");
          if (f) {
            fprintf(f, "%lu,%.1f\n", millis(), id(my_temp_sensor).state);
            fclose(f);
          }
```

### Example 3: Configuration Storage (BOTH modes)

```yaml
binary_storage:
  - type: FRAM
    id: config_fram
    model: MB85RC256
    address: 0x50
    mode: both  # Binary for quick flags, filesystem for configs
    mount_path: /config
    auto_format: true

# Quick flag at address 0x0000
on_boot:
  then:
    - lambda: |-
        auto *fram = id(config_fram);
        uint8_t first_boot_flag;
        fram->read(0x0000, &first_boot_flag, 1);

        if (first_boot_flag != 0xAA) {
          ESP_LOGI("config", "First boot detected!");

          // Set flag
          first_boot_flag = 0xAA;
          fram->write(0x0000, &first_boot_flag, 1);

          // Create default config file
          FILE *f = fopen("/config/settings.json", "w");
          if (f) {
            fprintf(f, "{\"initialized\": true}\n");
            fclose(f);
          }
        }
```

### Example 4: State Persistence (Alternative to flash)

```yaml
binary_storage:
  - type: FRAM
    id: state_fram
    model: MB85RC256
    address: 0x50
    mode: raw

# Save light state to FRAM instead of flash (unlimited writes!)
light:
  - platform: binary
    id: my_light
    output: my_output
    on_turn_on:
      - binary_storage.write_byte:
          id: state_fram
          address: 0x0010  # Light state address
          value: 0x01
    on_turn_off:
      - binary_storage.write_byte:
          id: state_fram
          address: 0x0010
          value: 0x00

# Restore state on boot
on_boot:
  then:
    - lambda: |-
        auto *fram = id(state_fram);
        uint8_t state;
        fram->read(0x0010, &state, 1);
        if (state) {
          id(my_light).turn_on().perform();
        }
```

---

## Configuration Options Reference

### Mode Selection

| Mode | Binary Access | Filesystem Access | Use Case |
|------|---------------|-------------------|----------|
| `raw` | ✅ Yes | ❌ No | Simple values, counters, flags |
| `littlefs` | ❌ No | ✅ Yes | Files, logs, configuration |
| `both` | ✅ Yes | ✅ Yes | Flexible, both methods needed |

### LittleFS Options (when mode is littlefs or both)

```yaml
binary_storage:
  - mode: littlefs  # or both
    mount_path: /fram          # Optional: defaults to /<id>
    auto_format: true          # Optional: format if not formatted (default: true)
    partition_label: storage   # Optional: partition label (advanced)
```

**Important Notes:**
- `mount_path` auto-generates from device `id` if not specified
- `auto_format: true` will format on first boot if filesystem is corrupted
- Setting `auto_format: false` is safer but requires manual formatting

---

## Lambda Access (C++ API)

For complex operations, use lambdas:

```yaml
on_...:
  - lambda: |-
      auto *storage = id(my_fram);

      // Read
      uint8_t buffer[10];
      if (storage->read(0x0000, buffer, 10)) {
        ESP_LOGI("storage", "Read successful");
      }

      // Write
      uint8_t data[] = {1, 2, 3, 4, 5};
      if (storage->write(0x0100, data, 5)) {
        ESP_LOGI("storage", "Write successful");
      }

      // Get device info
      ESP_LOGI("storage", "Capacity: %u bytes", storage->get_capacity());
      ESP_LOGI("storage", "Type: %s", storage->get_device_type());
      ESP_LOGI("storage", "Page size: %u", storage->get_page_size());

      // Fill
      storage->fill(0xFF);  // Erase to 0xFF
```

---

## Best Practices

1. **Choose the Right Mode:**
   - Use `raw` for simple key-value storage
   - Use `littlefs` for files and structured data
   - Use `both` when you need flexibility

2. **FRAM vs EEPROM:**
   - Use FRAM for frequent writes (counters, state)
   - Use EEPROM for infrequent writes (configuration)
   - FRAM has no wear limit, EEPROM has ~1M cycles

3. **Address Organization:**
   - Document your memory map
   - Reserve address ranges for different purposes
   - Example:
     - 0x0000-0x00FF: Flags and counters
     - 0x0100-0x01FF: Configuration
     - 0x0200-0x0FFF: Data buffers

4. **Filesystem Best Practices:**
   - Always check FILE* is not NULL
   - Close files after use
   - Use `auto_format: true` for production
   - Regular filesystem checks if critical

5. **Error Handling:**
   - Check return values in lambdas
   - Use `is_ready` condition before operations
   - Log errors for debugging

---

## Troubleshooting

**Action doesn't work:**
- Check device ID is correct
- Verify storage is ready with `is_ready` condition
- Check address is within capacity range

**Filesystem mount fails:**
- Enable `auto_format: true`
- Check capacity is configured correctly
- Verify I2C communication works (test with raw mode first)

**Write doesn't persist (EEPROM):**
- Wait for write completion (~5ms)
- Don't exceed 1 million write cycles per location
- Check page boundaries (EEPROM only)

**FRAM advantages:**
- No write delays
- Instant persistence
- Unlimited writes
- Perfect for state machines, counters, flags
