#include "nco.hpp"

#include <array>
#include <cmath>

namespace {
constexpr std::size_t table_bits = 10;
constexpr std::size_t table_length = 1U << table_bits;

// Generate the Q15 cosine table at compile time; no trig or floating point in IRQs.
constexpr auto make_table() {
  std::array<std::int16_t, table_length + 1> result{};
  constexpr double step_cos = 0.9999811752826011;
  constexpr double step_sin = 0.006135884649154475;
  double cosine = 1.0;
  double sine = 0.0;

  for (auto& value : result) {
    const double scaled = cosine * 32767.0;
    value = static_cast<std::int16_t>(scaled + (scaled >= 0.0 ? 0.5 : -0.5));
    const double next_cosine = cosine * step_cos - sine * step_sin;
    sine = sine * step_cos + cosine * step_sin;
    cosine = next_cosine;
  }

  return result;
}

constexpr auto table = make_table();
}  // namespace

namespace pickup::bsp::stm32 {
void Nco::reset(
    float frequency_hz,
    float sample_rate_hz,
    float amplitude_v
) {
  phase_ = 0;

  configure(
      frequency_hz,
      sample_rate_hz,
      amplitude_v
  );
}

void Nco::configure(
    float frequency_hz,
    float sample_rate_hz,
    float amplitude_v
) {
  increment_ = static_cast<std::uint32_t>(
      std::llround(static_cast<double>(frequency_hz) / sample_rate_hz * 4294967296.0)
  );
  amplitude_ = static_cast<std::int32_t>(std::lround(amplitude_v * 4095.0F / 3.3F));
}

std::uint16_t Nco::next() {
  const auto index = phase_ >> (32 - table_bits);
  const auto fraction = static_cast<std::int32_t>((phase_ >> (16 - table_bits)) & 65535U);
  const std::int32_t first = table[index];
  const std::int32_t difference = table[index + 1] - first;
  const std::int32_t cosine = first + difference * fraction / 65536;
  const std::int32_t scaled = cosine * amplitude_;
  const auto sample = 2048 + (scaled + (scaled >= 0 ? 16383 : -16383)) / 32767;
  phase_ += increment_;

  return static_cast<std::uint16_t>(sample);
}

void Nco::fill(std::span<std::uint16_t> output) {
  for (auto& sample : output) {
    sample = next();
  }
}
}  // namespace pickup::bsp::stm32
