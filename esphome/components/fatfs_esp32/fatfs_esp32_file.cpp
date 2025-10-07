// #include "fatfs_esp32.h"
#include "esphome/core/log.h"
#include "esphome/core/time.h"

#include "fatfs_esp32_file.h"

extern "C" {
#include "ff.h"
#include "diskio.h"
#if ESP_IDF_VERSION_MAJOR > 3
#include "diskio_impl.h"
#endif
#include "esp_vfs_fat.h"
}

namespace esphome {
namespace fatfs_esp32 {

static const char *const TAG = "fatfs_esp32_file";

// --------------------------------------------------------------------------------

FatESP32Info::FatESP32Info(std::string path) {
  ESP_LOGV(TAG, "Init fatInfo. Path=%s ", path.c_str());
  exist_ = false;

  // Cut off drive part
  //
  if (path.length() > 2 && path[1] == ':') {
    drive_ = path.substr(0, 2);
    path_ = path.substr(2, path.length() - 1);
  } else {
    path_ = path;
    drive_ = std::string("0:");
    // TODO:  get first inserted drive
  }

  // Cut off trailing  '/'  if path non root
  //
  if ((path_.length() > 1) && (path_[path_.length() - 1] == '/')) {
    path_ = path_.substr(0, path_.length() - 2);
    ESP_LOGV(TAG, "Subst trail space |%s|", path_.c_str());
  }

  //  Devide for path and name
  size_t pos = path_.rfind('/');
  if (pos == std::string::npos) {
    name_ = path_;
    path_ = std::string("");
  } else {
    //  Devide for path and name
    name_ = path_.substr(pos + 1);
    path_ = path_.substr(0, pos + 1);
  }

  ESP_LOGV(TAG, "Parse input path. %s (drv=%s, path=%s, name=%s)", path.c_str(), drive_.c_str(), path_.c_str(),
           name_.c_str());

  // Check for root object
  if (path_.empty() && name_.empty()) {
    return;
  }

  //   Get infot object

  FILINFO finfo;
  FRESULT res = f_stat(this->get_full_path().c_str(), &finfo);
  ESP_LOGV(TAG, "f_stat for  %s, rc=%d", this->get_full_path().c_str(), res);
  if (res == FR_NO_FILE || res == FR_NO_PATH) {
    return;
  }

  else if (res != FR_OK) {
    ESP_LOGE(TAG, "f_stat %s error: (0x%x) %s", (drive_ + path_ + name_).c_str(), res, fs_errstr(res));
    // throw std::runtime_error(err_str);
    return;
  }

  exist_ = true;

  // https://elm-chan.org/fsw/ff/doc/sfileinfo.html

  // this->load(this->get_full_path(), &finfo);
  // TODO:  Place loading code here
  size_ = finfo.fsize;
  name_ = std::string(finfo.fname);
  is_dir_ = finfo.fattrib & AM_DIR;
  is_hidden_ = finfo.fattrib & AM_HID;
  is_system_ = finfo.fattrib & AM_SYS;
  is_ro_ = finfo.fattrib & AM_RDO;

  create_date_.year = (finfo.fdate >> 9) + 1980;
  create_date_.month = finfo.fdate >> 5 & 15;
  create_date_.day_of_month = finfo.fdate & 31;
  create_date_.hour = finfo.ftime >> 11;
  create_date_.minute = finfo.ftime >> 5 & 63;
  create_date_.second = 0;

  ESP_LOGV(TAG, "Load %s info %s", is_dir_ ? "DIR" : "FILE", name_.c_str());
}

// --------------------------------------------------------------------------------

FatESP32Info::FatESP32Info(fatfs::FatInfo &source) {
  *this = source;
  drive_ = source.get_drive();
  path_ = source.get_path();
  name_ = source.get_name();

  ESP_LOGVV(TAG, "Get FatInfo Object. d=%s, p=%s, n=%s", drive_.c_str(), path_.c_str(), name_.c_str());

  if (source.is_exist()) {
    exist_ = true;
    size_ = source.size();
    is_dir_ = source.is_dir();
    is_hidden_ = source.is_hidden();
    is_system_ = source.is_sys();
    is_ro_ = source.is_readonly();
    create_date_ = std::move(*(source.get_cr_date()));
  }

  // drive_ = source->get_drive();
  // path_ = source->get_path();
  // name_ = source->get_name();

  // ESP_LOGVV(TAG, "Get FatInfo Object. d=%s, p=%s, n=%s", drive_.c_str(), path_.c_str(), name_.c_str());

  // if (source->is_exist()) {
  //   exist_ = true;
  //   size_ = source->size();
  //   is_dir_ = source->is_dir();
  //   is_hidden_ = source->is_hidden();
  //   is_system_ = source->is_sys();
  //   is_ro_ = source->is_readonly();
  //   create_date_ = std::move(*(source->get_cr_date()));
  // }
}

// --------------------------------------------------------------------------------

// void FatESP32Info::load(const std::string path, FILINFO *finfo) {
//   size_ = finfo->fsize;
//   name_ = std::string(finfo->fname);
//   is_dir_ = finfo->fattrib & AM_DIR;
//   is_hidden_ = finfo->fattrib & AM_HID;
//   is_system_ = finfo->fattrib & AM_SYS;
//   is_ro_ = finfo->fattrib & AM_RDO;

//   create_date_.year = (finfo->fdate >> 9) + 1980;
//   create_date_.month = finfo->fdate >> 5 & 15;
//   create_date_.day_of_month = finfo->fdate & 31;
//   create_date_.hour = finfo->ftime >> 11;
//   create_date_.minute = finfo->ftime >> 5 & 63;
//   create_date_.second = 0;

//   ESP_LOGV(TAG, "Load %s info %s, size=%d", is_dir_ ? "DIR" : "FILE", name_.c_str(), size_);
// }

// --------------------------------------------------------------------------------

FatESP32Info &FatESP32Info::operator=(const FatESP32Info &source) {
  if (this == &source)
    return *this;

  drive_ = source.drive_;
  path_ = source.path_;
  name_ = source.name_;

  if (source.exist_) {
    exist_ = true;
    size_ = source.size_;
    is_dir_ = source.is_dir_;
    is_hidden_ = source.is_hidden_;
    is_system_ = source.is_system_;
    is_ro_ = source.is_ro_;
    create_date_ = source.create_date_;
  }
  return *this;
}

// --------------------------------------------------------------------------------

std::string FatESP32Info::get_full_path() { return drive_ + path_ + name_; };

// --------------------------------------------------------------------------------

FatESP32File::FatESP32File(std::string path, uint16_t mode) : FatESP32Info{std::move(path)} { this->open(mode); }

// --------------------------------------------------------------------------------

FatESP32File::FatESP32File(fatfs::FatInfo &finfo, uint16_t mode) : FatESP32Info{finfo} { this->open(mode); }

// --------------------------------------------------------------------------------

FatESP32File::~FatESP32File() {
  if (is_open_) {
    f_close(&fptr_);
    ESP_LOGV(TAG, "Close file %s. Delete object.", this->FatESP32Info::get_full_path().c_str());
  }
}

// --------------------------------------------------------------------------------

bool FatESP32File::open(uint16_t mode) {
  bool ret = false;
  if (this->is_open_) {
    ret = true;
  } else if (this->FatESP32Info::is_exist() || mode & FAT_F_CREATE_ALWAYS) {
    error_ = f_open(&fptr_, this->FatESP32Info::get_full_path().c_str(), mode);
    if (error_ != FR_OK) {
      ESP_LOGE(TAG, "Open %s error (0x%x) %s", this->FatESP32Info::get_full_path().c_str(), error_, fs_errstr(error_));
    } else {
      ESP_LOGV(TAG, "Open file %s ...", this->FatESP32Info::get_full_path().c_str());
      is_open_ = true;
      ret = true;
    }
  }
  return ret;
}

// --------------------------------------------------------------------------------

void FatESP32File::close() {
  if (is_open_) {
    error_ = f_close(&fptr_);
    if (error_ != FR_OK)
      ESP_LOGE(TAG, "Close error (0x%x) %s", error_, fs_errstr(error_));
    is_open_ = false;
    ESP_LOGV(TAG, "Close file %s, OK", this->FatESP32Info::get_full_path().c_str());
  }
}

// --------------------------------------------------------------------------------

int32_t FatESP32File::read(void *buf, size_t size) {
  UINT real_rd = 0;
  if (is_open_) {
    error_ = f_read(&fptr_, buf, size, &real_rd);
    if (error_ != FR_OK) {
      ESP_LOGE(TAG, "Read error (0x%x) %s", error_, fs_errstr(error_));
      return -1;
    }
    // else  if(f_eof(&fptr_)) real_rd = 0;   // (fp)->obj.objsize
  }
  return real_rd;
}

// --------------------------------------------------------------------------------

int32_t FatESP32File::write(void *buf, size_t size) {
  UINT real_wr = 0;
  if (is_open_) {
    error_ = f_write(&fptr_, buf, size, &real_wr);
    if (error_ != FR_OK) {
      ESP_LOGE(TAG, "Write error (0x%x) %s", error_, fs_errstr(error_));
      return -1;
    }
  }
  return real_wr;
}

// --------------------------------------------------------------------------------

bool FatESP32File::lseek(size_t pos) {
  if (is_open_) {
    error_ = f_lseek(&fptr_, pos);
    if (error_ != FR_OK) {
      ESP_LOGE(TAG, "Lseek error (0x%x) %s", error_, fs_errstr(error_));
      return false;
    }
  }
  return true;
}

// --------------------------------------------------------------------------------

bool FatESP32File::truncate() {
  if (is_open_) {
    error_ = f_truncate(&fptr_);
    if (error_ != FR_OK) {
      ESP_LOGE(TAG, "Truncate error (0x%x) %s", error_, fs_errstr(error_));
      return false;
    }
  }
  return true;
}

// --------------------------------------------------------------------------------

void FatESP32File::flush() {
  if (is_open_) {
    error_ = f_truncate(&fptr_);
    if (error_ != FR_OK) {
      ESP_LOGE(TAG, "Flush error (0x%x) %s", error_, fs_errstr(error_));
    }
  }
}

// --------------------------------------------------------------------------------

uint32_t FatESP32File::get_pos() {
  if (is_open_) {
    return f_tell(&fptr_);
  }
  return 0;
}

// --------------------------------------------------------------------------------

bool FatESP32File::is_eof() { return f_eof(&fptr_); }

// --------------------------------------------------------------------------------

FatESP32Dir::FatESP32Dir(std::string path) : FatESP32Info{path} { this->open(); }

// --------------------------------------------------------------------------------

FatESP32Dir::FatESP32Dir(fatfs::FatInfo &finfo) : FatESP32Info{finfo} { this->open(); }

// --------------------------------------------------------------------------------

FatESP32Dir::~FatESP32Dir() {
  if (is_open_) {
    f_closedir(&dptr_);
  }
}

// --------------------------------------------------------------------------------

bool FatESP32Dir::open() {
  ESP_LOGV(TAG, "Open dir %s", this->FatESP32Info::get_full_path().c_str());

  error_ = f_opendir(&dptr_, this->FatESP32Info::get_full_path().c_str());

  if (error_ != FR_OK) {
    is_open_ = false;
  } else {
    is_open_ = true;
  }
  return is_open_;
}

// --------------------------------------------------------------------------------

fatfs::FatInfo *FatESP32Dir::get_next() {
  ESP_LOGVV(TAG, "Get next dir %s object", this->FatESP32Info::get_full_path().c_str());
  FILINFO fptr;
  if (is_open_) {
    error_ = f_readdir(&dptr_, &fptr);
    if (error_ != FR_OK) {
      ESP_LOGE(TAG, "Read dir entry error %s", fs_errstr(error_));
      return NULL;
    }
    if (fptr.fname[0] == 0) {
      return NULL;  //  Mean end of list
    }
    // return new FatESP32Info(this->get_full_path() + "/" + std::string(fptr.fname));
    ESP_LOGV(TAG, "Read next object %s from directory %s (d=%s,p=%s,n=%s)", fptr.fname, this->get_full_path().c_str(),
             this->FatESP32Info::get_drive().c_str(), this->FatESP32Info::get_path().c_str(),
             this->FatESP32Info::get_name().c_str());

    if (this->FatESP32Info::get_name().empty()) {
      return new FatESP32Info(this->FatESP32Info::get_full_path() + std::string(fptr.fname));
    } else {
      return new FatESP32Info(this->FatESP32Info::get_full_path() + "/" + std::string(fptr.fname));
    }
  }
  return NULL;
}

// --------------------------------------------------------------------------------

bool FatESP32Dir::reset() {
  if (is_open_) {
    error_ = f_closedir(&dptr_);
    if (error_ != FR_OK) {
      return false;
    }
  }
  error_ = f_opendir(&dptr_, this->FatESP32Info::get_full_path().c_str());
  if (error_ != FR_OK) {
    return false;
  }

  return true;
}

}  // namespace fatfs_esp32
}  // namespace esphome
