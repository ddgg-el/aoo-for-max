cmake_minimum_required(VERSION 3.15)

string(REGEX REPLACE "(.*)/" "" THIS_FOLDER_NAME "${CMAKE_CURRENT_SOURCE_DIR}")
project(${THIS_FOLDER_NAME})

message(STATUS "Project: ${PROJECT_NAME}")

################################################################

# Max-SDK directories and settings
set(MAX_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE STRING "Max SDK Base folder")
set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")
set(C74_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/externals")

message(STATUS "Max include directory: ${MAX_INCLUDE_DIR}")
message(STATUS "OSX Deployment Target: ${CMAKE_OSX_DEPLOYMENT_TARGET}\n")

################################################################

# Aoo directories and settings
option(AOO_USE_OPUS "use Opus codec" OFF)
set(AOO_SAMPLE_SIZE 64 CACHE STRING "Audio sample size" )

add_subdirectory(aoo EXCLUDE_FROM_ALL)
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/source/include )

################################################################

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

# Generate a project for every folder in the "source/" folder
SUBDIRLIST(PROJECT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/source)
foreach (project_dir ${PROJECT_DIRS})
	set(project_path ${CMAKE_CURRENT_SOURCE_DIR}/source/${project_dir})
  if (EXISTS "${project_path}/CMakeLists.txt")
   # Non compilare aoo_template
    if(NOT "${project_dir}" STREQUAL "aoo_template_tilde" AND NOT "${project_dir}" STREQUAL "aoo_template")
      string(REGEX REPLACE "_tilde" "~" EXTERN_PROJECT_NAME_DEFAULT "${project_dir}")
      message("*** Generating: ${EXTERN_PROJECT_NAME_DEFAULT} ***")
      add_subdirectory(${project_path})
    endif ()
  endif ()
endforeach ()

message("\n********************************\n")


################################################################

# target_link_libraries(${PROJECT_NAME} PRIVATE aoo aoo_common aoo_binary)
# target_link_libraries(my_project PRIVATE Aoo::aoo)