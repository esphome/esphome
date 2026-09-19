/* Host (Linux) side of the I2C passthrough bridge -- compiled into the
 * native_simulator runner, not the Zephyr application. Talks to a real
 * /dev/i2c-N device via the standard Linux i2c-dev ioctl interface. */

#ifdef USE_ZEPHYR_VARIANT_NATIVE_SIM

#include "i2c_passthrough_bottom.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <unistd.h>
namespace esphome::zephyr {
int i2c_passthrough_open(const char *linux_bus) {
  int fd = open(linux_bus, O_RDWR);
  if (fd < 0)
    return -errno;
  return fd;
}

void i2c_passthrough_close(int fd) {
  if (fd >= 0)
    close(fd);
}

int i2c_passthrough_transfer(int fd, uint16_t addr, const unsigned char *write_buf, unsigned int write_len,
                             unsigned char *read_buf, unsigned int read_len) {
  struct i2c_msg msgs[2];
  struct i2c_rdwr_ioctl_data ioctl_data;
  int num_msgs = 0;

  if (write_len > 0) {
    msgs[num_msgs].addr = addr;
    msgs[num_msgs].flags = 0;
    msgs[num_msgs].len = write_len;
    msgs[num_msgs].buf = (unsigned char *) write_buf;
    num_msgs++;
  }
  if (read_len > 0) {
    msgs[num_msgs].addr = addr;
    msgs[num_msgs].flags = I2C_M_RD;
    msgs[num_msgs].len = read_len;
    msgs[num_msgs].buf = read_buf;
    num_msgs++;
  }
  if (num_msgs == 0) {
    struct i2c_msg probe_msg;
    probe_msg.addr = addr;
    probe_msg.flags = 0;
    probe_msg.len = 0;
    probe_msg.buf = nullptr;
    ioctl_data.msgs = &probe_msg;
    ioctl_data.nmsgs = 1;
    if (ioctl(fd, I2C_RDWR, &ioctl_data) < 0)
      return -errno;
    return 0;
  }

  ioctl_data.msgs = msgs;
  ioctl_data.nmsgs = (unsigned int) num_msgs;

  if (ioctl(fd, I2C_RDWR, &ioctl_data) < 0)
    return -errno;
  return 0;
}
}  // namespace esphome::zephyr

#endif  // USE_ZEPHYR_VARIANT_NATIVE_SIM
