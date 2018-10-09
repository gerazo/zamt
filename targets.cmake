# All target apps are configured here (what modules they are built of...)

set(modules
  core
  liveaudio_pulse
  vis_gtk
)
AddExe(zamtdemo "${modules}")

