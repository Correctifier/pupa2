#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace pickup::bsp {

enum class ProfileContextType {
  task,
  interrupt,
};

struct ProfileContext {
  std::uint32_t id{};
  std::uint32_t priority{};
  ProfileContextType type{};
  std::string_view name;
};

struct ProfileStatistics {
  std::uint32_t id{};
  std::uint64_t executions{};
  double total_us{};
  double average_us{};
  double minimum_us{};
  double maximum_us{};
  double cpu_percent{};
};

class Profiler {
 public:
  virtual ~Profiler() = default;

  virtual std::span<const ProfileContext> contexts() const = 0;
  // Writes one entry per context and returns the initialized portion of output.
  virtual std::span<ProfileStatistics> statistics(std::span<ProfileStatistics> output) = 0;
  virtual void reset() = 0;
};

class NullProfiler final : public Profiler {
 public:
  std::span<const ProfileContext> contexts() const override {
    return {};
  }

  std::span<ProfileStatistics> statistics(std::span<ProfileStatistics>) override {
    return {};
  }

  void reset() override {}
};

inline Profiler& null_profiler() {
  static NullProfiler profiler;

  return profiler;
}

}  // namespace pickup::bsp
