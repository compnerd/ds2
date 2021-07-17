# Windows headers may require these to be disabled.
#  -Wno-missing-braces
#  -Wno-missing-field-initializers
#  -Wno-tautological-compare
set(CMAKE_CXX_FLAGS
      "-Wall -Wextra -Werror -Wcomma -Wdeprecated -Wextra-semi -Wfloat-equal -Wshadow -Winconsistent-missing-override -Wnon-virtual-dtor -Wno-unused-parameter"
    CACHE STRING "C++ Flags")
