#include "measurement.hpp"

namespace pickup::protocol {
void MeasurementModule::acquired(std::uint32_t endpoint, const ProcessedMeasurement& sample) const {
  StaticJsonDocument<768> document;
  document["type"] = "event";
  document["object"] = "measurement";
  auto data = document.createNestedObject("data");
  data["f"] = sample.frequency_hz;
  data["range"] = sample.range_index;
  data["rsense"] = sample.sense_resistor_ohm;
  auto v = data.createNestedObject("v");
  v["re"] = sample.v.real();
  v["im"] = sample.v.imag();
  auto vsense = data.createNestedObject("vsense");
  vsense["re"] = sample.vsense.real();
  vsense["im"] = sample.vsense.imag();
  data["v_min"] = sample.v_min;
  data["v_max"] = sample.v_max;
  data["vsense_min"] = sample.vsense_min;
  data["vsense_max"] = sample.vsense_max;
  auto impedance = data.createNestedObject("z");
  impedance["re"] = sample.impedance.real();
  impedance["im"] = sample.impedance.imag();

  send(endpoint, encode(document));
}

void MeasurementModule::invalid_signal(std::uint32_t endpoint) const {
  const RequestContext context{
      endpoint,
      0,
      "measurement",
      "acquire"
  };

  send(
      endpoint,
      response(context, {"invalid_signal", "acquisition produced no valid sense signal"})
  );
}
}  // namespace pickup::protocol
