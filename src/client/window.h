#pragma once

#include <GLFW/glfw3.h>

#ifndef __gl_h_
#include <glad/gl.h>
#endif

#include <spdlog/spdlog.h>

void error_callback(int error, const char *description);

void key_callback(GLFWwindow *window, int key, int scancode, int action,
                  int mods);

class Window {
public:
  static void initialize();
  static void deinitialize();

  Window();
  Window(const Window &) = delete;
  Window(Window &&) = delete;
  auto operator=(const Window &) -> Window & = delete;
  auto operator=(Window &&) -> Window & = delete;
  ~Window();

  void use() const;
  void poll_events() const;
  void render() const;

  auto button(std::string const &label, float scale = 3) const -> bool;
  void text(std::string const &text, float scale = 3) const;

  [[nodiscard]] auto should_close() const -> bool;

private:
  GLFWwindow *_window;
};
