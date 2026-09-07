#include <cassert>
#include <cstdint>
#include <limits>

#include "profiler_timing.hpp"

namespace {
void test_preemption_is_exclusive() {
  pickup::ExclusiveProfileTiming<3, 4> timing;

  assert(timing.start(0, 0));
  assert(timing.start(1, 10));
  assert(timing.start(2, 15));
  assert(timing.stop(2, 25));
  assert(timing.stop(1, 35));
  assert(timing.stop(0, 45));

  const auto& counters = timing.counters();

  assert(counters[0].total_ticks == 20);
  assert(counters[1].total_ticks == 15);
  assert(counters[2].total_ticks == 10);
  assert(counters[0].total_ticks + counters[1].total_ticks + counters[2].total_ticks == 45);
}

void test_reset_and_wrapping_timer() {
  pickup::ExclusiveProfileTiming<2, 2> timing;
  constexpr auto near_wrap = std::numeric_limits<std::uint32_t>::max() - 4;

  assert(timing.start(0, near_wrap));
  assert(timing.stop(0, 5));
  assert(timing.counters()[0].total_ticks == 10);

  assert(timing.start(0, 20));
  timing.reset(25);
  assert(timing.stop(0, 32));
  assert(timing.counters()[0].executions == 1);
  assert(timing.counters()[0].total_ticks == 7);
  assert(timing.counters()[0].minimum_ticks == 7);
  assert(timing.counters()[0].maximum_ticks == 7);
}

void test_invalid_notifications_do_not_change_results() {
  pickup::ExclusiveProfileTiming<2, 2> timing;

  assert(!timing.stop(0, 1));
  assert(!timing.start(2, 2));
  assert(timing.start(0, 3));
  assert(!timing.stop(1, 4));
  assert(timing.start(1, 5));
  assert(!timing.start(0, 6));
  assert(timing.stop(1, 7));
  assert(timing.stop(0, 9));
  assert(timing.counters()[0].total_ticks == 4);
  assert(timing.counters()[1].total_ticks == 2);
}
}  // namespace

int main() {
  test_preemption_is_exclusive();
  test_reset_and_wrapping_timer();
  test_invalid_notifications_do_not_change_results();
}
