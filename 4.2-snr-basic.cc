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

#// SECTION BEGIN: Basic
int mul_2(int x) { return x * 2; }
sender auto add_3(int x) { return just(x + 3); }

sender auto snd = just(1) | then(mul_2) | let_value(add_3);
#// SECTION END: Basic

TEST_CASE("SnR basic") {
  auto [i] = sync_wait(std::move(snd)).value();
  REQUIRE(i == 5);
}

}  // namespace
