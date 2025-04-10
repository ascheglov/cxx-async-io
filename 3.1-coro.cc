#include <catch2/catch_test_macros.hpp>
#include <coroutine>
#include <expected>
#include <functional>
#include <future>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

using namespace std;

namespace {

template <typename R> struct Future {
  struct promise_type;

  explicit Future(promise_type* promise) : promise_{promise} {}

  [[nodiscard]] bool await_ready() const;

  void await_suspend(coroutine_handle<> resume) const;

  R await_resume() noexcept;

  void Then(function<void(R)> cb);

  struct Deleter {
    void operator()(promise_type* promise);
  };

  std::unique_ptr<promise_type, Deleter> promise_;
};

template <typename R> struct Future<R>::promise_type {
  // If we were called from a coroutine.
  std::coroutine_handle<> caller_coro{noop_coroutine()};

  // If .Then() was used
  std::function<void(R)> then_cb;

  optional<R> result;

  // Coroutine support.
  Future get_return_object() { return Future{this}; }

  // Coroutine support: non-lazy.
  std::suspend_never initial_suspend() noexcept { return {}; }

  // Coroutine support: mandatory.
  static void unhandled_exception() { throw; }

  // Coroutine support: co_return expr;
  void return_value(R r) { result = std::move(r); }

  promise_type& final_suspend() noexcept { return *this; }

  // for final_suspend
  static constexpr bool await_ready() noexcept { return false; }

  // for final_suspend
  coroutine_handle<> await_suspend(coroutine_handle<> resume) const noexcept {
    if (then_cb) { then_cb(result.value()); }
    return caller_coro;
  }

  // for final_suspend
  void await_resume() noexcept {}
};

template <typename R> inline bool Future<R>::await_ready() const {
  return promise_->result.has_value();
}

template <typename R>
inline void Future<R>::await_suspend(std::coroutine_handle<> resume) const {
  promise_->caller_coro = resume;
}

template <typename R> inline R Future<R>::await_resume() noexcept {
  if constexpr (!std::is_void_v<R>) { return promise_->result.value(); }
}

template <typename R> inline void Future<R>::Then(std::function<void(R)> cb) {
  if (promise_->result.has_value()) {
    cb(promise_->result.value());
  } else {
    promise_->then_cb = std::move(cb);
  }
}

template <typename R>
inline void Future<R>::Deleter::operator()(Future::promise_type* promise) {
  std::coroutine_handle<promise_type>::from_promise(*promise).destroy();
}

//////////////////////////////////////

void Process(vector<byte> data);

#// SECTION BEGIN: API
Future<expected<size_t, error_code>> ReadSome(int fd, span<byte> buf);
#// SECTION END: API

#// SECTION BEGIN: ReadAll
Future<expected<void, error_code>> ReadAll(int fd, span<byte> buf) {
  while (!buf.empty()) {
    auto res = co_await ReadSome(fd, buf);
    if (!res) { co_return unexpected(res.error()); }
    buf = buf.subspan(*res);
  }
  co_return {};
}
#// SECTION END: ReadAll

#// SECTION BEGIN: HandleConnection
Future<expected<void, error_code>> HandleConnection(int fd) {
  for (;;) {
    uint8_t data_size;
    auto res = co_await ReadAll(fd, as_writable_bytes(span{&data_size, 1}));
    if (!res || data_size == 0) { co_return res; }
    std::vector<byte> data(data_size);
    res = co_await ReadAll(fd, data);
    if (!res) { co_return res; }
    Process(std::move(data));
  }
}
#// SECTION END: HandleConnection

///////////////////////////////////////////////////////////////////////////////

char test_bytes[] = {2, 'a', 'b',  //
                     1, 'c',       //
                     0};

span<char> input;

Future<expected<size_t, error_code>> ReadSome(int fd, span<byte> buf) {
  if (input.empty()) { co_return unexpected(make_error_code(errc::io_error)); }
  buf[0] = (byte)input.front();
  input = input.subspan(1);
  co_return 1;
}

void Process(vector<byte> data) {
  INFO("process" << string_view((char*)data.data(), data.size()));
}

error_code Run() {
  promise<expected<void, error_code>> p;
  auto fut = HandleConnection(1);
  fut.Then([&](expected<void, error_code> res) { p.set_value(res); });
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
