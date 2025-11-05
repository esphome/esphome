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
 * @param[in] mount_path         VFS mount path for this device
 *
 * @return
 *         - ESP_OK on success.
 *         - ESP_ERR_NOT_FOUND if no free slot is available.
 *         - ESP_ERR_NO_MEM if memory allocation fails.
 *         - Other esp_err_t codes if device installation or VFS registration fails.
 */
esp_err_t USBMscHost::allocate_new_msc_device(uint8_t new_dev_address, const std::string &mount_path) {
  int slot = this->find_free_slot();
  if (slot < 0) {
    ESP_LOGW(TAG, "No free slots for new MSC device (max %d)", MAX_MSC_DEVICES);
    return ESP_ERR_NOT_FOUND;
  }

  ESP_LOGI(TAG, "Allocating slot %d for device address %d with mount path '%s'", slot, new_dev_address,
           mount_path.c_str());

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

  err = msc_host_vfs_register(this->msc_devices_[slot]->msc_device, mount_path.c_str(), &mount_config,
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
 *  - Create a directory `<mount_path>/esp` if it does not exist.
 *  - Create a file `test.txt` in the directory with sample content if it does not exist.
 *  - Read the content of the `test.txt` file and print it to the console.
 */
void USBMscDevice::file_operations() {
  std::string directory = this->mount_path_ + "/esp";
  std::string file_path = this->mount_path_ + "/esp/test.txt";

  // Create <mount_path>/esp directory
  struct stat s = {0};
  bool directory_exists = stat(directory.c_str(), &s) == 0;
  if (!directory_exists) {
    if (mkdir(directory.c_str(), 0775) != 0) {
      ESP_LOGE(TAG, "mkdir failed with errno: %s", strerror(errno));
    }
  }

  // Create <mount_path>/esp/test.txt file, if it doesn't exist
  if (stat(file_path.c_str(), &s) != 0) {
    ESP_LOGI(TAG, "Creating file");
    FILE *f = fopen(file_path.c_str(), "w");
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
  f = fopen(file_path.c_str(), "r");
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
  ESP_LOGI(TAG, "Read from file '%s': '%s'", file_path.c_str(), line);
}

/**
 * @brief Perform sequential write and read speed test on the mounted USB storage device.
 *
 * This function performs:
 *  - A write speed test by writing a series of 4KB blocks to a test file.
 *  - A read speed test by reading the same number of 4KB blocks from the file.
 *
 * The results (in MiB/s) are printed to the console.
 */
void USBMscDevice::speed_test() {
#define ITERATIONS 256  // 256 * 4kb = 1MB
  int64_t test_start, test_end;
  std::string test_file = this->mount_path_ + "/esp/dummy";

  FILE *f = fopen(test_file.c_str(), "wb+");
  if (f == NULL) {
    ESP_LOGE(TAG, "Failed to open file for writing");
    return;
  }
  // Set larger buffer for this file. It results in larger and more effective USB transfers
  setvbuf(f, NULL, _IOFBF, BUFFER_SIZE);

  // Allocate application buffer used for read/write
  void *data = malloc(BUFFER_SIZE);
  assert(data);

  ESP_LOGI(TAG, "Writing to file %s", test_file.c_str());
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

  ESP_LOGI(TAG, "Reading from file %s", test_file.c_str());
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
 * @brief List contents of the root directory of this mounted USB storage device.
 *
 * This function lists the contents of the root directory for this specific device.
 * It is intended for debugging and demonstration purposes.
 *
 * If opening the directory fails, an error is logged.
 */
void USBMscDevice::list_files() {
  ESP_LOGI(TAG, "Listing contents of '%s'", this->mount_path_.c_str());

  struct dirent *d;
  DIR *dh = opendir(this->mount_path_.c_str());
  if (!dh) {
    ESP_LOGE(TAG, "Failed to open directory: %s", this->mount_path_.c_str());
    return;
  }

  while ((d = readdir(dh)) != NULL) {
    ESP_LOGI(TAG, "  %s/%s", this->mount_path_.c_str(), d->d_name);
  }
  closedir(dh);
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

// Interface-class based matching with optional VID/PID filtering
bool USBMscDevice::matches_device(const usb_config_desc_t *config_desc) {
  ESP_LOGD(TAG, "matches_device() called - checking if device is MSC (vid_filter=0x%04X, pid_filter=0x%04X)",
           this->vid_, this->pid_);

  // First check if device has MSC interface
  const usb_intf_desc_t *msc_intf = find_msc_interface(config_desc);
  if (msc_intf == nullptr) {
    ESP_LOGD(TAG, "Device not matched: No MSC interface found");
    return false;
  }

  ESP_LOGD(TAG, "MSC interface found: Class=0x%02X, SubClass=0x%02X, Protocol=0x%02X", msc_intf->bInterfaceClass,
           msc_intf->bInterfaceSubClass, msc_intf->bInterfaceProtocol);

  // If both VID and PID are 0x0000 (wildcard), match any MSC device
  if (this->vid_ == 0x0000 && this->pid_ == 0x0000) {
    ESP_LOGD(TAG, "Device matched: MSC interface with wildcard VID/PID");
    return true;
  }

  // VID/PID filtering requested - need to check device descriptor
  // We need to get the device descriptor properly using the device handle
  // The config_desc alone doesn't contain VID/PID - we need the device descriptor
  // This is a problem - we don't have access to device_handle here!
  // For now, we'll accept any MSC device if VID/PID is specified (handler will filter later)
  ESP_LOGW(TAG,
           "VID/PID filtering requested but device descriptor not available in matches_device() - accepting MSC device "
           "for now");
  return true;
}

// Called when device is matched and claimed by interface-class handler
void USBMscDevice::on_device_connected(usb_device_handle_t device_handle, uint8_t addr) {
  ESP_LOGI(TAG, "USB MSC Device connected via interface-class matching (address=%d, mount_path='%s')", addr,
           this->mount_path_.c_str());

  // VID/PID filtering (if specified)
  if (this->vid_ != 0x0000 || this->pid_ != 0x0000) {
    // Get device descriptor to check VID/PID
    const usb_device_desc_t *device_desc;
    esp_err_t err = usb_host_get_device_descriptor(device_handle, &device_desc);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to get device descriptor for VID/PID check: %s", esp_err_to_name(err));
      if (this->usb_host_ != nullptr) {
        this->usb_host_->close_device_handle(device_handle);
      }
      return;
    }

    bool vid_match = (this->vid_ == 0x0000) || (device_desc->idVendor == this->vid_);
    bool pid_match = (this->pid_ == 0x0000) || (device_desc->idProduct == this->pid_);

    if (!vid_match || !pid_match) {
      ESP_LOGD(TAG, "Device VID/PID (0x%04X/0x%04X) doesn't match filter (0x%04X/0x%04X) - rejecting",
               device_desc->idVendor, device_desc->idProduct, this->vid_, this->pid_);
      if (this->usb_host_ != nullptr) {
        this->usb_host_->close_device_handle(device_handle);
      }
      return;
    }

    ESP_LOGD(TAG, "Device VID/PID (0x%04X/0x%04X) matches filter", device_desc->idVendor, device_desc->idProduct);
  }

  this->device_addr_ = addr;

  // Close the device handle opened by usb_host - msc_host_install_device() will open it again with its own client
  ESP_LOGD(TAG, "Closing device handle from usb_host before calling msc_host_install_device()");
  if (this->usb_host_ != nullptr) {
    this->usb_host_->close_device_handle(device_handle);
  } else {
    ESP_LOGE(TAG, "usb_host_ is nullptr, cannot close device handle!");
  }

  // Now call msc_host_install_device() which will open the device with its own client handle
  esp_err_t err = this->parent_->allocate_new_msc_device(addr, this->mount_path_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to allocate new MSC device: %s", esp_err_to_name(err));
    return;
  }

  // Find the slot that was just allocated for this device
  this->slot_ = this->parent_->find_msc_device_slot(addr);
  if (this->slot_ < 0) {
    ESP_LOGE(TAG, "Failed to find slot for newly allocated device!");
    return;
  }

  ESP_LOGI(TAG, "Successfully allocated MSC device to slot %d", this->slot_);

  // Print information about the connected disk
  this->print_device_info();
  this->list_files();
  this->file_operations();
  this->speed_test();

  // NEW: Notify all registered callbacks that mount is ready
  ESP_LOGI(TAG, "Notifying %zu mount ready callbacks for '%s'", this->mount_ready_callbacks_.size(),
           this->mount_path_.c_str());
  for (const auto &callback : this->mount_ready_callbacks_) {
    callback(this->mount_path_);
  }
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

// NEW: Public mount/unmount methods
bool USBMscDevice::remount_device() {
  if (this->device_addr_ == 255) {
    ESP_LOGW(TAG, "No device connected, cannot remount");
    return false;
  }

  // Unmount first if already mounted
  if (this->slot_ >= 0) {
    this->unmount_device();
  }

  // Re-allocate and mount
  esp_err_t err = this->parent_->allocate_new_msc_device(this->device_addr_, this->mount_path_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to remount MSC device: %s", esp_err_to_name(err));
    return false;
  }

  // Find the slot that was just allocated
  this->slot_ = this->parent_->find_msc_device_slot(this->device_addr_);
  if (this->slot_ < 0) {
    ESP_LOGE(TAG, "Failed to find slot for remounted device!");
    return false;
  }

  ESP_LOGI(TAG, "Device remounted successfully to '%s'", this->mount_path_.c_str());

  // Notify callbacks
  for (const auto &callback : this->mount_ready_callbacks_) {
    callback(this->mount_path_);
  }

  return true;
}

void USBMscDevice::unmount_device() {
  if (this->slot_ < 0) {
    ESP_LOGD(TAG, "Device not mounted, nothing to unmount");
    return;
  }

  ESP_LOGI(TAG, "Unmounting MSC device from slot %d (mount path: '%s')", this->slot_, this->mount_path_.c_str());
  this->parent_->free_msc_device(this->slot_);
  this->slot_ = -1;
  ESP_LOGI(TAG, "Device unmounted successfully");
}

}  // namespace usb_msc_host
}  // namespace esphome

#endif  // USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3 || USE_ESP32_VARIANT_ESP32P4
