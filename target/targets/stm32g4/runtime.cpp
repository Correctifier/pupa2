#include <cerrno>
#include <cstddef>

#include "stm32g4xx_hal.h"

namespace {
[[noreturn]] void allocation_failure() {
  __disable_irq();

  while (true) {
    __WFI();
  }
}
}  // namespace

// Static application objects must never reach an allocating/deleting path.
void* operator new(std::size_t) {
  allocation_failure();
}

void* operator new[](std::size_t) {
  allocation_failure();
}

void operator delete(void* pointer) noexcept {
  if (pointer != nullptr) {
    allocation_failure();
  }
}

void operator delete(void* pointer, std::size_t) noexcept {
  if (pointer != nullptr) {
    allocation_failure();
  }
}

void operator delete[](void* pointer) noexcept {
  if (pointer != nullptr) {
    allocation_failure();
  }
}

void operator delete[](void* pointer, std::size_t) noexcept {
  if (pointer != nullptr) {
    allocation_failure();
  }
}

extern "C" void* _sbrk(std::ptrdiff_t) {
  errno = ENOMEM;

  return reinterpret_cast<void*>(-1);
}

extern "C" int _write(
    int,
    const char*,
    int
) {
  errno = ENOSYS;

  return -1;
}

extern "C" int _read(
    int,
    char*,
    int
) {
  errno = ENOSYS;

  return -1;
}

extern "C" int _close(int) {
  errno = ENOSYS;

  return -1;
}

extern "C" int _lseek(
    int,
    int,
    int
) {
  errno = ENOSYS;

  return -1;
}

extern "C" [[noreturn]] void __assert_func(
    const char*,
    int,
    const char*,
    const char*
) {
  allocation_failure();
}
