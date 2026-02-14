# Require CMake version 3.16 (Better for Qt6)
cmake_minimum_required(VERSION 3.16)
project(MagOp LANGUAGES CXX) 

# Set Output Directory to root (same as source)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

# Require C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt framework settings
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 1. Find Qt6
# (หมายเหตุ: ถ้าบน Pi หา Qt6 ไม่เจอ ให้เปลี่ยนเลข 6 เป็น 5 ตรงนี้และข้างล่างครับ)
find_package(Qt6 COMPONENTS Widgets Core Gui REQUIRED)

# 2. Find OpenCV
find_package(OpenCV REQUIRED)

# 3. ONNX Runtime Configuration
set(ONNX_ROOT "${CMAKE_SOURCE_DIR}/lib/onnxruntime")

# Find ONNX Library
find_library(ONNX_LIB 
    NAMES onnxruntime 
    PATHS ${ONNX_ROOT}/lib 
    NO_DEFAULT_PATH
)

if(NOT ONNX_LIB)
    message(WARNING "ONNX Runtime library not found in ${ONNX_ROOT}/lib")
else()
    message(STATUS "Found ONNX Runtime: ${ONNX_LIB}")
endif()

# Include Headers
include_directories(include)             # Local headers
include_directories(${ONNX_ROOT}/include) # ONNX headers

# 4. Source files setup (MANUAL LIST)
# เปลี่ยนจาก GLOB มาเป็นการระบุชื่อไฟล์ เพื่อเลือกปิด backend_controller ได้
set(SOURCES
    main.cpp
    src/camera_handler.cpp
    src/ai_processing.cpp
    # src/backend_controller.cpp  <-- ปิดไว้ชั่วคราว (Uncomment เมื่อทำ UI จริง)
)

set(HEADERS
    include/camera_handler.h
    include/ai_processing.h
    # include/backend_controller.h <-- ปิดไว้ชั่วคราว
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

# 7. [LINUX/PI FIX] RPATH Settings
if(UNIX AND NOT APPLE)
    set_target_properties(MagOp_app PROPERTIES
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/lib/onnxruntime/lib"
        INSTALL_RPATH_USE_LINK_PATH TRUE
    )
endif()

# 8. [WINDOWS FIX] Copy DLLs
if(WIN32)
    add_custom_command(TARGET MagOp_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ONNX_ROOT}/lib/onnxruntime.dll"
        $<TARGET_FILE_DIR:MagOp_app>)
endif()