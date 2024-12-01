#include "command.h"
#include <chrono>
#include <format>
#include <spdlog/spdlog.h>

void to_json(json &j, const Command &cmd)
{
  j = cmd._data;
}

void from_json(const json &j, Command &cmd)
{
  cmd._data = j;
}

Command::Command(char const *name) : Command(std::string{name}) {}

Command::Command(std::string name) : _data(json::object())
{
  // spdlog::debug("Creating command from std::string: {}", name);
  _data["name"] = name;
  std::chrono::zoned_time now{std::chrono::current_zone(),
                              std::chrono::system_clock::now()};
  _data["created_time"] = std::format(
      "{:%F %T}",
      std::chrono::round<std::chrono::seconds>(now.get_local_time()));
}

Command::Command(json data) : _data(std::move(data))
{
  // spdlog::debug("Creating command from json: {}", _data.dump());
  std::chrono::zoned_time now{std::chrono::current_zone(),
                              std::chrono::system_clock::now()};
  _data["created_time"] = std::format(
      "{:%F %T}",
      std::chrono::round<std::chrono::seconds>(now.get_local_time()));
}

auto Command::name() const -> std::string const &
{
  static std::string empty;
  if (!_data.contains("name")) {
    return empty;
  }
  return _data["name"].get_ref<std::string const &>();
}

auto Command::dump() const -> std::string
{
  return _data.dump();
}
auto Command::args() -> json &
{
  return _data["args"];
}

auto Command::args() const -> json const &
{
  return _data["args"];
}
auto Command::created_time() const -> std::string
{
  return _data["created_time"].get<std::string>();
}
