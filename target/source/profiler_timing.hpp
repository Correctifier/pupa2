#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace pickup {

struct ProfileCounters {
  std::uint64_t executions{};
  std::uint64_t total_ticks{};
  std::uint32_t minimum_ticks{std::numeric_limits<std::uint32_t>::max()};
  std::uint32_t maximum_ticks{};
};

// Accumulates exclusive execution time from a monotonically wrapping 32-bit timer.
template <std::size_t ContextCount, std::size_t MaximumNesting>
class ExclusiveProfileTiming {
 public:
  bool start(std::size_t id, std::uint32_t now) {
    if (id >= counters_.size() || depth_ == active_.size()) {
      return false;
    }

    if (depth_ != 0) {
      auto& interrupted = active_[depth_ - 1];
      interrupted.accumulated_ticks += now - interrupted.started_at;
    }

    active_[depth_++] = {
        id,
        now,
        0
    };

    return true;
  }

  bool stop(std::size_t id, std::uint32_t now) {
    if (depth_ == 0 || active_[depth_ - 1].id != id) {
      return false;
    }

    const auto current = active_[--depth_];
    const auto elapsed = current.accumulated_ticks + now - current.started_at;
    auto& result = counters_[id];
    ++result.executions;
    result.total_ticks += elapsed;
    result.minimum_ticks = std::min(result.minimum_ticks, elapsed);
    result.maximum_ticks = std::max(result.maximum_ticks, elapsed);

    if (depth_ != 0) {
      active_[depth_ - 1].started_at = now;
    }

    return true;
  }

  void reset(std::uint32_t now) {
    counters_ = {};

    for (std::size_t index = 0; index < depth_; ++index) {
      active_[index].accumulated_ticks = 0;
    }

    if (depth_ != 0) {
      active_[depth_ - 1].started_at = now;
    }
  }

  const std::array<ProfileCounters, ContextCount>& counters() const {
    return counters_;
  }

 private:
  struct ActiveContext {
    std::size_t id{};
    std::uint32_t started_at{};
    std::uint32_t accumulated_ticks{};
  };

  std::array<ProfileCounters, ContextCount> counters_{};
  std::array<ActiveContext, MaximumNesting> active_{};
  std::size_t depth_{};
};

}  // namespace pickup
