cmake_minimum_required(VERSION 3.21)

# The client can also be configured directly with cmake -S client -B <build-dir>.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(littlechat_client VERSION 1.0.0 LANGUAGES CXX)
    include(CTest)
endif()

if(DEFINED CMAKE_CXX_STANDARD AND CMAKE_CXX_STANDARD GREATER 17)
    message(FATAL_ERROR "littlechat supports C++17 at most; do not raise the language standard.")
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
get_filename_component(LITTLECHAT_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(client_src "${LITTLECHAT_REPO_ROOT}/src")

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Svg)

add_library(littlechat_core STATIC
    ${client_src}/core/models/message.h
    ${client_src}/core/models/conversation.h
    ${client_src}/core/storage/chatstore.cpp ${client_src}/core/storage/chatstore.h
    ${client_src}/core/controllers/chatcontroller.cpp ${client_src}/core/controllers/chatcontroller.h)
target_link_libraries(littlechat_core PUBLIC Qt6::Core)
target_include_directories(littlechat_core PUBLIC "${client_src}")

add_library(littlechat_ui STATIC
    ${client_src}/ui/windows/mainwindow.cpp ${client_src}/ui/windows/mainwindow.h
    ${client_src}/ui/widgets/captionbutton.cpp ${client_src}/ui/widgets/captionbutton.h
    ${client_src}/ui/widgets/conversationdelegate.cpp ${client_src}/ui/widgets/conversationdelegate.h
    ${client_src}/ui/widgets/messagebubble.cpp ${client_src}/ui/widgets/messagebubble.h
    ${client_src}/ui/graphics/icons.cpp ${client_src}/ui/graphics/icons.h
    ${client_src}/ui/graphics/resources.cpp ${client_src}/ui/graphics/resources.h
    ${client_src}/platform/window/framelesswindow.cpp ${client_src}/platform/window/framelesswindow.h
    ${LITTLECHAT_REPO_ROOT}/resources/resources.qrc)
target_link_libraries(littlechat_ui PUBLIC littlechat_core Qt6::Widgets Qt6::Svg)
target_include_directories(littlechat_ui PUBLIC "${client_src}")
if(WIN32)
    target_link_libraries(littlechat_ui PRIVATE dwmapi user32)
    target_compile_definitions(littlechat_ui PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()
foreach(target littlechat_core littlechat_ui)
    if(MSVC)
        target_compile_options(${target} PUBLIC /utf-8 /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()

qt_add_executable(littlechat WIN32 ${client_src}/app/main.cpp)
target_link_libraries(littlechat PRIVATE littlechat_ui)
set_target_properties(littlechat PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
install(TARGETS littlechat RUNTIME DESTINATION bin)
qt_generate_deploy_app_script(TARGET littlechat OUTPUT_SCRIPT deploy_script NO_UNSUPPORTED_PLATFORM_ERROR)
install(SCRIPT ${deploy_script})

if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    foreach(suite chatstore mainwindow window_win)
        qt_add_executable(tst_${suite} "${LITTLECHAT_REPO_ROOT}/tests/tst_${suite}.cpp")
        set_target_properties(tst_${suite} PROPERTIES
            WIN32_EXECUTABLE FALSE
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
        target_link_libraries(tst_${suite} PRIVATE Qt6::Test)
        if(suite STREQUAL "chatstore")
            target_link_libraries(tst_${suite} PRIVATE littlechat_core)
        else()
            target_link_libraries(tst_${suite} PRIVATE littlechat_ui)
        endif()
        add_test(NAME ${suite} COMMAND tst_${suite})
    endforeach()
    set_tests_properties(mainwindow PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    if(WIN32)
        set_tests_properties(window_win PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=windows")
    else()
        set_tests_properties(window_win PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endif()
endif()
