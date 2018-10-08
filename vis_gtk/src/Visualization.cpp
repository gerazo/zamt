#include "zamt/vis_gtk/Visualization.h"

#include "zamt/core/Core.h"
#include "zamt/core/Log.h"
#include "zamt/core/ModuleCenter.h"

#include <algorithm>
#include <cassert>

#include <glibmm/main.h>

namespace zamt {

const char* Visualization::kModuleLabel = "vis_gtk";
const char* Visualization::kGTKApplicationID = "hu.lib.zamt";
const char* Visualization::kActivationsPerSecondParamStr = "-fps";

Visualization::Visualization(int argc, const char* const* argv)
    : cli_(argc, argv) {
  log_.reset(new Log(kModuleLabel, cli_));
  if (cli_.HasParam(Core::kHelpParamStr)) {
    PrintHelp();
    return;
  }
  int fps = cli_.GetNumParam(kActivationsPerSecondParamStr);
  activations_per_second_ =
      (fps == CLIParameters::kNotFound) ? kActivationsPerSecond : fps;
  log_->LogMessage("Starting...");
  visualization_loop_.reset(new std::thread(&Visualization::RunMainLoop, this));
}

Visualization::~Visualization() {
  if (!visualization_loop_) return;
  visualization_loop_->join();
  log_->LogMessage("Visualization thread stopped.");
}

void Visualization::Initialize(const ModuleCenter* mc) {
  mc_ = mc;
  Core& core = mc_->Get<Core>();
  core.RegisterForQuitEvent(
      std::bind(&Visualization::Shutdown, this, std::placeholders::_1));
}

void Visualization::Shutdown(int /*exit_code*/) {
  shutdown_initiated_ = true;
  log_->LogMessage("Stopping...");
}

void Visualization::OpenWindow(const char* window_title, int width, int height,
                               int& window_id) {
  while (windows_mutex_.test_and_set(std::memory_order_acquire))
    ;
  size_t id = 0;
  while (id < windows_.size()) {
    if (windows_[id].IsEmpty() && !windows_[id].IsInitialized()) break;
    id++;
  }
  if (id >= windows_.size()) windows_.emplace_back();
  assert(id < windows_.size());
  windows_[id].window_title_ = window_title;
  windows_[id].width_ = width;
  windows_[id].height_ = height;
  assert(!windows_[id].IsEmpty() && !windows_[id].IsInitialized());
  window_id = (int)id;
  windows_mutex_.clear(std::memory_order_release);
}

void Visualization::CloseWindow(int window_id) {
  while (windows_mutex_.test_and_set(std::memory_order_acquire))
    ;
  size_t id = (size_t)window_id;
  assert(id < windows_.size() && !windows_[id].IsEmpty() &&
         windows_[id].IsInitialized());
  windows_[id].window_title_ = nullptr;
  assert(windows_[id].IsEmpty() && windows_[id].IsInitialized());
  windows_mutex_.clear(std::memory_order_release);
}

void Visualization::QueryRender(int window_id, RenderCallback render_callback) {
  assert(render_callback);
  size_t id = (size_t)window_id;
  assert(id < windows_.size() && !windows_[id].IsEmpty());
  while (windows_[id].callback_mutex_.test_and_set(std::memory_order_acquire))
    ;
  windows_[id].queried_callback_ = render_callback;
  windows_[id].callback_mutex_.clear(std::memory_order_release);
}

void Visualization::Window::OpenWindow(
    Glib::RefPtr<Gtk::Application>& application) {
  assert(window_title_ && width_ && height_ && !window_ && !canvas_);
  window_.reset(new Gtk::Window());
  window_->set_title(window_title_);
  window_->resize(width_, height_);
  canvas_.reset(new Gtk::DrawingArea());
  window_->add(*canvas_);
  window_->set_application(application);
  window_->show();
  canvas_->show();
  canvas_->signal_draw().connect(
      sigc::mem_fun(this, &Visualization::Window::OnDraw), false);
}

void Visualization::Window::CloseWindow(
    Glib::RefPtr<Gtk::Application>& /*application*/) {
  assert(window_ && window_ && canvas_);
  window_->unset_application();
  window_.reset(nullptr);
  canvas_.reset(nullptr);
  queried_callback_ = nullptr;
}

bool Visualization::Window::OnDraw(const Cairo::RefPtr<Cairo::Context>& cr) {
  if (shutdown_initiated_ || !window_ || !canvas_ || !queried_callback_)
    return true;
  while (callback_mutex_.test_and_set(std::memory_order_acquire))
    ;
  RenderCallback callback = queried_callback_;
  queried_callback_ = nullptr;
  callback_mutex_.clear(std::memory_order_release);
  int width = canvas_->get_allocated_width();
  int height = canvas_->get_allocated_height();
  callback(cr, width, height);
  return true;
}

bool Visualization::OnTimeout() {
  if (shutdown_initiated_) {
    assert(application_);
    application_->quit();
    return false;
  }
  for (size_t id = 0; id < windows_.size(); ++id) {
    Window& win = windows_[id];
    while (windows_mutex_.test_and_set(std::memory_order_acquire))
      ;
    if (win.IsEmpty() && win.IsInitialized()) {
      win.CloseWindow(application_);
      assert(win.IsEmpty() && !win.IsInitialized());
    }
    if (!win.IsEmpty() && !win.IsInitialized()) {
      win.OpenWindow(application_);
      assert(!win.IsEmpty() && win.IsInitialized());
    }
    if (win.IsInitialized() && win.queried_callback_) {
      win.canvas_->queue_draw();
    }
    windows_mutex_.clear(std::memory_order_release);
  }
  return true;
}

void Visualization::RunMainLoop() {
  log_->LogMessage("Visualization mainloop starting up...");
  // int argc = cli_.argc();
  // char** argv = (char**)cli_.argv();
  application_ = Gtk::Application::create(Glib::ustring(kGTKApplicationID));
  log_->LogMessage("Setting mainloop timer to ", activations_per_second_,
                   " fps");
  Glib::signal_timeout().connect(sigc::mem_fun(this, &Visualization::OnTimeout),
                                 1000 / (unsigned)activations_per_second_);
  Gtk::Window dummy_window;
  application_->run(dummy_window);
  log_->LogMessage("Visualization mainloop stopping...");
}

void Visualization::PrintHelp() {
  Log::Print("ZAMT Visualization Module using GTK");
  Log::Print(
      " -fpsNum        Sets target rendering FPS to Num per seconds instead "
      "of the default 25.");
}

std::atomic<bool> Visualization::shutdown_initiated_(false);

}  // namespace zamt
