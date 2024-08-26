#pragma once

#include "packet.h"
#include "session-service.h"
#include <asio.hpp>
#include <map>

class Game;
class Command;
class Player;
struct Packet;
class Server_command_executor;

class Server {
  friend class Say_server_command_executor;
  friend class Query_event_server_command_executor;
  friend class Fuck_server_command_executor;
  friend class Session_service;

public:
  static auto instance(std::uint16_t bind_port = 0) -> Server &;
  void run();
  void shutdown();
  auto io_context() -> asio::io_context &;

private:
  explicit Server(std::uint16_t bind_port);
  [[nodiscard]] static constexpr auto tick_interval();
  void
  register_command_executor(std::unique_ptr<Server_command_executor> executor);
  void remove_player(std::string const &player_name);
  auto allocate_game(std::array<Player *, 2> const &players) -> Game &;
  void run_main_game_loop();
  auto verify_userinfo(Packet_sender const &user) const -> bool;

  std::atomic<bool> _main_game_loop_should_stop;
  std::map<std::uint64_t, Game> _games;
  std::map<std::string, Player> _players;
  std::map<std::string, std::unique_ptr<Server_command_executor>>
      _server_commands;

  asio::io_context _io_context;
  Session_service _session_service;

  static constexpr std::size_t max_tick_per_second{60};
};
