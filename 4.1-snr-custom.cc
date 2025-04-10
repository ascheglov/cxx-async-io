#include <catch2/catch_test_macros.hpp>
#include <exec/any_sender_of.hpp>
#include <exec/repeat_effect_until.hpp>
#include <exec/static_thread_pool.hpp>
#include <expected>
#include <functional>
#include <future>
#include <span>
#include <stdexec/execution.hpp>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

using namespace std;
using namespace stdexec;
using namespace exec;

namespace {
struct NonMoveable {
  NonMoveable() = default;
  NonMoveable(NonMoveable&&) = delete;
};

#// SECTION BEGIN: Def
template <class Receiver> struct OperationState : NonMoveable {
  Receiver r_;
  void start() noexcept { std::move(r_).set_value(42); }
};

struct Sender {
  using sender_concept = sender_t;
  using completion_signatures = completion_signatures<set_value_t(int)>;
  auto connect(receiver auto r) { return OperationState{.r_ = std::move(r)}; };
};
#// SECTION END: Def

TEST_CASE("SnR custom") {
#// SECTION BEGIN: Use
  sender auto s = Sender{};
  auto [result] = sync_wait(s).value();
  REQUIRE(result == 42);
#// SECTION END: Use
}

}  // namespace
