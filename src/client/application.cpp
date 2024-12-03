#include "application.h"
#include "event.h"
#include "handle-error.h"
#include "imgui.h"
#include "item/item.h"
#include "player.h"
#include <asio.hpp>
#include <cassert>

Application::Application()
    : _start_time{std::chrono::steady_clock::now()}, _window{this}
{
  spdlog::trace("Call {}", std::source_location::current().function_name());
}

void Application::run()
{
  spdlog::info("Application started");

  while (!should_stop()) {
    tick();
  }

  // TODO(shelpam): This blocks, fix it.
  if (_you) {
    spdlog::info("Logging out from server...");
    async_request(Command{"logout"}, [](Event const &e) {
      if (e.name() != "ok") {
        throw std::runtime_error{"Logout failed"};
      }
    });
    _io_context.poll();
  }

  spdlog::info("Application stopped");
}

auto Application::should_stop() const -> bool
{
  return _state == State::should_stop || _window.should_close();
}

void Application::tick()
{
  spdlog::trace("Ticking");
  poll_events();
  update();
  render();
}

void Application::poll_events()
{
  // If `_io_context.stopped()` returns `true`, any call to `_io_context.poll()`
  // (the same as poll_one run run_one) won't do anything at all. So, we should
  // restart them.
  if (_io_context.stopped()) {
    _io_context.restart();
  }
  _io_context.poll();

  _window.poll_events();
}

void Application::update()
{
  static Duration last_tick{current_time()};
  auto const now{current_time()};
  _frame_per_second = 1.0s / (now - last_tick);
  last_tick = now;

  // Async query event would be started before main loop.

  if (_you) {
    async_request(Command{"sync"}, [this](Event const &e) {
      _you = std::make_unique<Player>(e.get_arg<Player>(0));
    });
  }

  switch (_state) {
  case State::greeting:
    async_request(Command{"list-store-items"}, [this](Event const &e) {
      _store_items = e.get_arg<std::map<std::string, item::Item_info>>(0);
    });
    async_request(Command{"get-game-map"}, [this](Event const &e) {
      _game_map = std::make_unique<Game_map>(e.get_arg<Game_map>(0));
    });
    break;
  case State::starting_battle:
    async_request(Command{"list-players"}, [this](Event const &e) {
      if (e.name() != "ok") {
        throw std::runtime_error{
            "list-players returns {}, which is impossible."};
      }
      auto players{e.get_arg<std::vector<Player>>(0)};
      update_players(players);
    });
  default:
    break;
  }

  remove_expired_messages();
}

void Application::render()
{
  {
    _window.pane_begin("Player info"s);
    _window.font_scale(3);

    static Duration last_sample_fps{current_time()};
    static long double sampled_fps;
    if (auto const current_sample_fps{current_time()};
        current_sample_fps > last_sample_fps + 1s) {
      sampled_fps = _frame_per_second;
      last_sample_fps = current_time();
    }
    _window.text(std::format("fps: {:.0f}", sampled_fps));

    if (_you) {
      show_player_info();
    }

    switch (_state) {
    case State::unlogged_in:
      render_unlogged_in();
      break;
    case State::logging:
      _window.text("Logging in...");
      break;
    case State::greeting:
      handle_greeting();
      break;
    case State::starting_battle:
      handle_starting_battle();
      break;
    case State::battling:
      handle_battling();
      break;
    case State::ended:
      handle_ended();
      break;
    default: // Unreachable
      break;
    }

    _window.text("");
    render_messages();

    _window.pane_end();
  }

  {
    _window.pane_begin("Battle field");
    _window.font_scale(3);

    // if (ImGui::IsItemFocused()) {
    //   if () {
    //   }
    // }

    if (_game_map) {
      for (auto const &line : _game_map->to_char_matrix()) {
        _window.text(line | std::ranges::to<std::string>());
      }
    }
    _window.pane_end();
  }

  _window.render();
}

void Application::show_player_info()
{
  _window.text(std::format("Your name: {}", _you->name()));
  _window.text(std::format("Your health: {}", _you->health()));
  if (_you->health() == 0) {
    ImGui::SameLine();
    if (_window.button("Resurrect")) {
      async_request(Command{"resurrect"}, [](Event const &e) {
        if (e.name() != "ok") {
          spdlog::error(e.get_arg<std::string>(0));
          // throw std::runtime_error{e.get_arg<std::string>(0)};
        }
      });
    }
  }
  _window.text(std::format("Your damage range: {}, {}",
                           _you->damage_range().first,
                           _you->damage_range().second));
  _window.text(
      std::format("Your critical hit rate: {:.3}", _you->critical_hit_rate()));
  _window.text(std::format("Your defense: {}", _you->defense()));
  _window.text(std::format("Your money left: {}", _you->money()));
  _window.text(std::format("Your position: ({}, {})", _you->position().x(),
                           _you->position().y()));

  // TODO(shelpam): show game map here
}

