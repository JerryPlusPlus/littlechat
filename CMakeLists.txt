# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8...4.4)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("Chat")

set(CMAKE_CXX_STANDARD 17 )
set(CMAKE_CXX_STANDARD_REQUIRED YES )
set(CMAKE_CXX_EXTENSIONS NO )
# Include sub-projects.
#add_subdirectory ("client_win")
add_subdirectory("myNet")


#=======================================
#=======================================


# add_executable(json_t main.cpp )
# 
# target_include_directories(json_t PRIVATE "thirdparty/json-develop/include")

#=======================================
#=======================================