#include "profiler.hpp"

#include <algorithm>
#include <array>

#include "profiler_timing.hpp"
#include "stm32g4xx_hal.h"

namespace {
using pickup::bsp::ProfileContext;
using pickup::bsp::ProfileContextType;
using pickup::bsp::ProfileStatistics;
using pickup::bsp::stm32::ProfileId;

constexpr std::uint32_t cpu_hz = 170000000;
constexpr std::size_t context_count = 7;
constexpr std::size_t maximum_nesting = 8;
constexpr std::array contexts{
    ProfileContext{
        0,
        255,
        ProfileContextType::task,
        "main loop"
    },
    ProfileContext{
        1,
        0,
        ProfileContextType::interrupt,
        "USART2"
    },
    ProfileContext{
        2,
        1,
        ProfileContextType::interrupt,
        "DAC DMA"
    },
    ProfileContext{
        3,
        1,
        ProfileContextType::interrupt,
        "ADC DMA"
    },
    ProfileContext{
        4,
        1,
        ProfileContextType::interrupt,
        "ADC"
    },
    ProfileContext{
        5,
        1,
        ProfileContextType::interrupt,
        "TIM6/DAC"
    },
    ProfileContext{
        6,
        15,
        ProfileContextType::interrupt,
        "SysTick"
    },
};

pickup::ExclusiveProfileTiming<context_count, maximum_nesting> timing;
std::uint32_t reset_at_ms{};
bool initialized{};

class CriticalSection {
 public:
  [[gnu::always_inline]] explicit inline CriticalSection() : previous_(__get_PRIMASK()) {
    __disable_irq();
    __DMB();
  }

  [[gnu::always_inline]] inline ~CriticalSection() {
    __DMB();
    __set_PRIMASK(previous_);
  }

 private:
  std::uint32_t previous_;
};

std::size_t index(ProfileId id) {
  return static_cast<std::size_t>(id);
}

void clear_counters(std::uint32_t now_cycles) {
  timing.reset(now_cycles);

  reset_at_ms = HAL_GetTick();
}
}  // namespace

namespace pickup::bsp::stm32::profile {
void initialize() {
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  initialized = true;

  clear_counters(DWT->CYCCNT);
}

void start(ProfileId id) {
  if (!initialized) {
    return;
  }

  CriticalSection critical;

  timing.start(index(id), DWT->CYCCNT);
}

void stop(ProfileId id) {
  if (!initialized) {
    return;
  }

  CriticalSection critical;

  timing.stop(index(id), DWT->CYCCNT);
}
}  // namespace pickup::bsp::stm32::profile

namespace pickup::bsp::stm32 {
std::span<const ProfileContext> PerformanceProfiler::contexts() const {
  return ::contexts;
}

std::span<ProfileStatistics> PerformanceProfiler::statistics(std::span<ProfileStatistics> output) {
  CriticalSection critical;
  const auto& counters = timing.counters();
  const auto count = std::min(output.size(), counters.size());
  const auto elapsed_ms = static_cast<std::uint32_t>(HAL_GetTick() - reset_at_ms);
  const double cycles_per_us = cpu_hz / 1000000.0;
  const double window_us = static_cast<double>(elapsed_ms) * 1000.0;

  for (std::size_t result_index = 0; result_index < count; ++result_index) {
    const auto& source = counters[result_index];
    auto& destination = output[result_index];
    destination.id = ::contexts[result_index].id;
    destination.executions = source.executions;
    destination.total_us = source.total_ticks / cycles_per_us;
    destination.average_us =
        source.executions == 0 ? 0.0 : destination.total_us / source.executions;
    destination.minimum_us = source.executions == 0 ? 0.0 : source.minimum_ticks / cycles_per_us;
    destination.maximum_us = source.maximum_ticks / cycles_per_us;
    destination.cpu_percent = window_us == 0.0 ? 0.0 : 100.0 * destination.total_us / window_us;
  }

  return output.first(count);
}

void PerformanceProfiler::reset() {
  CriticalSection critical;

  clear_counters(DWT->CYCCNT);
}
}  // namespace pickup::bsp::stm32