void Application::handle_greeting()
{
  spdlog::trace("Call {}", std::source_location::current().function_name());

  // ImGui::LabelText("lebel", "fmt");
  ImGui::InputTextWithHint("Message", "type your message here...",
                           _message_input_buf.data(),
                           _message_input_buf.size());
  ImGui::SameLine();
  if (_window.button("Send")) {
    Command say{"say"s};
    say.add_arg(std::string{_message_input_buf.data(),
                            std::strlen(_message_input_buf.data())});
    async_request(say, [](Event const &e) {
      if (e.name() != "ok") {
        spdlog::warn("Failed to send message.");
        return;
      }
      spdlog::info("Message successfully sent.");
    });
  }
  if (_window.button("Start battle")) {
    _state = State::starting_battle;
  }
  if (_window.button("Fuck others")) {
    async_request(Command{"fuck"}, [this](Event const &event) {
      if (event.name() == "ok") {
        add_to_show("Fuck succeeded");
      }
    });
  }
  _window.text("");
  _window.text("Store: (Press button to buy goods)");
  _window.text("When you buy it, you use it.");
  for (auto const &[_, item] : _store_items) {
    if (_window.button(std::format("{} (${})", item.name, item.price))) {
      Command buy{"buy"};
      buy.add_arg(item.name);
      async_request(buy, [this](Event const &e) {
        if (e.name() != "ok") {
          add_to_show(e.get_arg<std::string>(0));
        }
      });
    }
  }
}

void Application::handle_starting_battle()
{
  spdlog::trace("Entering {}", std::source_location::current().function_name());

  starting_new_game();

  spdlog::trace("Leaving {}", std::source_location::current().function_name());
}

void Application::handle_battling()
{
  if (_battled_rounds >= 6) {
    if (_window.button("Escape from the battle")) {
      Command escape{"escape"};
      escape.set_param("game-id", _battle_id);
      async_request(escape, [this](Event const &e) {
        if (e.name() != "ok") {
          add_to_show(e.get_arg<std::string>(0));
          return;
        }
        add_to_show("You have escaped from the battle.");
        _state = State::ended;
      });
    }
  }
}

void Application::handle_ended()
{
  spdlog::trace("Entering {}", std::source_location::current().function_name());

  _battled_rounds = 0;
  _state = State::greeting;

  spdlog::trace("Leaving {}", std::source_location::current().function_name());
}

void Application::starting_new_game()
{
  _window.text("Making new game...");
  _window.text("Who do you want battle with?");
  if (_window.button("Go back")) {
    _state = State::greeting;
  }
  _window.text("(Note: only players visible to you are shown.)");
  for (auto const &[_, player] : _players) {
    if (!_you->can_see(*player)) {
      continue;
    }
    if (_window.button(player->name())) {
      Command battle{"battle"};
      battle.add_arg(player->name());
      async_request(battle, [this](Event const &e) {
        if (e.name() == "ok") {
          _battle_id = e.get_param<std::size_t>("game-id");
          _state = State::battling;
          add_to_show("Game started.");
        }
        else if (e.name() == "error") {
          spdlog::warn("{}", e.get_arg<std::string>(0));
          add_to_show("error: " + e.get_arg<std::string>(0));
        }
        else {
          spdlog::warn("Unknown result: {}", e.name());
        }
      });
    }
  }
}

void Application::process_event(Event const &event)
{
  if (event.name() == "none") {
    return;
  }

  spdlog::info("Event: {}", event.dump());

  if (event.name() == "broadcast") {
    auto const from{event.get_param<std::string>("from")};
    auto const what{event.get_arg<std::string>(0)};
    spdlog::info("{} said: {}", from, what);
    add_to_show(
        std::format("In {}, {} said: {}", event.created_time(), from, what));
  }
  else if (event.name() == "battle") {
    add_to_show(std::format("{} called for a fight with you.",
                            event.get_param<std::string>("from")));
  }
  else if (event.name() == "cure") {
    _you->heal(event.get_arg<int>(0));
    add_to_show(event.get_param<std::string>("cause"));
  }
  else if (event.name() == "fuck") {
    add_to_show(std::format("You are fucked by {}",
                            event.get_param<std::string>("fucker")));
  }
  else if (event.name() == "health-drop") {
    auto const who{event.get_param<std::string>("player")};
    auto const drop{event.get_param<int>("drop")};
    add_to_show(std::format("{} has dropped {} health", who, drop));
    if (who == _you->name()) {
      _you->take_damage(drop);
    }
    ++_battled_rounds;
  }
  else if (event.name() == "game-end") {
    add_to_show("Game ended.");
    if (_you->health() == 0) {
      add_to_show("You have lost.");
    }
    else {
      add_to_show("You are victorior.");
    }
    _state = State::ended;
  }
  else if (event.name() == "message") {
    if (event.get_arg<std::string>(0) == "game-end") {
      add_to_show("Game ended.");
      if (_you->health() == 0) {
        add_to_show("You have lost.");
      }
      else {
        add_to_show("You are victorior.");
      }
    }
    else {
      add_to_show(event.get_arg<std::string>(0));
    }
  }
  else {
    spdlog::warn("Unknown event: {}", event.name());
  }
}

