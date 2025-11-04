#if defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || defined(USE_ESP32_VARIANT_ESP32P4)

#include "esphome/components/usb_msc_host/usb_msc_host.h"

namespace esphome {
namespace usb_msc_host {

static const usb_standard_desc_t *next_interface_desc(const usb_standard_desc_t *desc, size_t len, size_t *offset) {
  return usb_parse_next_descriptor_of_type(desc, len, USB_W_VALUE_DT_INTERFACE, (int *) offset);
}

static const usb_intf_desc_t *find_msc_interface(const usb_config_desc_t *config_desc) {
  size_t offset = 0;
  size_t total_length = config_desc->wTotalLength;
  const usb_standard_desc_t *next_desc = (const usb_standard_desc_t *) config_desc;

  next_desc = next_interface_desc(next_desc, total_length, &offset);

  while (next_desc) {
    const usb_intf_desc_t *ifc_desc = (const usb_intf_desc_t *) next_desc;

    if (ifc_desc->bInterfaceClass == USB_CLASS_MASS_STORAGE && ifc_desc->bInterfaceSubClass == SCSI_COMMAND_SET &&
        ifc_desc->bInterfaceProtocol == BULK_ONLY_TRANSFER) {
      return ifc_desc;
    }

    next_desc = next_interface_desc(next_desc, total_length, &offset);
  };
  return NULL;
}

static bool is_mass_storage_device(usb_config_desc_t *config_desc) {
  if (find_msc_interface(config_desc)) {
    return true;
  } else {
    ESP_LOGV(TAG, "Connected USB device is not MSC");
    return false;
  }
}

/**
 * @brief Find a free slot in the device table.
 *
 * @return Index of the free slot, or -1 if no free slot is available.
 */
int8_t USBMscHost::find_free_slot(void) {
  for (int i = 0; i < MAX_MSC_DEVICES; i++) {
    if (this->msc_devices_[i] == NULL) {
      ESP_LOGI(TAG, "Found free slot for MSC device at index %d", i);
      return i;
    }
  }
  return -1;
}

int8_t USBMscHost::find_msc_device_slot(uint8_t usb_addr) {
  for (int i = 0; i < MAX_MSC_DEVICES; i++) {
    if (this->msc_devices_[i] != nullptr && this->msc_devices_[i]->usb_addr == usb_addr) {
      ESP_LOGI(TAG, "Found MSC device slot at index %d", i);
      return i;
    }
  }
  return -1;
}

/**
 * @brief Allocates a new MSC device entry and mounts it to VFS.
 *
 * This function finds a free slot for a new MSC device, allocates memory for the device entry,
 * installs the MSC device, and mounts it to the virtual file system (VFS).
 *
 * If any step fails, the function ensures proper cleanup of allocated resources before returning an error.
 *
 * @param[in] new_dev_address    USB device address
 *
 * @return
 *         - ESP_OK on success.
 *         - ESP_ERR_NOT_FOUND if no free slot is available.
 *         - ESP_ERR_NO_MEM if memory allocation fails.
 *         - Other esp_err_t codes if device installation or VFS registration fails.
 */
esp_err_t USBMscHost::allocate_new_msc_device(uint8_t new_dev_address) {
  int slot = this->find_free_slot();
  if (slot < 0) {
    ESP_LOGW(TAG, "No free slots for new MSC device (max %d)", MAX_MSC_DEVICES);
    return ESP_ERR_NOT_FOUND;
  }

  ESP_LOGI(TAG, "Allocating slot %d for device address %d", slot, new_dev_address);

  // void *slotbuffer = calloc(1, sizeof(msc_dev_entry_t));
  this->msc_devices_[slot] = (msc_dev_entry_t *) calloc(1, sizeof(msc_dev_entry_t));
  if (this->msc_devices_[slot] == NULL) {
    ESP_LOGE(TAG, "Failed to allocate memory for new MSC device entry");
    return ESP_ERR_NO_MEM;
  }

  ESP_LOGI(TAG, "Memory allocated, calling msc_host_install_device...");

  esp_err_t err = msc_host_install_device(new_dev_address, &this->msc_devices_[slot]->msc_device);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "msc_host_install_device failed: %s", esp_err_to_name(err));
    free(this->msc_devices_[slot]);
    this->msc_devices_[slot] = NULL;
    return err;
  }

