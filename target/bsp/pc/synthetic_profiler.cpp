#include "synthetic_profiler.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace {
using pickup::bsp::ProfileContext;
using pickup::bsp::ProfileContextType;

constexpr std::array contexts{
    ProfileContext{
        0,
        255,
        ProfileContextType::task,
        "main loop"
    },
    ProfileContext{
        1,
        1,
        ProfileContextType::interrupt,
        "simulated acquisition"
    },
    ProfileContext{
        2,
        2,
        ProfileContextType::interrupt,
        "virtual transport"
    },
};
}  // namespace

namespace pickup::bsp::pc {
SyntheticProfiler::SyntheticProfiler() : reset_at_(std::chrono::steady_clock::now()) {}

std::span<const ProfileContext> SyntheticProfiler::contexts() const {
  return ::contexts;
}

std::span<ProfileStatistics> SyntheticProfiler::statistics(std::span<ProfileStatistics> output) {
  const double elapsed =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - reset_at_).count();
  const auto count = std::min(output.size(), ::contexts.size());
  const std::array loads{
      5.0 + 1.5 * std::sin(elapsed * 0.7),
      1.8 + 0.6 * std::sin(elapsed * 1.1 + 0.4),
      0.4 + 0.2 * std::sin(elapsed * 0.5 + 1.2),
  };
  const std::array averages{
      52.0,
      18.0,
      7.0,
  };

  for (std::size_t index = 0; index < count; ++index) {
    auto& result = output[index];
    result.id = ::contexts[index].id;
    result.average_us = averages[index];
    result.minimum_us = averages[index] * 0.72;
    result.maximum_us = averages[index] * 1.55;
    result.cpu_percent = loads[index];
    result.total_us = elapsed * 10000.0 * loads[index];
    result.executions = static_cast<std::uint64_t>(result.total_us / result.average_us);
  }

  return output.first(count);
}

void SyntheticProfiler::reset() {
  reset_at_ = std::chrono::steady_clock::now();
}
}  // namespace pickup::bsp::pc