void Application::add_to_show(std::string message)
{
  using namespace std::chrono_literals;
  constexpr auto existing_time{24h};
  _messages.insert(Message{.expiring_time{current_time() + existing_time},
                           .content{std::move(message)}});
}

void Application::async_request(Command const &command,
                                std::function<void(Event)> on_replied)
{
  spdlog::debug("Scheduling request: {}", command.dump());
  Packet packet{Packet::Sender{_name, _name}, command.dump()};
  _session->schedule_request(
      std::move(packet), [on_replied{std::move(on_replied)}](Packet packet) {
        on_replied(Event{json::parse(std::move(packet.payload))});
      });
}

void Application::schedule_continuous_query_event()
{
  async_request(Command{"query-event"}, [this](Event const &event) {
    process_event(event);
    schedule_continuous_query_event();
  });
}

auto connect(asio::io_context &io_context, std::string_view host,
             std::string_view port) -> tcp::socket
{
  spdlog::trace("Call {}", std::source_location::current().function_name());

  tcp::socket socket{io_context};
  auto endpoints{tcp::resolver{io_context}.resolve(host, port)};
  std::error_code ec;
  // TODO(shelpam): use asio::async_connect here.
  asio::connect(socket, endpoints, ec);
  handle_error(ec);
  return socket;
}
auto Application::current_time() const -> Duration
{
  return std::chrono::steady_clock::now() - _start_time;
}
void Application::remove_expired_messages()
{
  auto const now{current_time()};
  decltype(_messages.begin()) it{_messages.begin()};

  while (!_messages.empty() && it->expiring_time < now) {
    it = _messages.erase(it);
  }
}
void Application::render_unlogged_in()
{
  // TODO(shelpam): refactor: extract `input text` into `Window` class.
  ImGui::InputText("Your name here", _name_buf.data(), _name_buf.size());
  _name = std::string(_name_buf.data(), std::strlen(_name_buf.data()));
  ImGui::InputText("Server host", _host_buf.data(), _host_buf.size());
  if (_window.button("Login")) {
    if (_name.empty()) {
      add_to_show("error: Your name can not be empty. Please retry.");
      return;
    }
    try {
      connect_to_the_server();
      async_request(Command{"login"}, [this](Event const &e) {
        if (e.name() != "ok") {
          assert(false);
        }
        _you = std::make_unique<Player>(e.get_arg<Player>(0));
        schedule_continuous_query_event();
        _state = State::greeting;
      });
      _state = State::logging;
    }
    catch (std::system_error const &e) {
      if (e.code() == asio::error::host_not_found) {
        add_to_show(
            "Cannot connect to the server. Host not found. Please check "
            "whether the server is down, or you mistyped the host.");
      }
      else if (e.code() == asio::error::connection_refused) {
        add_to_show("Cannot connect to the server. Connection refused.");
      }
      else if (e.code() == asio::error::timed_out) {
        add_to_show("Cannot connect to the server. Connection time out.");
      }
      else {
        spdlog::error(e.what());
        throw;
      }
    }
    catch (std::runtime_error const &e) {
      if (e.what() == "Connection timed out"sv) {
        add_to_show("Connection timed out");
      }
      else {
        throw;
      }
    }
  }
}
void Application::render_messages()
{
  _window.text("Messages:");
  ImGui::BeginChild("Scrolling");

  // Since the scroll data will be updated in the next frame, so we should
  // save last frame data, to update current frame scrolling position.
  static bool was_at_bottom{};
  static bool last_frame_new_message_arrived{};
  static std::size_t last_messages_size{};

  if (last_frame_new_message_arrived && was_at_bottom) {
    // Scrolls down
    ImGui::SetScrollY(ImGui::GetScrollMaxY());
  }

  for (auto const &msg : _messages) {
    // In order to make the font size seems to be the same with outter text, and
    // the actual font size is calculated by `outter-size * inner-scale`, we
    // should set the second parameter `scale` to 1 here.
    _window.text(std::format("{}", msg.content));
  }

  was_at_bottom = ImGui::GetScrollY() == ImGui::GetScrollMaxY();
  last_frame_new_message_arrived = _messages.size() != last_messages_size;
  last_messages_size = _messages.size();

  ImGui::EndChild();

  if (_window.button("Clear messages")) {
    _messages.clear();
  }
}
void Application::connect_to_the_server()
{
  _session = std::make_shared<Session>(
      connect(_io_context, std::string_view{_host_buf}, "1438"));
}
void Application::update_players(std::vector<Player> const &players)
{
  _players.clear();
  for (auto &&p : players) {
    _players.insert({p.name(), std::make_unique<Player>(p)});
  }
}
Application::~Application() = default;
auto Application::state() const -> State
{
  return _state;
}
