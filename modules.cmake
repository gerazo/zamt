# All available modules are enumerated here (they are compiled and tested)

set(zamt_modules
  core
  liveaudio_pulse
  vis_gtk
)


# Find 3rd party libs

find_package(Threads)
find_package(PkgConfig)

# If missing: sudo apt install libpulse-dev
list(FIND zamt_modules liveaudio_pulse liveaudio_pulse_on)
if(liveaudio_pulse_on GREATER -1)
  find_package(PulseAudio REQUIRED CONFIG)
endif()

# If missing: sudo apt install libgtkmm-3.0-dev
list(FIND zamt_modules vis_gtk vis_gtk_on)
if(vis_gtk_on GREATER -1)
  pkg_check_modules(GTKMM REQUIRED gtkmm-3.0)
endif()

