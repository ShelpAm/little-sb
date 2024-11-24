#include "server.h"
#include "battle.h"
#include "player.h"
#include "server-command-executor.h"
#include <source_location>
#include <spdlog/spdlog.h>

auto Server::instance(std::uint16_t bind_port) -> Server &
{
  static Server the_instance{bind_port};
  return the_instance;
}

void Server::run()
{
  spdlog::trace("Call {}", std::source_location::current().function_name());

  spdlog::info("Server started. Accepting connections...");
  _session_service.start();
  run_main_game_loop();
  spdlog::info("Server running ended.");
}

void Server::shutdown()
{
  spdlog::info("Server shutting down...");

  _session_service.stop();
  _main_game_loop_should_stop = true;
}

auto Server::io_context() -> asio::io_context &
{
  return _io_context;
}

Server::Server(std::uint16_t bind_port)
    : _game_map{10, 20},
      _store_items{{"First aid kit",
                    item::Item_info{.name{"First aid kit"}, .price = 3}}},
      _session_service(this, bind_port, "Server")
{
  register_command_executor<Say_server_command_executor>();
  register_command_executor<Escape_server_command_executor>();
  register_command_executor<Fuck_server_command_executor>();
  register_command_executor<server_command_executors::Resurrect>();
  // register_command_executor(
  //     std::make_unique<Query_event_server_command_executor>(this));
}

constexpr auto Server::tick_interval()
{
  using namespace std::chrono_literals;
  return static_cast<std::chrono::nanoseconds>(1s) / max_tick_per_second;
}

void Server::remove_player(std::string const &player_name)
{
  spdlog::trace("Call {}", std::source_location::current().function_name());

  _players.extract(player_name);
}

auto Server::allocate_game(std::array<Player *, 2> players) -> Battle &
{
  auto const id{_battles.empty() ? std::uint64_t{}
                                 : _battles.rbegin()->first + 1};
  return _battles.try_emplace(id, Battle{id, players, &_session_service})
      .first->second;
}

void Server::run_main_game_loop()
{
  spdlog::info("Main game loop started");

  auto last_update{std::chrono::steady_clock::now()};
  while (!_main_game_loop_should_stop) {
    _io_context.poll();

    if (auto const now{std::chrono::steady_clock::now()};
        now >= last_update + tick_interval()) {
      for (auto &[_, game] : _battles) {
        game.update(now - last_update);
      }

      last_update = std::chrono::steady_clock::now();
    }
  }

  spdlog::info("Main game loop over");
}

auto Server::verify_userinfo(Packet::Sender const &user) const -> bool
{
  // TODO(ShelpAm): replace this placeholder implementation.
  return user.username() == user.password();
}
