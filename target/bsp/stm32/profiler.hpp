#pragma once

#include <cstdint>

#include "interfaces/profiler.hpp"

namespace pickup::bsp::stm32 {

enum class ProfileId : std::uint32_t {
  main_loop,
  uart,
  dac_dma,
  adc_dma,
  adc,
  timer_dac,
  systick,
};

class PerformanceProfiler final : public Profiler {
 public:
  std::span<const ProfileContext> contexts() const override;
  std::span<ProfileStatistics> statistics(std::span<ProfileStatistics> output) override;
  void reset() override;
};

namespace profile {
void initialize();
// These are the only calls required at instrumented execution boundaries.
void start(ProfileId id);
void stop(ProfileId id);
}  // namespace profile

}  // namespace pickup::bsp::stm32
