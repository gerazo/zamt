#ifndef ZAMT_VIS_GTK_VISUALIZATION_H_
#define ZAMT_VIS_GTK_VISUALIZATION_H_

/// Visualization module to render various states into a dedicated window.
/**
 * This module uses gtkmm to start a gtk main loop using its own thread for UI.
 * Clients can register and get a new window for drawing using cairo library.
 * Visualization takes care of Gtk::Application and Gtk::Window creation and
 * calls back Scheduler to do UI tasks on the one and only UI thread, so
 * clients should also register for the appropriate data source in Scheduler as
 * a UI task.
 */

#include "zamt/core/CLIParameters.h"
#include "zamt/core/Module.h"

#include <memory>
#include <thread>

namespace zamt {

class Log;

class Visualization : public Module {
 public:
  const static char* kModuleLabel;

  Visualization(int argc, const char* const* argv);
  ~Visualization();
  void Initialize(const ModuleCenter* mc);
  void Shutdown(int /*exit_code*/);

 private:
  void RunMainLoop();

  const ModuleCenter* mc_ = nullptr;
  CLIParameters cli_;
  std::unique_ptr<Log> log_;
  std::unique_ptr<std::thread> visualization_loop_;
};

}  // namespace zamt

#endif  // ZAMT_VIS_GTK_VISUALIZATION_H_