  ESP_LOGI(TAG, "msc_host_install_device succeeded");

  this->msc_devices_[slot]->usb_addr = new_dev_address;

  ESP_LOGI(TAG, "Stored USB address, proceeding with VFS registration...");

  const esp_vfs_fat_mount_config_t mount_config = {
      .format_if_mount_failed = false,
      .max_files = 5,
      .allocation_unit_size = 1024,
  };

  char mount_path[16];
  snprintf(mount_path, sizeof(mount_path), "%s", MNT_PATH);

  err = msc_host_vfs_register(this->msc_devices_[slot]->msc_device, mount_path, &mount_config,
                              &this->msc_devices_[slot]->vfs_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "msc_host_vfs_register failed: %s", esp_err_to_name(err));
    esp_err_t res = (msc_host_uninstall_device(this->msc_devices_[slot]->msc_device));
    if (res != ESP_OK) {
      ESP_LOGE(TAG, "msc_host_uninstall_device failed during cleanup: %s", esp_err_to_name(res));
    }
    free(this->msc_devices_[slot]);
    this->msc_devices_[slot] = NULL;
    return err;
  }
  return ESP_OK;
}

/**
 * @brief Free resources associated with a specific MSC device by slot index.
 *
 * This function releases all resources associated with a device identified by its slot index.
 * It unmounts the VFS, uninstalls the MSC device, and frees the allocated memory.
 *
 * @param slot Index of the MSC device in the device array.
 */
void USBMscHost::free_msc_device(int slot) {
  if (slot < 0 || slot >= MAX_MSC_DEVICES || !this->msc_devices_[slot]) {
    ESP_LOGE(TAG, "Invalid slot index for MSC device deallocation");
    return;
  }

  if (this->msc_devices_[slot]->vfs_handle) {
    ESP_ERROR_CHECK(msc_host_vfs_unregister(this->msc_devices_[slot]->vfs_handle));
  }
  if (this->msc_devices_[slot]->msc_device) {
    ESP_ERROR_CHECK(msc_host_uninstall_device(this->msc_devices_[slot]->msc_device));
  }

  free(this->msc_devices_[slot]);
  this->msc_devices_[slot] = NULL;
}

/**
 * @brief Free all connected MSC devices.
 *
 * Iterates over all allocated MSC devices, unmounts them from VFS, and frees their memory.
 */
void USBMscHost::free_all_msc_devices(void) {
  for (int i = 0; i < MAX_MSC_DEVICES; i++) {
    if (this->msc_devices_[i]) {
      free_msc_device(i);
    }
  }
}

/**
 * @brief Find a USB addr by MSC device handle.
 *
 * @param handle MSC device handle
 * @return USB addr, or -1 if not found.
 */
uint8_t USBMscDevice::find_usb_addr_by_handle(msc_host_device_handle_t handle) {
  for (uint8_t i = 0; i < MAX_MSC_DEVICES; i++) {
    if (this->parent_->msc_devices_[i] && this->parent_->msc_devices_[i]->msc_device == handle) {
      return this->parent_->msc_devices_[i]->usb_addr;
    }
  }
  return -1;
}

/**
 * @brief Get MSC device handle by USB addr
 *
 * @param usb_device_handle_t handle
 * @return msc_host_device_handle_t, or nullptr if not found.
 */
