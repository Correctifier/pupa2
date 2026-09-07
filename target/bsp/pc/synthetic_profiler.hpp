#pragma once

#include <chrono>

#include "interfaces/profiler.hpp"

namespace pickup::bsp::pc {

class SyntheticProfiler final : public Profiler {
 public:
  SyntheticProfiler();

  std::span<const ProfileContext> contexts() const override;
  std::span<ProfileStatistics> statistics(std::span<ProfileStatistics> output) override;
  void reset() override;

 private:
  std::chrono::steady_clock::time_point reset_at_;
};

}  // namespace pickup::bsp::pc
