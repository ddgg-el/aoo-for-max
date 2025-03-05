
message("*** Configuring aoo_max_common library ***\n")

include(${MAX_SDK_DIR}/script/max-pretarget.cmake)

## Max externals minimum compile 10.11 Aoo minimum compile 10.13
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum OS X deployment version" FORCE)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# HACK: check with Christof
# Ensure symbols are hidden by default to match the aoo library
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

# Include directories
include_directories(
    "${MAX_SDK_INCLUDES}"
    "${MAX_SDK_MSP_INCLUDES}"
	"../include"
)

# Source files for the library
file(GLOB PROJECT_SRC
     "*.h"
     "*.cpp"
)

# Create the static library
add_library(
    aoo_max_common 
    STATIC
    ${PROJECT_SRC}
)

# Public headers for the library
target_include_directories(aoo_max_common PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(aoo_max_common PUBLIC aoo aoo_common aoo_binary)

