include(${MAX_SDK_DIR}/script/max-pretarget.cmake)

## Max externals minimum compile 10.11 Aoo minimum compile 10.13
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum OS X deployment version" FORCE)
#############################################################
# MAX EXTERNAL
#############################################################

include_directories( 
	"${MAX_SDK_INCLUDES}"
	"${MAX_SDK_MSP_INCLUDES}"
	"../include"
)

file(GLOB PROJECT_SRC
     "*.h"
	 "*.c"
     "*.cpp"
)

add_library(
	${PROJECT_NAME} 
	MODULE
	${PROJECT_SRC}
)

target_link_libraries(${PROJECT_NAME} PRIVATE aoo_max_common)

include(${MAX_SDK_DIR}/script/max-posttarget.cmake)
message("\n")
