#include "zamt/vis_gtk/Visualization.h"

#include "zamt/core/Core.h"
#include "zamt/core/Log.h"
#include "zamt/core/ModuleCenter.h"

#include <algorithm>
#include <cassert>

#include <glibmm/main.h>

namespace zamt {

const char* Visualization::kModuleLabel = "vis_gtk";
const char* Visualization::kApplicationID = "zamt";

Visualization::Visualization(int argc, const char* const* argv)
    : cli_(argc, argv), shutdown_initiated_(false) {
  log_.reset(new Log(kModuleLabel, cli_));
  if (cli_.HasParam(Core::kHelpParamStr)) return;
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
  size_t id = 0;
  while (id < windows_.size()) {
    if (!windows_[id].window_) break;
    id++;
  }
  if (id >= windows_.size()) windows_.emplace_back();
  assert(id < windows_.size() && !windows_[id].window_ &&
         !windows_[id].canvas_);
  auto& win = windows_[id].window_;
  win.reset(new Gtk::Window());
  win->set_title(window_title);
  win->resize(width, height);
  auto& area = windows_[id].canvas_;
  area.reset(new Gtk::DrawingArea());
  win->add(*area);
  win->set_application(application_);
  win->show();
  area->show();
  area->signal_draw().connect(
      sigc::mem_fun(&windows_[id], &Visualization::Window::OnDraw), false);
  window_id = (int)id;
}

void Visualization::CloseWindow(int window_id) {
  size_t id = (size_t)window_id;
  assert(id < windows_.size() && windows_[id].window_ && windows_[id].canvas_);
  windows_[id].window_->unset_application();
  windows_[id].window_.reset(nullptr);
  windows_[id].canvas_.reset(nullptr);
  windows_[id].queried_callback_ = nullptr;
}

void Visualization::QueryRender(int window_id, RenderCallback render_callback) {
  assert(render_callback);
  size_t id = (size_t)window_id;
  assert(id < windows_.size() && windows_[id].window_ && windows_[id].canvas_);
  while (windows_[id].callback_mutex_.test_and_set(std::memory_order_acquire))
    ;
  windows_[id].queried_callback_ = render_callback;
  windows_[id].callback_mutex_.clear(std::memory_order_release);
}

bool Visualization::Window::OnDraw(const Cairo::RefPtr<Cairo::Context>& cr) {
  if (!window_ || !canvas_ || !queried_callback_) return true;
  while (callback_mutex_.test_and_set(std::memory_order_acquire))
    ;
  RenderCallback callback = queried_callback_;
  queried_callback_ = nullptr;
  callback_mutex_.clear(std::memory_order_release);
  callback(cr);
  return true;
}

bool Visualization::OnTimeout() {
  if (shutdown_initiated_) {
    assert(application_);
    application_->quit();
    return false;
  }
  for (size_t id = 0; id < windows_.size(); ++id) {
    if (windows_[id].window_ && windows_[id].queried_callback_) {
      windows_[id].canvas_->queue_draw();
    }
  }
  return true;
}

void Visualization::RunMainLoop() {
  log_->LogMessage("Visualization mainloop starting up...");
  int argc = cli_.argc();
  char** argv = (char**)cli_.argv();
  application_ = Gtk::Application::create(argc, argv, kApplicationID);
  Glib::signal_timeout().connect(sigc::mem_fun(this, &Visualization::OnTimeout),
                                 1000 / kActivationsPerSecond);
  application_->run();
  log_->LogMessage("Visualization mainloop stopping...");
}

}  // namespace zamt
