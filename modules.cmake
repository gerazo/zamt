# All available modules are enumerated here (they are compiled and tested)

set(zamt_modules
  core
  audio
  gui
)


# Find 3rd party libs

find_package(Threads)
find_package(PkgConfig)

# If missing: sudo apt install libpulse-dev
list(FIND zamt_modules audio audio_on)
if(audio_on GREATER -1)
  find_package(PulseAudio 8 REQUIRED CONFIG)
endif()

# If missing: sudo apt install libgtkmm-3.0-dev
list(FIND zamt_modules gui gui_on)
if(gui_on GREATER -1)
  pkg_check_modules(GTKMM REQUIRED gtkmm-3.0)
endif()

