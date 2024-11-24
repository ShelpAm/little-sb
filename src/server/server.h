#pragma once

#include "battle-fwd.h"
#include "game-map.h"
#include "item/item.h"
#include "packet.h"
#include "server/server-command-executor.h"
#include "server/session-service.h"
#include <asio.hpp>
#include <map>

class Command;
class Player;
struct Packet;
class Server_command_executor;

class Server {
  friend class Say_server_command_executor;
  friend class Query_event_server_command_executor;
  friend class Fuck_server_command_executor;
  friend class Escape_server_command_executor;
  friend class server_command_executors::Resurrect;

private:
  friend class Session_service;

public:
  static auto instance(std::uint16_t bind_port = 0) -> Server &;
  void run();
  void shutdown();
  auto io_context() -> asio::io_context &;

private:
  explicit Server(std::uint16_t bind_port);

  [[nodiscard]] static constexpr auto tick_interval();

  // To use a server command executor, you should register it here first.
  template <typename Derived_server_command_executor>
    requires(std::derived_from<Derived_server_command_executor,
                               Server_command_executor>)
  void register_command_executor();

  void remove_player(std::string const &player_name);
  auto allocate_game(std::array<Player *, 2> players) -> Battle &;
  void run_main_game_loop();
  [[nodiscard]] auto verify_userinfo(Packet::Sender const &user) const -> bool;

  std::atomic<bool> _main_game_loop_should_stop;

  Game_map _game_map;

  std::map<Battle_id, Battle> _battles;
  std::map<std::string, item::Item_info> _store_items;
  std::map<std::string, std::unique_ptr<Player>> _players;
  std::map<std::string, std::unique_ptr<Server_command_executor>>
      _server_commands;

  asio::io_context _io_context;
  Session_service _session_service;

  static constexpr std::size_t max_tick_per_second{3};
};

template <typename Derived_server_command_executor>
  requires(std::derived_from<Derived_server_command_executor,
                             Server_command_executor>)
void Server::register_command_executor()
{
  auto executor{std::make_unique<Derived_server_command_executor>(this)};
  auto name{executor->name()};
  _server_commands.insert({name, std::move(executor)});
}
