#pragma once

#include <source_location>
#include <spdlog/spdlog.h>
#include <system_error>

inline void handle_error(std::error_code ec)
{
  spdlog::trace("Call {}", std::source_location::current().function_name());

  if (ec) {
    throw std::runtime_error{ec.message()};
  }
}
