#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <string>

#include "esphome/components/canopen/od.h"

namespace esphome::canopen::testing {

// upload a whole VAR through CANopenNode's OD interface, as the SDO server does
static std::string read_var(ObjectDictionary &od, uint16_t index, OD_attr_t *attr = nullptr) {
  OD_IO_t io;
  EXPECT_EQ(OD_getSub(od.find(index), 0, &io, false), ODR_OK);
  if (attr != nullptr)
    *attr = io.stream.attribute;
  char buf[64];
  OD_size_t n = 0;
  EXPECT_EQ(io.read(&io.stream, buf, sizeof(buf), &n), ODR_OK);
  return std::string(buf, n);
}

TEST(CanopenOD, Strings) {
  static const char NAME[] = "canopen test node";
  static const char EMPTY[] = "";
  ObjectDictionary od;
  // added out of order: finalize() sorts
  od.add_string(0x100A, "2026.11.0");
  od.add_string(0x1008, NAME);
  od.add_string(0x1009, EMPTY);
  od.finalize();

  OD_attr_t attr = 0;
  EXPECT_EQ(read_var(od, 0x1008, &attr), NAME);
  // read only, no ODA_STR (size known up front) and no ODA_MB (no byte order)
  EXPECT_EQ(attr, ODA_SDO_R);
  EXPECT_EQ(read_var(od, 0x100A), "2026.11.0");
  // an empty string is one zero byte: CANopenNode aborts zero-length uploads
  EXPECT_EQ(read_var(od, 0x1009), std::string(1, '\0'));
  EXPECT_EQ(od.find(0x1007), nullptr);
}

TEST(CanopenOD, Record) {
  uint8_t highest = 2;
  uint32_t a = 0x11223344, b = 0;
  ObjectDictionary od;
  od.add_record(0x1200, {{0, ODA_SDO_R, &highest, 1}, {1, ODA_SDO_R, &a, 4}, {2, ODA_SDO_RW, &b, 4}});
  od.finalize();

  OD_entry_t *entry = od.find(0x1200);
  ASSERT_NE(entry, nullptr);
  uint32_t value = 0;
  EXPECT_EQ(OD_get_u32(entry, 1, &value, true), ODR_OK);
  EXPECT_EQ(value, 0x11223344u);
  // CANopenNode writes the default COB-IDs into 0x1200 this way (original data, no attribute check)
  EXPECT_EQ(OD_set_u32(entry, 2, 0x5FF, true), ODR_OK);
  EXPECT_EQ(b, 0x5FFu);
  uint8_t count = 0;
  EXPECT_EQ(OD_get_u8(entry, 0, &count, true), ODR_OK);
  EXPECT_EQ(count, 2);
}

// overrides only od_write(): reads use the storage, writes are counted
struct CountingObject : OdObject {
  int writes{0};
  uint8_t last_sub{0};
  OdResult od_write(OdStream stream, const void *buf, size_t count, size_t *done) {
    this->writes++;
    this->last_sub = stream.sub();
    return stream.write_original(buf, count, done);
  }
};

TEST(CanopenOD, ValueSize) {
  uint8_t u8 = 0;
  uint32_t u32 = 0;
  CountingObject handler;
  ObjectDictionary od;
  od.add_var(0x2000, OD_READ_WRITE, &u8, sizeof(u8));
  od.add_var(0x2001, OD_READ_WRITE, &u32, sizeof(u32));
  od.add_var(0x2002, OD_READ_WRITE, &u32, sizeof(u32));
  od.set_object(0x2002, &handler);
  EXPECT_EQ(od.value_size(0x2000, 0), 0u);  // before finalize()
  ASSERT_TRUE(od.finalize());
  EXPECT_EQ(od.value_size(0x2000, 0), 1u);
  EXPECT_EQ(od.value_size(0x2001, 0), 4u);
  EXPECT_EQ(od.value_size(0x2000, 1), 0u);  // no such sub-index
  EXPECT_EQ(od.value_size(0x2FFF, 0), 0u);  // no such entry
  EXPECT_EQ(od.value_size(0x2002, 0), 4u);
  EXPECT_EQ(handler.writes, 0);  // handlers don't run
}

// a read-only domain without storage, generated in chunks: 100 bytes 0, 1, 2, ...
struct GeneratedObject : OdObject {
  OdResult od_read(OdStream stream, void *buf, size_t count, size_t *done) {
    if (stream.offset() == 0)
      stream.set_size(100);  // size indication for the upload
    size_t n = std::min(count, stream.size() - stream.offset());
    for (size_t i = 0; i < n; i++)
      static_cast<uint8_t *>(buf)[i] = stream.offset() + i;
    stream.set_offset(stream.offset() + n);
    *done = n;
    return stream.offset() < stream.size() ? OdResult::OD_RESULT_PARTIAL : OdResult::OD_RESULT_OK;
  }
};

TEST(CanopenOD, HandlerWrite) {
  uint32_t value = 7;
  uint16_t ro = 1;
  CountingObject handler;
  ObjectDictionary od;
  od.add_var(0x2000, OD_READ_WRITE, &value, sizeof(value));
  od.add_var(0x2001, OD_READ, &ro, sizeof(ro));
  od.set_object(0x2000, &handler);
  ASSERT_TRUE(od.finalize());

  uint32_t v = 0x12345678;
  EXPECT_EQ(od.write(0x2000, 0, &v, sizeof(v)), OdResult::OD_RESULT_OK);
  EXPECT_EQ(value, 0x12345678u);
  EXPECT_EQ(handler.writes, 1);
  // reads go to the storage (default od_read)
  uint32_t read = 0;
  EXPECT_EQ(OD_get_u32(od.find(0x2000), 0, &read, false), ODR_OK);
  EXPECT_EQ(read, 0x12345678u);

  uint16_t short_value = 1;
  uint64_t long_value = 1;
  EXPECT_EQ(od.write(0x2000, 0, &short_value, sizeof(short_value)), OdResult::OD_RESULT_DATA_SHORT);
  EXPECT_EQ(od.write(0x2000, 0, &long_value, sizeof(long_value)), OdResult::OD_RESULT_DATA_LONG);
  EXPECT_EQ(od.write(0x2001, 0, &short_value, sizeof(short_value)), OdResult::OD_RESULT_READ_ONLY);
  EXPECT_EQ(od.write(0x2002, 0, &v, sizeof(v)), OdResult::OD_RESULT_NOT_EXIST);
  EXPECT_EQ(od.write(0x2000, 1, &v, sizeof(v)), OdResult::OD_RESULT_SUB_NOT_EXIST);
  EXPECT_EQ(handler.writes, 1);
}

TEST(CanopenOD, HandlerPartialRead) {
  GeneratedObject handler;
  ObjectDictionary od;
  od.add_var(0x3100, OD_READ, nullptr, 0);
  od.set_object(0x3100, &handler);
  ASSERT_TRUE(od.finalize());

  // as the SDO server reads it: chunks of the free buffer space until OK
  OD_IO_t io;
  ASSERT_EQ(OD_getSub(od.find(0x3100), 0, &io, false), ODR_OK);
  uint8_t data[128];
  size_t total = 0;
  ODR_t ret;
  do {
    OD_size_t n = 0;
    ret = io.read(&io.stream, data + total, 32, &n);
    total += n;
  } while (ret == ODR_PARTIAL && total < sizeof(data));
  EXPECT_EQ(ret, ODR_OK);
  EXPECT_EQ(total, 100u);
  EXPECT_EQ(io.stream.dataLength, 100u);
  EXPECT_EQ(data[0], 0);
  EXPECT_EQ(data[99], 99);
  // no od_write: the default writes to the storage, which a read-only entry doesn't allow
  uint8_t b = 0;
  EXPECT_EQ(od.write(0x3100, 0, &b, 1), OdResult::OD_RESULT_READ_ONLY);
}

struct CountingReadObject : OdObject {
  int reads{0};
  OdResult od_read(OdStream stream, void *buf, size_t count, size_t *done) {
    this->reads++;
    return stream.read_original(buf, count, done);
  }
};

TEST(CanopenOD, Read) {
  uint32_t value = 0x12345678;
  uint8_t small = 7, write_only = 0;
  CountingReadObject counting;
  ObjectDictionary od;
  od.add_var(0x2000, OD_READ, &value, sizeof(value));
  od.add_var(0x2001, OD_READ, &small, sizeof(small));
  od.add_var(0x2002, OD_WRITE, &write_only, sizeof(write_only));
  od.add_var(0x2003, OD_READ, &value, sizeof(value));
  od.set_object(0x2003, &counting);
  size_t len = 99;
  uint32_t read = 0;
  EXPECT_EQ(od.read(0x2000, 0, &read, sizeof(read), &len), OdResult::OD_RESULT_DEVICE_STATE);  // before finalize()
  EXPECT_EQ(len, 0u);
  ASSERT_TRUE(od.finalize());

  EXPECT_EQ(od.read(0x2000, 0, &read, sizeof(read), &len), OdResult::OD_RESULT_OK);
  EXPECT_EQ(read, 0x12345678u);
  EXPECT_EQ(len, 4u);
  // a larger buffer: len says how much is valid
  uint8_t buf[8] = {};
  EXPECT_EQ(od.read(0x2001, 0, buf, sizeof(buf), &len), OdResult::OD_RESULT_OK);
  EXPECT_EQ(len, 1u);
  EXPECT_EQ(buf[0], 7);
  // through the handler
  EXPECT_EQ(od.read(0x2003, 0, &read, sizeof(read), &len), OdResult::OD_RESULT_OK);
  EXPECT_EQ(counting.reads, 1);

  EXPECT_EQ(od.read(0x2000, 0, buf, 2, &len), OdResult::OD_RESULT_DATA_LONG);
  EXPECT_EQ(od.read(0x2000, 0, buf, 0, &len), OdResult::OD_RESULT_DATA_LONG);
  EXPECT_EQ(od.read(0x2002, 0, buf, sizeof(buf), &len), OdResult::OD_RESULT_WRITE_ONLY);
  EXPECT_EQ(od.read(0x2FFF, 0, buf, sizeof(buf), &len), OdResult::OD_RESULT_NOT_EXIST);
  EXPECT_EQ(od.read(0x2000, 1, buf, sizeof(buf), &len), OdResult::OD_RESULT_SUB_NOT_EXIST);
  EXPECT_EQ(len, 0u);

  // a value in several handler calls
  GeneratedObject generated;
  ObjectDictionary od2;
  od2.add_var(0x3100, OD_READ, nullptr, 0);
  od2.set_object(0x3100, &generated);
  ASSERT_TRUE(od2.finalize());
  uint8_t data[128];
  EXPECT_EQ(od2.read(0x3100, 0, data, sizeof(data), &len), OdResult::OD_RESULT_OK);
  EXPECT_EQ(len, 100u);
  EXPECT_EQ(data[99], 99);
  EXPECT_EQ(od2.read(0x3100, 0, data, 50, &len), OdResult::OD_RESULT_DATA_LONG);
  EXPECT_EQ(len, 50u);
}

TEST(CanopenOD, Errors) {
  uint8_t a = 0, b = 0;
  CountingObject handler;
  ObjectDictionary od;
  EXPECT_EQ(od.write(0x2000, 0, &a, 1), OdResult::OD_RESULT_DEVICE_STATE);  // before finalize()
  od.add_var(0x2000, OD_READ_WRITE, &a, 1);
  od.add_var(0x2000, OD_READ_WRITE, &b, 1);
  od.set_object(0x2100, &handler);  // no such entry: logged, ignored
  EXPECT_FALSE(od.finalize());
}

// many entries of every kind, handlers set in between: the containers reallocate while the OD is built, and every
// entry must still reach its own storage and handler
TEST(CanopenOD, ManyEntries) {
  static constexpr int N = 100;
  uint32_t vars[N], elems[N][2], subs[N];
  uint8_t counts[N], highest = 1;
  CountingObject handlers[N];
  ObjectDictionary od;
  for (int i = 0; i < N; i++) {
    vars[i] = i;
    od.add_var(0x2000 + i, OD_READ_WRITE, &vars[i], 4);
    od.add_array(0x3000 + i, OD_READ_WRITE, &counts[i], elems[i], 4, 2);
    od.add_record(0x4000 + i, {{0, OD_READ, &highest, 1}, {1, OD_READ_WRITE, &subs[i], 4}});
    od.set_object(0x2000 + i, &handlers[i]);
  }
  ASSERT_TRUE(od.finalize());

  for (int i = 0; i < N; i++) {
    uint32_t value = 0;
    EXPECT_EQ(OD_get_u32(od.find(0x2000 + i), 0, &value, true), ODR_OK);
    EXPECT_EQ(value, uint32_t(i));
    uint32_t v = 1000 + i;
    EXPECT_EQ(od.write(0x2000 + i, 0, &v, 4), OdResult::OD_RESULT_OK);
    EXPECT_EQ(handlers[i].writes, 1);
    EXPECT_EQ(vars[i], v);
    EXPECT_EQ(od.write(0x3000 + i, 2, &v, 4), OdResult::OD_RESULT_OK);
    EXPECT_EQ(elems[i][1], v);
    EXPECT_EQ(od.write(0x4000 + i, 1, &v, 4), OdResult::OD_RESULT_OK);
    EXPECT_EQ(subs[i], v);
  }
}

TEST(CanopenOD, AfterFinalize) {
  uint32_t a = 1, b = 2;
  CountingObject handler;
  ObjectDictionary od;
  od.add_var(0x2000, OD_READ_WRITE, &a, 4);
  ASSERT_TRUE(od.finalize());
  // ignored: the entry list is fixed, and growing a container would move the objects CANopenNode points to
  od.add_var(0x2001, OD_READ_WRITE, &b, 4);
  od.set_object(0x2000, &handler);
  EXPECT_EQ(od.find(0x2001), nullptr);
  uint32_t v = 5;
  EXPECT_EQ(od.write(0x2000, 0, &v, 4), OdResult::OD_RESULT_OK);
  EXPECT_EQ(handler.writes, 0);
  EXPECT_EQ(a, 5u);
}

}  // namespace esphome::canopen::testing
