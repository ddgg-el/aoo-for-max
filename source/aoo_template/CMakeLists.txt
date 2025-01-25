include(${CMAKE_CURRENT_SOURCE_DIR}/../../max-sdk-base/script/max-pretarget.cmake)

## Max externals minimum compile 10.11 Aoo minimum compile 10.13
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum OS X deployment version" FORCE)
#############################################################
# MAX EXTERNAL
#############################################################

include_directories( 
	"${MAX_SDK_INCLUDES}"
	"${MAX_SDK_MSP_INCLUDES}"
	"${MAX_SDK_JIT_INCLUDES}"
)

file(GLOB PROJECT_SRC
     "*.h"
	 "*.c"
     "*.cpp"
	 "../common/*.cpp"
)
add_library( 
	${PROJECT_NAME} 
	MODULE
	${PROJECT_SRC}
)

target_link_libraries(${PROJECT_NAME} PRIVATE aoo aoo_common aoo_binary)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../max-sdk-base/script/max-posttarget.cmake)
