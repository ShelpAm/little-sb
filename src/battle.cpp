#include "battle.h"
#include "chrono.h"

Battle::Battle(std::uint64_t const id,
               std::array<player::Player *, 2> const players,
               Session_service *const session_service)
    : _id{id}, _players{players}, _session_service{session_service}
{
}

void Battle::update(Duration delta)
{
  if (_ended) {
    return;
  }

  _time_since_last_update += delta;
  if (_time_since_last_update < 2s) {
    return;
  }
  _time_since_last_update = 0s;

  if (std::ranges::all_of(_players,
                          [](auto &player) { return player->health() != 0; })) {
    auto const &attacker{_players.at(_turn % 2)};
    auto &target{_players.at((_turn % 2) ^ 1)};

    auto const drop{attacker->attack(*target)};

    Event health_drop{"health-drop"};
    health_drop.set_param("player", target->name());
    health_drop.set_param("drop", drop);
    _session_service->push_event(attacker->name(), health_drop);
    _session_service->push_event(target->name(), health_drop);

    ++_turn;
  }
  else {
    stop(Stop_cause::normal);
  }
}

auto Battle::id() const -> std::uint64_t
{
  return _id;
}

auto Battle::pending_events() -> std::queue<std::string> &
{
  return _pending_events;
}

auto Battle::ended() const -> bool
{
  return _ended;
}

void Battle::stop(Stop_cause cause)
{
  switch (cause) {
  case Stop_cause::normal: {
    Event game_end{"game-end"};
    _session_service->push_event(_players[0]->name(), game_end);

    Event message{"message"};
    message.add_arg(cause);
    _session_service->push_event(_players[1]->name(), message);
    break;
  }
  case Stop_cause::escaping: {
    Event message{"message"};
    message.add_arg("Your opponent has escaped from the battle.");
    _session_service->push_event(_players[1]->name(), message);
    break;
  }
  }

  _ended = true;
}
