# All target apps are configured here (what modules they are built of...)

set(modules
  core
  audio
  gui
)
AddExe(zamtdemo "${modules}")

