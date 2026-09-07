#include <array>
#include <cassert>
#include <cmath>
#include <complex>

#include "pickup_circuit.hpp"
#include "range_selection.hpp"

namespace {
constexpr double tau = 6.28318530717958647692;

void test_steady_state() {
  for (const double resistance : pickup::sense_resistors_ohm) {
    for (const double frequency : {
        20.0,
        1000.0,
        8000.0,
        20000.0
    }) {
      pickup::bsp::pc::PickupCircuit circuit;

      circuit.set_sense_resistor(resistance);
      circuit.set_generator(frequency, 0.25);
      circuit.advance(1.0);

      std::complex<double> voltage{}, sense{};

      for (int index = 1; index <= 64; ++index) {
        const auto sample = circuit.advance(1.0 / (frequency * 64));
        const auto oscillator = std::polar(1.0, -tau * index / 64);
        voltage += sample.v * oscillator;
        sense += sample.vsense * oscillator;
      }

      const std::complex<double> jw(0, tau * frequency);
      const auto expected =
          1.0 / (1.0 / (7000.0 + jw * 3.0) + jw * 120e-12 + 1.0 / 1000000.0);
      const auto measured = resistance * voltage / sense;

      assert(std::abs(measured / expected - 1.0) < 1e-8);
    }
  }
}

void test_state_continuity() {
  pickup::bsp::pc::PickupCircuit circuit;

  circuit.advance(0.010123);

  const auto before = circuit.sample();
  auto unchanged = circuit;

  circuit.set_sense_resistor(1000);

  const auto switched = circuit.sample();

  assert(switched.v == before.v);
  assert(switched.inductor_current_a == before.inductor_current_a);
  assert(switched.source_v == before.source_v);

  const auto after = circuit.advance(1e-8);
  const auto old_range = unchanged.advance(1e-8);

  assert(std::abs(after.v - old_range.v) > 1e-7);

  const auto state = circuit.sample();

  circuit.set_generator(17000, 0.25);
  assert(circuit.sample().v == state.v);
  assert(circuit.sample().inductor_current_a == state.inductor_current_a);
  assert(circuit.sample().source_v == state.source_v);
  circuit.set_generator(17000, 0.5);
  assert(circuit.sample().v == state.v);
  assert(circuit.sample().inductor_current_a == state.inductor_current_a);
  assert(circuit.sample().source_v == 2 * state.source_v);
}

void test_switch_transient_derivative() {
  pickup::bsp::pc::PickupCircuit circuit;

  circuit.advance(0.010123);
  circuit.set_sense_resistor(1000);

  const auto before = circuit.sample();
  const double dv = (before.source_v - before.v) / (1000 * 120e-12) -
                    before.v / (1000000 * 120e-12) - before.inductor_current_a / 120e-12;
  const double di = (before.v - 7000 * before.inductor_current_a) / 3;
  constexpr double dt = 1e-11;
  const auto after = circuit.advance(dt);

  assert(std::abs((after.v - before.v) / dt / dv - 1) < 0.002);
  assert(std::abs((after.inductor_current_a - before.inductor_current_a) / dt / di - 1) < 0.002);
}

void test_critical_damping() {
  pickup::bsp::pc::PickupCircuit single;

  // A = [-1, -1; 1, -3] has an exactly repeated pole at -2.
  single.set_parameters({
      3.0,
      1.0,
      1e12,
      0.0
  });
  single.set_sense_resistor(1);
  single.set_generator(1, 1);

  auto divided = single;
  const auto sample = single.advance(0.25);

  for (int step = 0; step < 25; ++step) {
    divided.advance(0.01);
  }

  assert(std::abs(sample.v - divided.sample().v) < 1e-12);
  assert(std::abs(sample.inductor_current_a - divided.sample().inductor_current_a) < 1e-12);
}

void test_partition_and_decay() {
  for (const double resistance : pickup::sense_resistors_ohm) {
    pickup::bsp::pc::PickupCircuit single;

    single.set_sense_resistor(resistance);
    single.set_parameters({
        100.0,
        20.0,
        1.0,
        0.0
    });
    single.set_generator(1234, 1);

    auto divided = single;
    const auto end = single.advance(0.002);

    for (int index = 0; index < 100; ++index) {
      divided.advance(0.00002);
    }

    assert(std::abs(end.v - divided.sample().v) < 1e-9);
    assert(std::abs(end.inductor_current_a - divided.sample().inductor_current_a) < 1e-12);

    single.set_generator(1234, 0);

    double previous_energy = INFINITY;

    for (int index = 0; index < 100; ++index) {
      const auto sample = single.advance(0.001);
      const double energy = 0.5e-12 * sample.v * sample.v +
                            10.0 * sample.inductor_current_a * sample.inductor_current_a;

      assert(std::isfinite(energy));
      assert(energy <= previous_energy * (1 + 1e-12));

      previous_energy = energy;
    }

    single.advance(100);
    assert(std::abs(single.sample().v) < 1e-12);
    assert(std::abs(single.sample().inductor_current_a) < 1e-12);
  }
}
}  // namespace

int main() {
  static_assert(static_cast<std::uint32_t>(pickup::RangeSelection::ohm_1k) == 0);
  static_assert(static_cast<std::uint32_t>(pickup::RangeSelection::ohm_10k) == 1);
  static_assert(static_cast<std::uint32_t>(pickup::RangeSelection::ohm_100k) == 2);
  static_assert(static_cast<std::uint32_t>(pickup::RangeSelection::ohm_1m) == 3);
  static_assert(pickup::sense_resistors_ohm[pickup::startup_range_index] == 100000.0F);

  test_steady_state();
  test_state_continuity();
  test_switch_transient_derivative();
  test_critical_damping();
  test_partition_and_decay();
}
