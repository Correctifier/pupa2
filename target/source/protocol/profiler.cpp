#include "profiler.hpp"

#include <array>

namespace pickup::protocol {
namespace {
constexpr std::size_t maximum_contexts = 8;

std::string_view type_name(bsp::ProfileContextType type) {
  return type == bsp::ProfileContextType::interrupt ? "interrupt" : "task";
}
}  // namespace

EncodedMessage ProfilerModule::process(const RequestContext& request, JsonVariantConst) {
  if (request.action == "threads") {
    return threads(request);
  }

  if (request.action == "data") {
    return data(request);
  }

  if (request.action == "reset") {
    profiler_.reset();

    return response(request);
  }

  return response(request, unsupported_operation());
}

EncodedMessage ProfilerModule::threads(const RequestContext& request) const {
  StaticJsonDocument<2048> document;
  document["type"] = "response";
  document["object"] = "profiler";
  document["action"] = "threads";
  document["id"] = request.id;
  document["status"] = "ok";
  auto rows = document.createNestedArray("data");

  for (const auto& context : profiler_.contexts()) {
    auto row = rows.createNestedObject();
    row["id"] = context.id;
    row["priority"] = context.priority;
    row["type"] = type_name(context.type);
    row["name"] = context.name;
  }

  return encode(document);
}

EncodedMessage ProfilerModule::data(const RequestContext& request) {
  std::array<bsp::ProfileStatistics, maximum_contexts> storage{};
  const auto statistics = profiler_.statistics(storage);
  StaticJsonDocument<2048> document;
  document["type"] = "response";
  document["object"] = "profiler";
  document["action"] = "data";
  document["id"] = request.id;
  document["status"] = "ok";
  auto rows = document.createNestedArray("data");

  for (const auto& statistic : statistics) {
    auto row = rows.createNestedObject();
    row["id"] = statistic.id;
    row["count"] = statistic.executions;
    row["total_us"] = statistic.total_us;
    row["avg_us"] = statistic.average_us;
    row["min_us"] = statistic.minimum_us;
    row["max_us"] = statistic.maximum_us;
    row["cpu_percent"] = statistic.cpu_percent;
  }

  return encode(document);
}

}  // namespace pickup::protocol
