#include "zamt/vis_gtk/Visualization.h"

#include "zamt/core/Core.h"
#include "zamt/core/Log.h"
#include "zamt/core/ModuleCenter.h"

namespace zamt {

const char* Visualization::kModuleLabel = "vis_gtk";

Visualization::Visualization(int argc, const char* const* argv)
    : cli_(argc, argv) {
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
  log_->LogMessage("Stopping...");
}

void Visualization::RunMainLoop() {
  log_->LogMessage("Visualization mainloop starting up...");

  log_->LogMessage("Visualization mainloop stopping...");
}

}  // namespace zamt
