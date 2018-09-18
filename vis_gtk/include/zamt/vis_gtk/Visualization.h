#ifndef ZAMT_VIS_GTK_VISUALIZATION_H_
#define ZAMT_VIS_GTK_VISUALIZATION_H_

/// Visualization module to render various states into a dedicated window.
/**
 * This module uses gtkmm to start a main loop using its own rendering thread.
 * Clients can get a new window for drawing using cairomm library.
 * Visualization takes care of Gtk::Application and Gtk::Window creation.
 * One can schedule rendering on the next frame by asking for a callback to the
 * given rendering code on the rendering thread.
 */

#include "zamt/core/CLIParameters.h"
#include "zamt/core/Module.h"

#include <atomic>
#include <deque>
#include <functional>
#include <memory>
#include <thread>

#include <gtkmm/application.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/window.h>

namespace zamt {

class Log;

class Visualization : public Module {
 public:
  const static char* kModuleLabel;
  const static char* kApplicationID;
  const static char* kActivationsPerSecondParamStr;
  const static int kActivationsPerSecond = 25;

  using RenderCallback =
      std::function<void(const Cairo::RefPtr<Cairo::Context>&)>;

  Visualization(int argc, const char* const* argv);
  ~Visualization();
  void Initialize(const ModuleCenter* mc);
  void Shutdown(int exit_code);

  /// Open a new window with the given attributes and return its id.
  void OpenWindow(const char* window_title, int width, int height,
                  int& window_id);

  /// Close an opened window.
  void CloseWindow(int window_id);

  /// Ask for a callback from the rendering thread on the next frame.
  /// There can be only one query in the queue for one window. A second query
  /// removes the previous one if the previous render have not started yet.
  void QueryRender(int window_id, RenderCallback render_callback);

 private:
  struct Window {
    bool IsEmpty() { return !window_title_; }
    bool IsInitialized() { return (bool)canvas_; }
    void OpenWindow(Glib::RefPtr<Gtk::Application>& application);
    void CloseWindow(Glib::RefPtr<Gtk::Application>& application);
    bool OnDraw(const Cairo::RefPtr<Cairo::Context>& cr);
    const char* window_title_ = nullptr;
    int width_;
    int height_;
    std::unique_ptr<Gtk::Window> window_;
    std::unique_ptr<Gtk::DrawingArea> canvas_;
    RenderCallback queried_callback_;
    std::atomic_flag callback_mutex_ = ATOMIC_FLAG_INIT;
  };

  bool OnTimeout();
  void RunMainLoop();
  void PrintHelp();

  CLIParameters cli_;
  const ModuleCenter* mc_ = nullptr;
  std::unique_ptr<Log> log_;
  std::unique_ptr<std::thread> visualization_loop_;
  int activations_per_second_;
  std::atomic<bool> shutdown_initiated_;
  Glib::RefPtr<Gtk::Application> application_;
  std::deque<Window> windows_;
  std::atomic_flag windows_mutex_ = ATOMIC_FLAG_INIT;
};

}  // namespace zamt

#endif  // ZAMT_VIS_GTK_VISUALIZATION_H_
