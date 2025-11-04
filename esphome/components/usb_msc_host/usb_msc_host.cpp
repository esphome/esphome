#if defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || defined(USE_ESP32_VARIANT_ESP32P4)

#include "esphome/components/usb_msc_host/usb_msc_host.h"

namespace esphome {
namespace usb_msc_host {

static QueueHandle_t app_queue;
static SemaphoreHandle_t ready_to_uninstall_usb;

#define MNT_PATH "/usb"
static constexpr uint16_t BUFFER_SIZE = 8192;
#define MAX_MSC_DEVICES CONFIG_FATFS_VOLUME_COUNT

/**
 * @brief MSC Device Entry
 *
 * This structure holds information about a connected MSC device,
 * including the USB address, MSC device handle, VFS handle, and assigned mount point.
 */
typedef struct {
  uint8_t usb_addr;                    /*!< USB device address */
  msc_host_device_handle_t msc_device; /*!< Handle of the MSC device */
  msc_host_vfs_handle_t vfs_handle;    /*!< VFS handle assigned to the MSC device */
} msc_dev_entry_t;

static msc_dev_entry_t *msc_devices[MAX_MSC_DEVICES] = {NULL};

/**
 * @brief Application Queue and its messages ID
 */
typedef struct {
  enum {
    APP_QUIT,                 // Signals request to exit the application
    APP_DEVICE_CONNECTED,     // USB device connect event
    APP_DEVICE_DISCONNECTED,  // USB device disconnect event
  } id;
  union {
    uint8_t new_dev_address;                 // Address of new USB device for APP_DEVICE_CONNECTED event
    msc_host_device_handle_t device_handle;  // Handle of removed USB device for APP_DEVICE_DISCONNECTED event
  } data;
} app_message_t;

/**
 * @brief Find a free slot in the device table.
 *
 * @return Index of the free slot, or -1 if no free slot is available.
 */
static inline int find_free_slot(void) {
  for (int i = 0; i < MAX_MSC_DEVICES; i++) {
    if (msc_devices[i] == NULL) {
      ESP_LOGI(TAG, "Found free slot for MSC device at index %d", i);
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
 * @param[in] msg        Message containing the address of the new USB device.
 * @param[out] out_slot  Pointer to store the allocated slot index on success.
 *
 * @return
 *         - ESP_OK on success.
 *         - ESP_ERR_NOT_FOUND if no free slot is available.
 *         - ESP_ERR_NO_MEM if memory allocation fails.
 *         - Other esp_err_t codes if device installation or VFS registration fails.
 */
static esp_err_t allocate_new_msc_device(uint8_t new_dev_address, int *out_slot) {
  int slot = find_free_slot();
  if (slot < 0) {
    ESP_LOGW(TAG, "No free slots for new MSC device (max %d)", MAX_MSC_DEVICES);
    return ESP_ERR_NOT_FOUND;
  }
  // void *slotbuffer = calloc(1, sizeof(msc_dev_entry_t));
  msc_devices[slot] = (msc_dev_entry_t *) calloc(1, sizeof(msc_dev_entry_t));
  if (msc_devices[slot] == NULL) {
    ESP_LOGE(TAG, "Failed to allocate memory for new MSC device entry");
    return ESP_ERR_NO_MEM;
  }

  esp_err_t err = msc_host_install_device(new_dev_address, &msc_devices[slot]->msc_device);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "msc_host_install_device failed: %s", esp_err_to_name(err));
    free(msc_devices[slot]);
    msc_devices[slot] = NULL;
    return err;
  }

  msc_devices[slot]->usb_addr = new_dev_address;

  const esp_vfs_fat_mount_config_t mount_config = {
      .format_if_mount_failed = false,
      .max_files = 5,
      .allocation_unit_size = 1024,
  };

  char mount_path[16];
  snprintf(mount_path, sizeof(mount_path), MNT_PATH "%d", slot);

  err = msc_host_vfs_register(msc_devices[slot]->msc_device, mount_path, &mount_config, &msc_devices[slot]->vfs_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "msc_host_vfs_register failed: %s", esp_err_to_name(err));
    esp_err_t res = (msc_host_uninstall_device(msc_devices[slot]->msc_device));
    if (res != ESP_OK) {
      ESP_LOGE(TAG, "msc_host_uninstall_device failed during cleanup: %s", esp_err_to_name(res));
    }
    free(msc_devices[slot]);
    msc_devices[slot] = NULL;
    return err;
  }

  *out_slot = slot;
  return ESP_OK;
}

/**
 * @brief Find a slot by MSC device handle.
 *
 * This function searches for the slot corresponding to a given MSC device handle.
 *
 * @param handle MSC device handle to search for.
 * @return Index of the slot if found, otherwise -1.
 */
static int find_slot_by_handle(msc_host_device_handle_t handle) {
  for (int i = 0; i < MAX_MSC_DEVICES; i++) {
    if (msc_devices[i] && msc_devices[i]->msc_device == handle) {
      return i;
    }
  }
  return -1;
}

/**
 * @brief Free resources associated with a specific MSC device by slot index.
 *
 * This function releases all resources associated with a device identified by its slot index.
 * It unmounts the VFS, uninstalls the MSC device, and frees the allocated memory.
 *
 * @param slot Index of the MSC device in the device array.
 */
static void free_msc_device(int slot) {
  if (slot < 0 || slot >= MAX_MSC_DEVICES || !msc_devices[slot]) {
    ESP_LOGE(TAG, "Invalid slot index for MSC device deallocation");
    return;
  }

  if (msc_devices[slot]->vfs_handle) {
    ESP_ERROR_CHECK(msc_host_vfs_unregister(msc_devices[slot]->vfs_handle));
  }
  if (msc_devices[slot]->msc_device) {
    ESP_ERROR_CHECK(msc_host_uninstall_device(msc_devices[slot]->msc_device));
  }

  free(msc_devices[slot]);
  msc_devices[slot] = NULL;
}

/**
 * @brief Free all connected MSC devices.
 *
 * Iterates over all allocated MSC devices, unmounts them from VFS, and frees their memory.
 */
static void free_all_msc_devices(void) {
  for (int i = 0; i < MAX_MSC_DEVICES; i++) {
    if (msc_devices[i]) {
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
static inline int8_t find_usb_addr_by_handle(msc_host_device_handle_t handle) {
  for (int8_t i = 0; i < MAX_MSC_DEVICES; i++) {
    if (msc_devices[i] && msc_devices[i]->msc_device == handle) {
      return msc_devices[i]->usb_addr;
    }
  }
  return -1;
}

/**
 * @brief Print information about the connected MSC device.
 *
 * This function prints detailed information about the connected USB MSC device,
 * such as capacity, sector size, PID, VID, and string descriptors.
 *
 * @param[in] info Pointer to MSC device information structure.
 */
static void print_device_info(msc_host_device_info_t *info) {
  const size_t megabyte = 1024 * 1024;
  uint64_t capacity = ((uint64_t) info->sector_size * info->sector_count) / megabyte;

  ESP_LOGI(TAG, "Device info:\n");
  ESP_LOGI(TAG, "\t Capacity: %llu MB\n", capacity);
  ESP_LOGI(TAG, "\t Sector size: %" PRIu32 "\n", info->sector_size);
  ESP_LOGI(TAG, "\t Sector count: %" PRIu32 "\n", info->sector_count);
  ESP_LOGI(TAG, "\t PID: 0x%04X \n", info->idProduct);
  ESP_LOGI(TAG, "\t VID: 0x%04X \n", info->idVendor);
#ifndef CONFIG_LIBC_NEWLIB_NANO_FORMAT
  ESP_LOGI(TAG, "\t iProduct: %S \n", info->iProduct);
  ESP_LOGI(TAG, "\t iManufacturer: %S \n", info->iManufacturer);
  ESP_LOGI(TAG, "\t iSerialNumber: %S \n", info->iSerialNumber);
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
static void file_operations(int slot) {
  char directory[32];
  char file_path[32];
  snprintf(directory, sizeof(directory), MNT_PATH "%d/esp", slot);
  snprintf(file_path, sizeof(file_path), MNT_PATH "%d/esp/test.txt", slot);

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
static void speed_test(int slot) {
#define ITERATIONS 256  // 256 * 4kb = 1MB
  int64_t test_start, test_end;
  char test_file[32];
  snprintf(test_file, sizeof(test_file), MNT_PATH "%d/esp/dummy", slot);

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
static inline void show_list_files_all_devices(void) {
  ESP_LOGI(TAG, "ls command output for all connected devices:");
  for (int i = 0; i < MAX_MSC_DEVICES; i++) {
    if (msc_devices[i] != NULL) {
      char mount_path[16];
      snprintf(mount_path, sizeof(mount_path), MNT_PATH "%d", i);

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

static void msc_event_cb(const msc_host_event_t *event, void *arg) {
  if (event->event == msc_host_event_t::MSC_DEVICE_CONNECTED) {
    ESP_LOGI(TAG, "MSC device connected");
    int slot;
    esp_err_t res = allocate_new_msc_device(event->device.address, &slot);
    if (res != ESP_OK) {
      ESP_LOGE(TAG, "Failed to allocate new MSC device: %s", esp_err_to_name(res));
      return;
    }
    ESP_LOGI(TAG, "Allocated memory for new MSC device: %s", esp_err_to_name(res));
    // 2. Print information about the connected disk
    msc_host_device_info_t info;
    esp_err_t err = msc_host_get_device_info(msc_devices[slot]->msc_device, &info);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "msc_host_get_device_info failed: %s", esp_err_to_name(err));
      return;
    }
    // msc_host_print_descriptors(msc_devices[slot]->msc_device);
    print_device_info(&info);

    // 3. List all the files in root directory from all connected msc devices
    show_list_files_all_devices();

    // 4. The disk is mounted to Virtual File System, perform some basic demo file operation
    file_operations(slot);
    // speed_test(slot);

    ESP_LOGI(TAG, "Example finished, you can disconnect the USB flash drive (or connect another USB flash drive)");
  } else if (event->event == msc_host_event_t::MSC_DEVICE_DISCONNECTED) {
    ESP_LOGI(TAG, "MSC device disconnected");
  }
  xQueueSend(app_queue, event, 10);
}

void USBMscHost::setup() {
  ESP_LOGCONFIG(TAG, "Setting up USB MSC Host");

  BaseType_t task_created;
  ready_to_uninstall_usb = xSemaphoreCreateBinary();

  app_queue = xQueueCreate(3, sizeof(msc_host_event_t));
  assert(app_queue);

  const msc_host_driver_config_t msc_config = {
      .create_backround_task = true,
      .task_priority = 5,
      .stack_size = 4096,
      .callback = msc_event_cb,
  };
  ESP_ERROR_CHECK(msc_host_install(&msc_config));
}

void USBMscHost::dump_config() { ESP_LOGCONFIG(TAG, "USB MSC Host:"); }

void USBMscDevice::setup() { ESP_LOGCONFIG(TAG, "Registering USB MSC Device"); }

void USBMscDevice::dump_config() { ESP_LOGCONFIG(TAG, "USB MSC Device:"); }

void USBMscDevice::on_connected() { ESP_LOGCONFIG(TAG, "USB MSC Device connected"); }

void USBMscDevice::disconnect() { ESP_LOGCONFIG(TAG, "USB MSC Device disconnected"); }

}  // namespace usb_msc_host
}  // namespace esphome

#endif  // USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3 || USE_ESP32_VARIANT_ESP32P4
