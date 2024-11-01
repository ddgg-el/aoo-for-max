cmake_minimum_required(VERSION 3.15)

string(REGEX REPLACE "(.*)/" "" THIS_FOLDER_NAME "${CMAKE_CURRENT_SOURCE_DIR}")
project(${THIS_FOLDER_NAME})

# Max directories
set(MAX_INCLUDE_DIR "/Users/davide/Documents/src/max-sdk/source/")
set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")

option(AOO_USE_OPUS "use Opus codec" OFF)
option(AOO_SAMPLE_SIZE "Audio sample size" 64)

message(STATUS "Max include directory: ${MAX_INCLUDE_DIR}")

add_subdirectory(aoo EXCLUDE_FROM_ALL)

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/source/include )


MACRO(SUBDIRLIST result curdir)
  FILE(GLOB children RELATIVE ${curdir} ${curdir}/*)
  SET(dirlist "")
  FOREACH(child ${children})
    IF(IS_DIRECTORY ${curdir}/${child})
        LIST(APPEND dirlist ${child})
    ENDIF()
  ENDFOREACH()
  SET(${result} ${dirlist})
ENDMACRO()

# Generate a project for every folder in the "source/category" folder
SUBDIRLIST(PROJECT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/source)
foreach (project_dir ${PROJECT_DIRS})
	set(project_path ${CMAKE_CURRENT_SOURCE_DIR}/source/${project_dir})
  if (EXISTS "${project_path}/CMakeLists.txt")
	message("Generating: ${project_path}")
	add_subdirectory(${project_path})
  endif ()
endforeach ()




# target_link_libraries(${PROJECT_NAME} PRIVATE aoo aoo_common aoo_binary)
# target_link_libraries(my_project PRIVATE Aoo::aoo)