msc_host_device_handle_t USBMscHost::get_handle_by_address(uint8_t usb_addr) {
  for (uint8_t i = 0; i < MAX_MSC_DEVICES; i++) {
    if (this->msc_devices_[i] && this->msc_devices_[i]->usb_addr == usb_addr) {
      return this->msc_devices_[i]->msc_device;
    }
  }
  return nullptr;
}

/**
 * @brief Print information about the connected MSC device.
 *
 * This function prints detailed information about the connected USB MSC device,
 * such as capacity, sector size, PID, VID, and string descriptors.
 *
 * @param[in] info Pointer to MSC device information structure.
 */
void USBMscDevice::print_device_info() {
  // Find the slot by USB address
  int8_t slot = this->parent_->find_msc_device_slot(this->device_addr_);
  if (slot < 0) {
    ESP_LOGE(TAG, "Device slot not found for printing device info");
    return;
  }
  msc_host_device_handle_t handle = this->parent_->msc_devices_[slot]->msc_device;

  // Allocate memory for device info structure
  msc_host_device_info_t info;
  esp_err_t err = msc_host_get_device_info(handle, &info);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "msc_host_get_device_info failed: %s", esp_err_to_name(err));
    return;
  }
  const size_t megabyte = 1024 * 1024;
  uint64_t capacity = ((uint64_t) info.sector_size * info.sector_count) / megabyte;

  ESP_LOGI(TAG, "Device info:\n");
  ESP_LOGI(TAG, "\t Capacity: %llu MB\n", capacity);
  ESP_LOGI(TAG, "\t Sector size: %" PRIu32 "\n", info.sector_size);
  ESP_LOGI(TAG, "\t Sector count: %" PRIu32 "\n", info.sector_count);
  ESP_LOGI(TAG, "\t PID: 0x%04X \n", info.idProduct);
  ESP_LOGI(TAG, "\t VID: 0x%04X \n", info.idVendor);
#ifndef CONFIG_LIBC_NEWLIB_NANO_FORMAT
  ESP_LOGI(TAG, "\t iProduct: %S \n", info.iProduct);
  ESP_LOGI(TAG, "\t iManufacturer: %S \n", info.iManufacturer);
  ESP_LOGI(TAG, "\t iSerialNumber: %S \n", info.iSerialNumber);
#endif
}

/**
 * @brief Perform basic file operations on the mounted USB storage device.
 *
 * This function demonstrates basic file I/O operations:
 *  - Create a directory `/usb<slot>/esp` if it does not exist.
 *  - Create a file `test.txt` in the directory with sample content if it does not exist.
 *  - Read the content of the `test.txt` file and print it to the console.
 *
 * @param[in] slot Index of the mounted USB device (0 to MAX_MSC_DEVICES-1).
 */
void USBMscDevice::file_operations() {
  char directory[32];
  char file_path[32];
  snprintf(directory, sizeof(directory), "%s/esp", MNT_PATH);
  snprintf(file_path, sizeof(file_path), "%s/esp/test.txt", MNT_PATH);

  // Create /usb<slot>/esp directory
  struct stat s = {0};
  bool directory_exists = stat(directory, &s) == 0;
  if (!directory_exists) {
    if (mkdir(directory, 0775) != 0) {
      ESP_LOGE(TAG, "mkdir failed with errno: %s", strerror(errno));
    }
  }

  // Create /usb<slot>/esp/test.txt file, if it doesn't exist
  if (stat(file_path, &s) != 0) {
    ESP_LOGI(TAG, "Creating file");
    FILE *f = fopen(file_path, "w");
    if (f == NULL) {
      ESP_LOGE(TAG, "Failed to open file for writing");
      return;
    }
    fprintf(f, "Hello World!\n");
    fclose(f);
  }

  // Read back the file
  FILE *f;
  ESP_LOGI(TAG, "Reading file");
  f = fopen(file_path, "r");
  if (f == NULL) {
    ESP_LOGE(TAG, "Failed to open file for reading");
    return;
  }
  char line[64];
  fgets(line, sizeof(line), f);
  fclose(f);
  // strip newline
  char *pos = strchr(line, '\n');
  if (pos) {
    *pos = '\0';
  }
  ESP_LOGI(TAG, "Read from file '%s': '%s'", file_path, line);
}

