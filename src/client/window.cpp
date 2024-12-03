#include "window.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdexcept>

std::unordered_map<GLFWwindow *, Window *> Window::forward_table;

void error_callback(int error, const char *description)
{
  spdlog::error("GLFW({}): {}", error, description);
}

void key_callback(GLFWwindow *window, int key, int scancode, int action,
                  int mods)
{
  Window::forward_table[window]->key_callback(key, scancode, action, mods);
}

void Window::initialize()
{
  glfwSetErrorCallback(error_callback);

  if (glfwInit() == 0) {
    // Initialization failed
    throw std::runtime_error{"Failed to initialize GLFW"};
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
}

void Window::deinitialize()
{
  glfwTerminate();
}

Window::Window()
    : _window{glfwCreateWindow(1600, 1200, "Little sb", nullptr, nullptr)}
{
  if (_window == nullptr) {
    throw std::runtime_error{"Window or OpenGL context creation failed"};
  }

  forward_table.insert({_window, this});

  glfwSetKeyCallback(_window, ::key_callback);
  int width;
  int height;
  glfwGetFramebufferSize(_window, &width, &height);

  use();
  glfwSwapInterval(1);
  glViewport(0, 0, width, height);

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |=
      ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
  io.ConfigFlags |=
      ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls

  // Setup Platform/Renderer backends
  ImGui_ImplGlfw_InitForOpenGL(
      _window, true); // Second param install_callback=true will install
                      // GLFW callbacks and chain to existing ones.
  ImGui_ImplOpenGL3_Init();
}

Window::~Window()
{
  forward_table.erase(_window);

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(_window);
}

void Window::use() const
{
  glfwMakeContextCurrent(_window);
}

void Window::poll_events() const
{
  use();
  glfwPollEvents();

  // (Your code calls glfwPollEvents())
  // ...
  // Start the Dear ImGui frame
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  // ImGui::ShowDemoWindow(); // Show demo window! :)
}

auto Window::button(std::string const &label) const -> bool
{
  use();
  return ImGui::Button(label.c_str());
}

void Window::text(std::string const &text) const
{
  use();
  ImGui::Text("%s", text.c_str());
}

void Window::render() const
{
  use();
  glClear(GL_COLOR_BUFFER_BIT);
  // Rendering
  // (Your code clears your framebuffer, renders your other stuff etc.)
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  // (Your code calls glfwSwapBuffers() etc.)
  glfwSwapBuffers(_window);
}

auto Window::should_close() const -> bool
{
  return glfwWindowShouldClose(_window) != 0;
}
void Window::pane_begin(std::string const &name) const
{
  ImGui::Begin(name.c_str());
}

void Window::pane_end() const
{
  ImGui::End();
}
void Window::font_scale(float scale)
{
  ImGui::SetWindowFontScale(scale);
}
void Window::key_callback(int key, int /*scancode*/, int action, int /*mods*/)
{
  // Default: Press Esc to close the window.
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
    glfwSetWindowShouldClose(_window, GLFW_TRUE);
  }
}
void Window::should_close(bool value)
{
  glfwSetWindowShouldClose(_window, static_cast<int>(value));
}
