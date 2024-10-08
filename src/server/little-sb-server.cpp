#include "server.h"
#include <csignal>
#include <spdlog/spdlog.h>

void signal_handler(int /*signal*/)
{
  // Delete entire server
  Server::instance().shutdown();
}

auto main(int /*argc*/, char * /*argv*/[]) -> int
{
  // Signal handler for SIGINT
  std::signal(SIGINT, signal_handler);
  spdlog::set_level(spdlog::level::info); // Set for debugging

#ifdef NDEBUG
  // Disable try-catch in DEBUG mode to allow the debugger to catch and display
  // the original exception location, rather than catching it here and losing
  // the call stack information.
  try {
#endif
    Server::instance(1438).run();
#ifdef NDEBUG
  }
  catch (std::exception &e) {
    spdlog::critical("Exception: {}", e.what());
  }
#endif

  return 0;
}