/**
 * @brief Perform sequential write and read speed test on the mounted USB storage device.
 *
 * This function performs:
 *  - A write speed test by writing a series of 4KB blocks to a test file.
 *  - A read speed test by reading the same number of 4KB blocks from the file.
 *
 * The results (in MiB/s) are printed to the console.
 *
 * @param[in] slot Index of the mounted USB device (0 to MAX_MSC_DEVICES-1).
 */
void USBMscDevice::speed_test() {
#define ITERATIONS 256  // 256 * 4kb = 1MB
  int64_t test_start, test_end;
  char test_file[32];
  snprintf(test_file, sizeof(test_file), "%s/esp/dummy", MNT_PATH);

  FILE *f = fopen(test_file, "wb+");
  if (f == NULL) {
    ESP_LOGE(TAG, "Failed to open file for writing");
    return;
  }
  // Set larger buffer for this file. It results in larger and more effective USB transfers
  setvbuf(f, NULL, _IOFBF, BUFFER_SIZE);

  // Allocate application buffer used for read/write
  void *data = malloc(BUFFER_SIZE);
  assert(data);

  ESP_LOGI(TAG, "Writing to file %s", test_file);
  test_start = esp_timer_get_time();
  for (int i = 0; i < ITERATIONS; i++) {
    if (fwrite(data, BUFFER_SIZE, 1, f) == 0) {
      ESP_LOGE(TAG, "Write error");
      fclose(f);
      free(data);
      return;
    }
  }
  test_end = esp_timer_get_time();
  ESP_LOGI(TAG, "Write speed %1.2f MiB/s", (BUFFER_SIZE * ITERATIONS) / (float) (test_end - test_start));
  rewind(f);

  ESP_LOGI(TAG, "Reading from file %s", test_file);
  test_start = esp_timer_get_time();
  for (int i = 0; i < ITERATIONS; i++) {
    if (0 == fread(data, BUFFER_SIZE, 1, f)) {
      ESP_LOGE(TAG, "Read error");
      fclose(f);
      free(data);
      return;
    }
  }
  test_end = esp_timer_get_time();
  ESP_LOGI(TAG, "Read speed %1.2f MiB/s", (BUFFER_SIZE * ITERATIONS) / (float) (test_end - test_start));

  fclose(f);
  free(data);
}

/**
 * @brief List contents of the root directories of all mounted USB storage devices.
 *
 * This function iterates over all mounted MSC devices and lists the contents
 * of their root directories. It is intended for debugging and demonstration purposes.
 *
 * For each connected and mounted device, the function attempts to open the root directory
 * `/usb<slot>` and print the names of all files and directories contained within.
 *
 * If opening the directory fails, an error is logged.
 */
void USBMscDevice::list_files() {
  ESP_LOGI(TAG, "ls command output for device:");
  for (int i = 0; i < MAX_MSC_DEVICES; i++) {
    if (this->parent_->msc_devices_[i] != NULL) {
      char mount_path[16];
      snprintf(mount_path, sizeof(mount_path), "%s%d", MNT_PATH, i);

      ESP_LOGI(TAG, "Listing contents of %s", mount_path);
      struct dirent *d;
      DIR *dh = opendir(mount_path);
      if (!dh) {
        ESP_LOGE(TAG, "Failed to open directory: %s", mount_path);
        continue;
      }

      while ((d = readdir(dh)) != NULL) {
        ESP_LOGI("%s/%s\n", mount_path, d->d_name);
      }
      closedir(dh);
    }
  }
}

// Dummy callback for msc_host_install - we don't use MSC library's event system
// Device detection and initialization is handled by usb_host component instead
static void msc_event_callback(const msc_host_event_t *event, void *arg) {
  // Intentionally empty - we handle device events through usb_host component
  // This callback is only here to satisfy msc_host_install() requirements
}

