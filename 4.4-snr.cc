#include <catch2/catch_test_macros.hpp>
#include <exec/any_sender_of.hpp>
#include <exec/repeat_effect_until.hpp>
#include <expected>
#include <functional>
#include <future>
#include <iostream>
#include <span>
#include <stdexec/execution.hpp>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "exec/static_thread_pool.hpp"

using namespace std;
using namespace stdexec;
using namespace exec;

namespace {

void Process(vector<byte> data);

void ReadSomeCPS(int fd, span<byte> buf,
                 function<void(expected<size_t, error_code>)> callback);

#// SECTION BEGIN: API
struct ReadSome {
  template <class Receiver> struct OpState {
    int fd_;
    span<byte> buf_;
    Receiver r_;
    void start() noexcept {
      ReadSomeCPS(fd_, buf_, [this](auto res) { OnRead(res); });
    }
    void OnRead(expected<size_t, error_code> res) {
      auto&& r = std::move(r_);
      res ? r.set_value(*res) : r.set_error(res.error());
    }
  };
  int fd_;
  span<byte> buf_;
  using sender_concept = sender_t;
  using completion_signatures =
      completion_signatures<set_value_t(size_t), set_error_t(error_code)>;
  auto connect(receiver auto r) { return OpState{fd_, buf_, std::move(r)}; }
};
#// SECTION END: API

#// SECTION BEGIN: ReadAll
sender auto ReadAll(int fd, span<byte> buf) {
  return just(buf) | let_value([fd](span<byte>& buf) {
           return just() | let_value([&] {
                    return ReadSome{fd, buf} | then([&buf](size_t n) {
                             buf = buf.subspan(n);
                             return buf.empty();
                           });
                  }) |
                  repeat_effect_until();
         });
}
#// SECTION END: ReadAll

template <class... Ts> using any_sender_of = typename any_receiver_ref<
    completion_signatures<Ts...>>::template any_sender<>;

using any_sender = any_sender_of<set_value_t(bool), set_error_t(error_code)>;

#// SECTION BEGIN: HandleConnection
sender auto HandleOne(int fd, uint8_t& data_size, vector<byte>& data) {
  auto read_size = ReadAll(fd, as_writable_bytes(span{&data_size, 1}));
  auto read_data = [&] {
    data.resize(data_size);
    return ReadAll(fd, data);
  };
  return read_size | let_value(read_data) | let_value([&] {
           Process(std::move(data));
           return just(true);
         });
}

sender auto HandleConnection(int fd) {
  return just(fd, uint8_t{}, vector<byte>{}) | let_value(HandleOne) |
         repeat_effect_until();
}
#// SECTION END: HandleConnection

char test_bytes[] = {2, 'a', 'b',  //
                     1, 'c',       //
                     0};

span<char> input;

void ReadSomeCPS(int fd, span<byte> buf,
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

TEST_CASE(__FILE__) {
  SECTION("ReadSome") {
    input = test_bytes;
    std::array<byte, 2> buf;
    auto [n] = sync_wait(ReadSome{1, buf}).value();
    REQUIRE(n == 1);
    REQUIRE(buf[0] == byte(2));
    auto [m] = sync_wait(ReadSome{1, buf}).value();
    REQUIRE(m == 1);
    REQUIRE(buf[0] == byte('a'));
  }

  SECTION("ReadAll") {
    input = test_bytes;
    std::array<byte, 2> buf{byte('0'), byte('1')};
    sync_wait(ReadAll(1, buf));
    REQUIRE(buf[0] == byte(2));
    REQUIRE(buf[1] == byte('a'));
  }

  SECTION("HandleOne - OK") {
    input = span(test_bytes).first(3);
    uint8_t data_size;
    vector<byte> data;
    auto [done] = *sync_wait(HandleOne(1, data_size, data));
    REQUIRE(done);
  }

  SECTION("HandleOne - Stop") {
    input = span(test_bytes).first(3);
    uint8_t data_size;
    vector<byte> data;
    auto [done] = *sync_wait(HandleOne(1, data_size, data));
    REQUIRE(done);
  }

  SECTION("HandleOne - Err") {
    input = span(test_bytes).first(2);
    uint8_t data_size;
    vector<byte> data;
    try {
      sync_wait(HandleOne(1, data_size, data));
      REQUIRE(false);
    } catch (...) {}
  }

  SECTION("OK") {
    input = test_bytes;
    sync_wait(HandleConnection(1));
  }
}

}  // namespace
