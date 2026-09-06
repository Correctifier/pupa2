#pragma once
#include <span>

#include "protocol/module.hpp"

namespace pickup::protocol {

// Registration storage and modules must remain valid whenever poll() is called.
class Router {
 public:
  Router(bsp::Transport& transport, std::span<Module* const> modules)
      : transport_(transport), modules_(modules) {}

  virtual ~Router() = default;

  Router(const Router&) = delete;

  Router& operator=(const Router&) = delete;

  void poll();

 protected:
  explicit Router(bsp::Transport& transport) : transport_(transport) {}

  virtual EncodedMessage dispatch_message(
      const RequestContext& request,
      const ApplicationMessage& message
  ) = 0;

  // Derived protocols register their owned modules after member construction.
  void register_modules(std::span<Module* const> modules) {
    modules_ = modules;
  }

 private:
  EncodedMessage dispatch(const bsp::ReceivedLine& line);
  bsp::Transport& transport_;
  std::span<Module* const> modules_{};
};

}  // namespace pickup::protocol
