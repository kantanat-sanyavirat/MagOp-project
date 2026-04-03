# Require CMake version 3.16 (Better for Qt6)
cmake_minimum_required(VERSION 3.16)
project(MagOp LANGUAGES CXX)

# Set Output Directory to root
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

# Require C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt framework settings
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 1. Find Qt6
find_package(Qt6 COMPONENTS Widgets Core Gui REQUIRED)

# 2. Find OpenCV
find_package(OpenCV REQUIRED)

# 3. ONNX Runtime — Auto-detect platform
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm")
    # Check both possible ARM paths
    if(EXISTS "${CMAKE_SOURCE_DIR}/lib/onnxruntime")
        set(ONNX_ROOT "${CMAKE_SOURCE_DIR}/lib/onnxruntime")
    else()
        set(ONNX_ROOT "${CMAKE_SOURCE_DIR}/lib/arm64/onnxruntime")
    endif()
    message(STATUS "Platform: ARM64 — using ${ONNX_ROOT}")
else()
    set(ONNX_ROOT "${CMAKE_SOURCE_DIR}/lib/x86_64/onnxruntime-linux-x64-1.20.1")
    message(STATUS "Platform: x86_64 — using lib/x86_64/onnxruntime-linux-x64-1.20.1")
endif()

find_library(ONNX_LIB NAMES onnxruntime PATHS ${ONNX_ROOT}/lib NO_DEFAULT_PATH)
if(NOT ONNX_LIB)
    message(WARNING "ONNX Runtime library not found in ${ONNX_ROOT}/lib")
else()
    message(STATUS "Found ONNX Runtime: ${ONNX_LIB}")
endif()

# Include Headers
include_directories(include)
include_directories(ui)
include_directories(${ONNX_ROOT}/include)

# 4. Source files
set(SOURCES
    main.cpp
    src/camera_handler.cpp
    src/ai_processing.cpp
    src/backend_controller.cpp
    ui/mainwindow.cpp
)

set(HEADERS
    include/camera_handler.h
    include/ai_processing.h
    include/backend_controller.h
    ui/mainwindow.h
)

# 5. Executable target
add_executable(MagOp_app ${SOURCES} ${HEADERS})

# 6. Link libraries
target_link_libraries(MagOp_app PRIVATE
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
    ${OpenCV_LIBS}
    ${ONNX_LIB}
)

# 7. RPATH
if(UNIX AND NOT APPLE)
    set_target_properties(MagOp_app PROPERTIES
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "${ONNX_ROOT}/lib"
        INSTALL_RPATH_USE_LINK_PATH TRUE
    )
endif()

# 8. Windows — copy DLL
if(WIN32)
    add_custom_command(TARGET MagOp_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ONNX_ROOT}/lib/onnxruntime.dll"
        $<TARGET_FILE_DIR:MagOp_app>)
endif()

# 9. test_inference — standalone AI test tool
if(EXISTS "${CMAKE_SOURCE_DIR}/models/yolo/yolo.onnx")
    add_executable(test_inference tools/test_inference.cpp)
    target_include_directories(test_inference PRIVATE
        ${ONNX_ROOT}/include
        ${OpenCV_INCLUDE_DIRS}
    )
    target_link_libraries(test_inference PRIVATE
        ${OpenCV_LIBS}
        ${ONNX_LIB}
    )
    if(UNIX AND NOT APPLE)
        set_target_properties(test_inference PROPERTIES
            BUILD_WITH_INSTALL_RPATH TRUE
            INSTALL_RPATH "${ONNX_ROOT}/lib"
            INSTALL_RPATH_USE_LINK_PATH TRUE
        )
    endif()
    message(STATUS "test_inference: enabled")
else()
    message(STATUS "test_inference: skipped — models/yolo/yolo.onnx not found")
endif()