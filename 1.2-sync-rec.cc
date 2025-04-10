#include <catch2/catch_test_macros.hpp>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

using namespace std;

namespace {

expected<size_t, error_code> ReadSome(int fd, span<byte> buf);

void Process(vector<byte> data);

#// SECTION BEGIN: ReadAll
expected<void, error_code> ReadAll(int fd, span<byte> buf) {
  if (buf.empty()) { return {}; }
  auto res = ReadSome(fd, buf);
  if (!res) { return unexpected(res.error()); }
  return ReadAll(fd, buf.subspan(*res));
}
#// SECTION END: ReadAll

#// SECTION BEGIN: HandleConnection
expected<void, error_code> HandleConnection(int fd) {
  uint8_t data_size;
  auto res = ReadAll(fd, as_writable_bytes(span{&data_size, 1}));
  if (!res || data_size == 0) { return res; }
  std::vector<byte> data(data_size);
  res = ReadAll(fd, data);
  if (!res) { return res; }
  Process(std::move(data));
  return HandleConnection(fd);
}
#// SECTION END: HandleConnection

char test_bytes[] = {2, 'a', 'b',  //
                     1, 'c',       //
                     0};

span<char> input;

expected<size_t, error_code> ReadSome(int fd, span<byte> buf) {
  if (input.empty()) { return unexpected(make_error_code(errc::io_error)); }
  buf[0] = (byte)input.front();
  input = input.subspan(1);
  return 1;
}

void Process(vector<byte> data) {
  INFO("process" << string_view((char*)data.data(), data.size()));
}

error_code Run() { return HandleConnection(1).error_or({}); }

TEST_CASE(__FILE__) {
  SECTION("OK") {
    input = test_bytes;
    REQUIRE(Run() == error_code{});
  }
  SECTION("fail") {
    input = span(test_bytes).first(2);
    REQUIRE(Run() == make_error_code(errc::io_error));
  }
}

}  // namespace
