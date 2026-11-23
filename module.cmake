# The module builds only when AWARE_BOTS is on (cmake -DAWARE_BOTS=ON), so SkyFire builds stay unaffected while the
# port is in progress.
if(NOT AWARE_BOTS)
  set(MODULE_ENABLED FALSE)
else()
  # Every source of this module sees the compatibility names first (src/compat/BotCompat.h). Set per source file, not
  # with add_compile_options: all modules share one "modules" library, and other modules must not get these names.
  file(GLOB_RECURSE AWARE_BOTS_SOURCES ${MODULE_PATH}/src/*.cpp)
  # Two list items: CMake does not expand "SHELL:" in source file properties.
  set_source_files_properties(${AWARE_BOTS_SOURCES} PROPERTIES
    COMPILE_OPTIONS "-include;${MODULE_PATH}/src/compat/BotCompat.h")
endif()
