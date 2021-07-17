# This was buggy with GCC[<6.0] with nullptr_t
#   -Wno-unused-but-set-parameter
set(CMAKE_CXX_FLAGS
      "-Wall -Wextra -Werror -Wdeprecated -Wextra-semi -Wfloat-equal -Wshadow -Wnon-virtual-dtor -Wsuggest-override -Wno-unused-parameter -Wno-shadow"
    CACHE STRING "C++ Flags")
