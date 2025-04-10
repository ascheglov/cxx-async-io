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

template <class... Ts> using any_sender_of = typename any_receiver_ref<
    completion_signatures<Ts...>>::template any_sender<>;

using any_sender = any_sender_of<set_value_t(int), set_error_t(int)>;

#// SECTION BEGIN: Any
any_sender ValueOrError(bool b) {
  if (b) { return just(1); }
  return just_error(2);
}
#// SECTION END: Any

TEST_CASE("SnR any sender") {
  auto [val] = *sync_wait(ValueOrError(true));
  REQUIRE(val == 1);

  auto snd = ValueOrError(false)  //
             | let_error([](int x) { return just(x); });
  auto [err] = *sync_wait(std::move(snd));
  REQUIRE(err == 2);
}

}  // namespace
