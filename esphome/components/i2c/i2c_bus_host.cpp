#ifdef USE_HOST
#if defined(__linux__)

#include "i2c_bus_host.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <cerrno>
#include <cstdint>
#include <cstring>

namespace esphome::i2c {

static const char *const TAG = "i2c.host";

HostI2CBus::~HostI2CBus() {
  if (this->file_descriptor_ != -1) {
    close(this->file_descriptor_);
    this->file_descriptor_ = -1;
  }
}

void HostI2CBus::setup() {
  ESP_LOGCONFIG(TAG, "Setting up I2C bus...");

  // Open I2C device file
  this->file_descriptor_ = open(this->device_.c_str(), O_RDWR);
  if (this->file_descriptor_ == -1) {
    int err = errno;
    if (err == ENOENT) {
      this->update_error_("not found");
    } else if (err == EACCES) {
      this->update_error_("permission denied");
    } else {
      this->update_error_(std::string("failed to open: ") + strerror(err));
    }
    this->mark_failed();
    return;
  }

  this->initialized_ = true;
  ESP_LOGCONFIG(TAG, "  Device: %s", this->device_.c_str());

  // Run bus scan if enabled
  if (this->scan_) {
    this->i2c_scan_();
  }
}

void HostI2CBus::dump_config() {
  ESP_LOGCONFIG(TAG, "I2C Bus:");
  ESP_LOGCONFIG(TAG, "  Device: %s", this->device_.c_str());
  // Bus frequency cannot be set from userspace via i2c-dev; report it as informational only
  ESP_LOGCONFIG(TAG, "  Frequency: %u Hz (informational; not applied on host)", this->frequency_);

  if (!this->first_error_.empty()) {
    ESP_LOGE(TAG, "  Setup Error: %s", this->first_error_.c_str());
  }

  if (this->scan_) {
    ESP_LOGI(TAG, "  Scan Results:");
    for (const auto &s : this->scan_results_) {
      if (s.second) {
        ESP_LOGI(TAG, "    0x%02X: Found", s.first);
      }
    }
  }
}

ErrorCode HostI2CBus::write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count,
                                  uint8_t *read_buffer, size_t read_count) {
  if (!this->initialized_) {
    ESP_LOGE(TAG, "I2C bus not initialized");
    return ERROR_NOT_INITIALIZED;
  }

  ESP_LOGVV(TAG, "I2C write_readv addr=0x%02X write=%zu read=%zu", address, write_count, read_count);

  // Handle special case: probe (no write data, no read data)
  // This is used for device detection during bus scanning
  if (write_count == 0 && read_count == 0) {
    struct i2c_msg msg;
    msg.addr = address;
    msg.flags = 0;
    msg.len = 0;
    msg.buf = nullptr;

    struct i2c_rdwr_ioctl_data rdwr_data;
    rdwr_data.msgs = &msg;
    rdwr_data.nmsgs = 1;

    int ret = ioctl(this->file_descriptor_, I2C_RDWR, &rdwr_data);
    if (ret < 0) {
      int err = errno;
      // If I2C_RDWR not supported, try SMBus Quick command (what i2cdetect uses)
      if (err == EOPNOTSUPP || err == ENOSYS) {
        ESP_LOGVV(TAG, "I2C_RDWR probe failed, trying SMBus Quick for addr=0x%02X", address);
        if (ioctl(this->file_descriptor_, I2C_SLAVE, address) < 0) {  // NOLINT
          return this->map_errno_to_error_code_(errno);
        }
        // Use I2C_SMBUS ioctl with Quick command
        union i2c_smbus_data data;
        struct i2c_smbus_ioctl_data args;
        args.read_write = I2C_SMBUS_WRITE;
        args.command = 0;
        args.size = I2C_SMBUS_QUICK;
        args.data = &data;
        ret = ioctl(this->file_descriptor_, I2C_SMBUS, &args);
        if (ret < 0) {
          return this->map_errno_to_error_code_(errno);
        }
        return ERROR_OK;
      }
      return this->map_errno_to_error_code_(err);
    }
    return ERROR_OK;
  }

  // i2c_msg.len is a 16-bit field; reject transfers that would silently truncate
  if (write_count > UINT16_MAX || read_count > UINT16_MAX) {
    ESP_LOGE(TAG, "I2C transfer too large: write=%zu read=%zu (max %u)", write_count, read_count,
             (unsigned) UINT16_MAX);
    return ERROR_TOO_LARGE;
  }

  // Prepare messages for combined write-read transaction
  struct i2c_msg msgs[2];
  int num_msgs = 0;

  // Add write message if write data present
  if (write_count > 0) {
    msgs[num_msgs].addr = address;
    msgs[num_msgs].flags = 0;  // Write
    msgs[num_msgs].len = write_count;
    msgs[num_msgs].buf = const_cast<uint8_t *>(write_buffer);
    num_msgs++;
  }

  // Add read message if read data requested
  if (read_count > 0) {
    msgs[num_msgs].addr = address;
    msgs[num_msgs].flags = I2C_M_RD;  // Read
    msgs[num_msgs].len = read_count;
    msgs[num_msgs].buf = read_buffer;
    num_msgs++;
  }

  // Execute I2C transaction
  struct i2c_rdwr_ioctl_data rdwr_data;
  rdwr_data.msgs = msgs;
  rdwr_data.nmsgs = num_msgs;

  int ret = ioctl(this->file_descriptor_, I2C_RDWR, &rdwr_data);
  if (ret < 0) {
    int err = errno;
    if (err == EOPNOTSUPP || err == ENOSYS) {
      ESP_LOGV(TAG, "I2C_RDWR not supported, using I2C_SLAVE fallback for addr=0x%02X", address);  // NOLINT
      if (ioctl(this->file_descriptor_, I2C_SLAVE, address) < 0) {                                 // NOLINT
        ESP_LOGV(TAG, "I2C_SLAVE ioctl failed: %s", strerror(errno));                              // NOLINT
        return this->map_errno_to_error_code_(errno);
      }
      // Perform write if needed
      if (write_count > 0) {
        ssize_t written = ::write(this->file_descriptor_, write_buffer, write_count);
        if (written != (ssize_t) write_count) {
          int write_err = errno;
          // If write() also fails with EOPNOTSUPP, try I2C_SMBUS as last resort
          if (write_err == EOPNOTSUPP || write_err == ENOSYS) {
            ESP_LOGV(TAG, "I2C_SLAVE write not supported, trying I2C_SMBUS for addr=0x%02X", address);  // NOLINT
            // Use I2C_SMBUS_I2C_BLOCK_DATA for writes up to 32 bytes
            // Standard SMBus mapping: first byte is command, remaining bytes are data
            if (write_count < 1) {
              ESP_LOGE(TAG, "Write size too small for I2C_SMBUS");
              return ERROR_INVALID_ARGUMENT;
            }
            if (write_count > I2C_SMBUS_BLOCK_MAX + 1) {
              ESP_LOGE(TAG, "Write size %zu exceeds I2C_SMBUS_BLOCK_MAX+1 (%d)", write_count, I2C_SMBUS_BLOCK_MAX + 1);
              return ERROR_INVALID_ARGUMENT;
            }
            union i2c_smbus_data data;
            // Standard SMBus: first byte = command, rest = data
            uint8_t command = write_buffer[0];
            size_t data_len = write_count - 1;
            data.block[0] = data_len;
            if (data_len > 0) {
              memcpy(&data.block[1], write_buffer + 1, data_len);
            }

            struct i2c_smbus_ioctl_data args;
            args.read_write = I2C_SMBUS_WRITE;
            args.command = command;
            args.size = I2C_SMBUS_I2C_BLOCK_DATA;
            args.data = &data;

            ret = ioctl(this->file_descriptor_, I2C_SMBUS, &args);
            if (ret < 0) {
              ESP_LOGV(TAG, "I2C_SMBUS write failed: %s", strerror(errno));
              return this->map_errno_to_error_code_(errno);
            }
          } else {
            ESP_LOGV(TAG, "I2C write failed: %s", strerror(write_err));
            return this->map_errno_to_error_code_(write_err);
          }
        }
      }
      // Perform read if needed
      if (read_count > 0) {
        ssize_t bytes_read = ::read(this->file_descriptor_, read_buffer, read_count);
        if (bytes_read != (ssize_t) read_count) {
          int read_err = errno;
          // If read() also fails with EOPNOTSUPP, try I2C_SMBUS as last resort
          if (read_err == EOPNOTSUPP || read_err == ENOSYS) {
            ESP_LOGV(TAG, "I2C_SLAVE read not supported, trying I2C_SMBUS for addr=0x%02X", address);  // NOLINT
            // Use I2C_SMBUS_I2C_BLOCK_DATA for reads up to 32 bytes
            if (read_count > I2C_SMBUS_BLOCK_MAX) {
              ESP_LOGE(TAG, "Read size %zu exceeds I2C_SMBUS_BLOCK_MAX (%d)", read_count, I2C_SMBUS_BLOCK_MAX);
              return ERROR_INVALID_ARGUMENT;
            }
            union i2c_smbus_data data;
            data.block[0] = read_count;

            struct i2c_smbus_ioctl_data args;
            args.read_write = I2C_SMBUS_READ;
            args.command = 0;  // Start register/command
            args.size = I2C_SMBUS_I2C_BLOCK_DATA;
            args.data = &data;

            ret = ioctl(this->file_descriptor_, I2C_SMBUS, &args);
            if (ret < 0) {
              ESP_LOGV(TAG, "I2C_SMBUS read failed: %s", strerror(errno));
              return this->map_errno_to_error_code_(errno);
            }
            // I2C_SMBUS_I2C_BLOCK_DATA returns the actual byte count in block[0];
            // a short read means we did not receive all requested bytes
            if (data.block[0] < read_count) {
              ESP_LOGV(TAG, "I2C_SMBUS short read: got %u, expected %zu", data.block[0], read_count);
              return ERROR_NOT_ACKNOWLEDGED;
            }
            // Copy data from SMBus buffer to output buffer
            memcpy(read_buffer, &data.block[1], read_count);
          } else {
            ESP_LOGV(TAG, "I2C read failed: %s", strerror(read_err));
            return this->map_errno_to_error_code_(read_err);
          }
        }
      }
      ESP_LOGVV(TAG, "I2C transaction successful (I2C_SLAVE method)");  // NOLINT
      return ERROR_OK;
    }
    ESP_LOGV(TAG, "I2C transaction failed: %s", strerror(err));
    return this->map_errno_to_error_code_(err);
  }

  ESP_LOGVV(TAG, "I2C transaction successful");
  return ERROR_OK;
}

ErrorCode HostI2CBus::map_errno_to_error_code_(int err) {
  switch (err) {
    case ENXIO:
      return ERROR_NOT_ACKNOWLEDGED;
    case ETIMEDOUT:
      return ERROR_TIMEOUT;
    case EINVAL:
      return ERROR_INVALID_ARGUMENT;
    case ENODEV:
    case ENOTTY:
      return ERROR_NOT_INITIALIZED;
    case EOPNOTSUPP:
    case ENOSYS:
      // Operation not supported - some I2C adapters don't support zero-length transactions
      ESP_LOGVV(TAG, "I2C adapter does not support this operation (likely zero-length probe)");
      return ERROR_NOT_ACKNOWLEDGED;
    default:
      ESP_LOGV(TAG, "Unmapped error code: %d (%s)", err, strerror(err));
      return ERROR_UNKNOWN;
  }
}

void HostI2CBus::update_error_(const std::string &error) {
  if (this->first_error_.empty()) {
    this->first_error_ = error;
  }
  ESP_LOGE(TAG, "[%s] %s", this->device_.c_str(), error.c_str());
}

}  // namespace esphome::i2c

#else
#error "HostI2CBus is only supported on Linux"
#endif  // defined(__linux__)
#endif  // USE_HOST
