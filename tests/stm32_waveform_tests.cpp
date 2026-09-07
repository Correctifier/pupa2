#include <array>
#include <cassert>
#include <cmath>
#include <complex>
#include <cstdint>
#include <span>

#include "adc_decimator.hpp"
#include "nco.hpp"
#include "signal_processing.hpp"

namespace {
constexpr float sample_rate = 200000.0F;
constexpr double tau = 6.28318530717958647692;

using pickup::bsp::stm32::AdcDecimator;
using pickup::bsp::stm32::Nco;

void test_nco_accuracy_and_wrap() {
  for (const float frequency : {
      1.0F,
      997.3F,
      20000.0F
  }) {
    for (const float amplitude : {
        0.001F,
        0.25F,
        1.5F
    }) {
      Nco nco;

      nco.reset(
          frequency,
          sample_rate,
          amplitude
      );

      const auto increment =
          std::llround(static_cast<double>(frequency) / sample_rate * 4294967296.0);
      const double actual_frequency = static_cast<double>(increment) * sample_rate / 4294967296.0;

      assert(std::abs(actual_frequency - frequency) <= sample_rate / 8589934592.0);

      // Several wraps even at 1 Hz; includes both sides of every DMA boundary.
      for (std::uint32_t index = 0; index < 1300000; ++index) {
        const auto sample = nco.next();
        const double expected = 2048.0 + amplitude * 4095.0 / 3.3 *
                                             std::cos(tau * actual_frequency * index / sample_rate);

        assert(sample <= 4095);
        assert(std::abs(sample - expected) < 1.6);
      }
    }
  }
}

void test_nco_refill_and_retune() {
  Nco continuous, buffered;

  continuous.reset(
      1234.5F,
      sample_rate,
      1.5F
  );
  buffered.reset(
      1234.5F,
      sample_rate,
      1.5F
  );

  assert(continuous.next() == buffered.next());

  std::array<std::uint16_t, 512> dma{};

  buffered.fill(dma);

  for (int half = 0; half < 30; ++half) {
    auto consumed = std::span(dma).subspan((half % 2) * 256, 256);

    for (const auto sample : consumed) {
      assert(sample == continuous.next());
    }

    buffered.fill(consumed);
  }

  buffered.reset(
      20.0F,
      sample_rate,
      0.25F
  );
  continuous.reset(
      20.0F,
      sample_rate,
      0.25F
  );
  buffered.fill(dma);

  for (const auto sample : dma) {
    assert(sample == continuous.next());
  }
}

void test_decimator_boundaries_and_reset() {
  AdcDecimator decimator;
  std::array<std::uint16_t, 4> output{};
  const std::array<std::uint32_t, 3> input{
      100U | (200U << 16),
      200U | (400U << 16),
      300U | (600U << 16)
  };

  decimator.configure(1000.0F, 96000.0F);
  decimator.begin(output);

  assert(decimator.sample_rate_hz() == 32000.0F);
  assert(!decimator.process(std::span(input).first(2)));
  assert(!decimator.process(std::span(input).last(1)));
  assert(output[0] == 200 && output[1] == 400);
  assert(decimator.process(input));
  assert(output[2] == 200 && output[3] == 400);
  assert(decimator.process(input));

  decimator.begin(output);

  assert(!decimator.process(std::span(input).first(1)));

  decimator.configure(20000.0F, sample_rate);
  decimator.begin(output);

  assert(decimator.process(input));
  assert(output[0] == 100 && output[1] == 200);
  assert(output[2] == 200 && output[3] == 400);
}

void test_low_frequency_accumulation() {
  AdcDecimator decimator;
  std::array<std::uint16_t, 2> output{};
  std::array<std::uint32_t, 256> input{};

  input.fill(4095U | (4095U << 16));
  decimator.configure(1.0F, sample_rate);
  decimator.begin(output);

  constexpr std::size_t divisor = static_cast<std::size_t>(sample_rate / 32.0F);
  const std::size_t incomplete_blocks = (divisor - 1) / input.size();

  for (std::size_t block = 0; block < incomplete_blocks; ++block) {
    assert(!decimator.process(input));
  }

  assert(decimator.process(input));
  assert(output[0] == 4095 && output[1] == 4095);
}

void test_decimated_impedance() {
  for (const float frequency : {
      1.0F,
      20.0F,
      1000.0F,
      9750.0F,
      9800.0F,
      15000.0F,
      20000.0F
  }) {
    AdcDecimator decimator;
    std::array<std::uint16_t, pickup::FourthOrderMovingAverage::settling_frames * 2> output{};
    std::array<std::uint32_t, 256> input{};

    decimator.configure(frequency, sample_rate);
    decimator.begin(output);

    std::size_t sample_index = 0;
    bool complete = false;

    while (!complete) {
      for (auto& pair : input) {
        const double phase = tau * frequency * sample_index++ / sample_rate + 0.7;
        const auto voltage = static_cast<std::uint32_t>(std::lround(2048 + 700 * std::cos(phase)));
        const auto sense =
            static_cast<std::uint32_t>(std::lround(2048 + 350 * std::cos(phase - 0.5)));
        pair = voltage | (sense << 16);
      }

      complete = decimator.process(input);
    }

    pickup::AcquisitionProcessor processor;

    processor.begin(frequency, decimator.sample_rate_hz());
    processor.process(output.data(), output.size());

    const auto result = processor.finish(2, 100000.0F);

    assert(result);

    const auto expected = std::polar(200000.0F, 0.5F);

    assert(std::abs(result->impedance / expected - 1.0F) < 0.005F);

    const auto settling =
        pickup::FourthOrderMovingAverage::settling_time_seconds(decimator.sample_rate_hz());

    assert(settling * frequency >= 2.0F && settling * frequency < 13.0F);
  }
}
}  // namespace

int main() {
  test_nco_accuracy_and_wrap();
  test_nco_refill_and_retune();
  test_decimator_boundaries_and_reset();
  test_low_frequency_accumulation();
  test_decimated_impedance();
}