void USBMscHost::setup() {
  ESP_LOGCONFIG(TAG, "Registering USB MSC Host Component...");
  ESP_LOGI(TAG, "USBMscHost setup() called");

  // Initialize MSC host driver (class driver layer, NOT the USB host stack)
  // The USB host stack is already initialized by usb_host component
  const msc_host_driver_config_t msc_config = {
      .create_backround_task = true,
      .task_priority = 5,
      .stack_size = 4096,
      .callback = msc_event_callback,  // Dummy callback - we use usb_host for device detection
  };

  esp_err_t err = msc_host_install(&msc_config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize MSC host driver: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  ESP_LOGI(TAG, "MSC host driver initialized successfully");
}

void USBMscDevice::setup() {
  ESP_LOGCONFIG(TAG, "Registering USB MSC Device (interface-class based handler)");
  ESP_LOGI(TAG, "USBMscDevice setup() called - will be triggered by interface-class matching");
}

void USBMscDevice::dump_config() { ESP_LOGCONFIG(TAG, "USB MSC Device:"); }

// NEW: Interface-class based matching (uses existing find_msc_interface logic)
bool USBMscDevice::matches_device(const usb_config_desc_t *config_desc) {
  const usb_intf_desc_t *msc_intf = find_msc_interface(config_desc);
  if (msc_intf != nullptr) {
    ESP_LOGD(TAG, "Device matched: MSC interface found (Class=0x%02X, SubClass=0x%02X, Protocol=0x%02X)",
             msc_intf->bInterfaceClass, msc_intf->bInterfaceSubClass, msc_intf->bInterfaceProtocol);
    return true;
  }
  ESP_LOGV(TAG, "Device not matched: No MSC interface found");
  return false;
}

// NEW: Called when device is matched and claimed by interface-class handler
void USBMscDevice::on_device_connected(usb_device_handle_t device_handle, uint8_t addr) {
  ESP_LOGI(TAG, "USB MSC Device connected via interface-class matching (address=%d)", addr);

  this->device_addr_ = addr;

  // Close the device handle opened by usb_host - msc_host_install_device() will open it again with its own client
  ESP_LOGD(TAG, "Closing device handle from usb_host before calling msc_host_install_device()");
  if (this->usb_host_ != nullptr) {
    this->usb_host_->close_device_handle(device_handle);
  } else {
    ESP_LOGE(TAG, "usb_host_ is nullptr, cannot close device handle!");
  }

  // Now call msc_host_install_device() which will open the device with its own client handle
  esp_err_t err = this->parent_->allocate_new_msc_device(addr);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to allocate new MSC device: %s", esp_err_to_name(err));
    return;
  }
  ESP_LOGI(TAG, "Allocated memory for new MSC device: %s", esp_err_to_name(err));

  // Print information about the connected disk
  this->print_device_info();
  this->list_files();
  this->file_operations();
  this->speed_test();
}

// NEW: Called when device is disconnected
void USBMscDevice::on_device_disconnected(usb_device_handle_t device_handle) {
  if (this->device_handle_ != device_handle) {
    return;  // Not our device
  }

  ESP_LOGI(TAG, "USB MSC Device disconnected (address=%d)", this->device_addr_);

  uint8_t slot = this->parent_->find_msc_device_slot(this->device_addr_);
  if (slot == (uint8_t) -1) {
    ESP_LOGE(TAG, "Could not find MSC device slot for disconnected device");
  } else {
    this->parent_->free_msc_device(slot);
    ESP_LOGI(TAG, "Freed MSC device resources for slot %d", slot);
  }

  this->device_handle_ = nullptr;
  this->device_addr_ = 255;
}

}  // namespace usb_msc_host
}  // namespace esphome

#endif  // USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3 || USE_ESP32_VARIANT_ESP32P4
