#include <catch2/catch_test_macros.hpp>
#include <expected>
#include <functional>
#include <future>
#include <iostream>
#include <source_location>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

using namespace std;

namespace {

//////////////////////////////////////

void Process(vector<byte> data);

#// SECTION BEGIN: API
optional<expected<size_t, error_code>> ReadSome(
    int fd, span<byte> buf,
    function<void(expected<size_t, error_code>)> callback);
#// SECTION END: API

struct Reader {
  int fd;
  function<void(expected<void, error_code>)> callback;
#// SECTION BEGIN: ReadAll
  void Start(span<byte> buffer) {
    buf = buffer;
    if (buf.empty()) { return callback({}); }
    auto res = ReadSome(fd, buf, [this](auto res) { OnRead(res); });
    if (res) { OnRead(*res); }
  }
  void OnRead(expected<size_t, error_code> res) {
    if (!res) { return callback(unexpected(res.error())); }
    Start(buf.subspan(*res));
  }
#// SECTION END: ReadAll
  span<byte> buf;
};

#// SECTION BEGIN: HandleConnection
struct ConnectionHandler {
  int fd;
  function<void(expected<void, error_code>)> callback;
  void Start() { size_reader.Start(as_writable_bytes(span{&data_size, 1})); }
  void OnReadSize(expected<void, error_code> res) {
    if (!res || data_size == 0) { return callback(res); }
    data.resize(data_size);
    data_reader.Start(data);
  }
  void OnReadData(expected<void, error_code> res) {
    if (!res) { return callback(res); }
    Process(std::move(data));
    Start();
  }
  uint8_t data_size;
  std::vector<byte> data;
  Reader size_reader{fd, [this](auto res) { OnReadSize(res); }};
  Reader data_reader{fd, [this](auto res) { OnReadData(res); }};
};
#// SECTION END: HandleConnection

char test_bytes[] = {2, 'a', 'b',  //
                     1, 'c',       //
                     0};

span<char> input;

optional<expected<size_t, error_code>> ReadSome(
    int fd, span<byte> buf,
    function<void(expected<size_t, error_code>)> callback) {
  if (input.empty()) { return unexpected(make_error_code(errc::io_error)); }
  buf[0] = (byte)input.front();
  input = input.subspan(1);
  return 1;
}

void Process(vector<byte> data) {
  INFO("process" << string_view((char*)data.data(), data.size()));
}

error_code Run() {
  promise<expected<void, error_code>> p;
  ConnectionHandler h{
      .fd = 1,
      .callback = [&](expected<void, error_code> res) { p.set_value(res); },
  };
  h.Start();
  return p.get_future().get().error_or({});
}

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
