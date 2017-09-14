#configuration
option(CLIPPER_USE_ARENA       "enable ArenaMemoryManager for ClipperLib" ON)
set(CLIPPER_BASE_DIR           "" CACHE PATH "tweaked clipper/iopaths base dir")
set(INITIAL_ARENA_SIZE  "52428800" CACHE STRING "Initial arena size for ArenaMemoryManager")
set(BIGCHUNK_ARENA_SIZE "2097152"  CACHE STRING "Bigchunk size for ArenaMemoryManager") #1-5 MB: 1048576 2097152 3145728 4194304 5242880
set(BOOST_ROOT_PATH            "" CACHE PATH "path prefix to boost library")
option(Boost_USE_STATIC_LIBS   "Use the static or the dynamic version of the Boost libraries" OFF)
option(ENABLE_SLICER_LOGGING   "enable slicer debug output to file" ON)
