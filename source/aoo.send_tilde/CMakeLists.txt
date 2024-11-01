include(${MAX_INCLUDE_DIR}/max-sdk-base/script/max-pretarget.cmake)

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
)
add_library(
	${PROJECT_NAME} 
	MODULE
	${PROJECT_SRC}
)

target_link_libraries(${PROJECT_NAME} PRIVATE aoo aoo_common aoo_binary)

include(${MAX_INCLUDE_DIR}/max-sdk-base/script/max-posttarget.cmake)
