#include <catch2/catch_test_macros.hpp>
#include <expected>
#include <functional>
#include <future>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

using namespace std;

namespace {

void Process(vector<byte> data);

#// SECTION BEGIN: API
void ReadSome(int fd, span<byte> buf,
              function<void(expected<size_t, error_code>)> callback);
#// SECTION END: API

#// SECTION BEGIN: ReadAll
void ReadAll(int fd, span<byte> buf,
             function<void(expected<void, error_code>)> callback) {
  if (buf.empty()) { return callback({}); }
  ReadSome(fd, buf, [=](expected<size_t, error_code> res) {
    if (!res) { return callback(unexpected(res.error())); }
    ReadAll(fd, buf.subspan(*res), callback);
  });
}
#// SECTION END: ReadAll

#// SECTION BEGIN: HandleConnection
void HandleConnection(int fd,
                      function<void(expected<void, error_code>)> callback) {
  auto data_size = make_shared<uint8_t>();
  ReadAll(fd, as_writable_bytes(span{data_size.get(), 1}),
          [=](expected<void, error_code> res) {
            if (!res || *data_size == 0) { return callback(res); }
            auto data = make_shared<std::vector<byte>>(*data_size);
            ReadAll(fd, *data, [=](expected<void, error_code> res) {
              if (!res) { return callback(res); }
              Process(std::move(*data));
              HandleConnection(fd, callback);
            });
          });
}
#// SECTION END: HandleConnection

char test_bytes[] = {2, 'a', 'b',  //
                     1, 'c',       //
                     0};

span<char> input;

void ReadSome(int fd, span<byte> buf,
              function<void(expected<size_t, error_code>)> callback) {
  if (input.empty()) {
    return callback(unexpected(make_error_code(errc::io_error)));
  }
  buf[0] = (byte)input.front();
  input = input.subspan(1);
  return callback(1);
}

void Process(vector<byte> data) {
  INFO("process" << string_view((char*)data.data(), data.size()));
}

error_code Run() {
  promise<expected<void, error_code>> p;
  HandleConnection(1,
                   [&](expected<void, error_code> res) { p.set_value(res); });
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
