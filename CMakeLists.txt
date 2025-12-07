#Require CMake version 3.10 or higher
cmake_minimum_required(VERSION 3.10)
project(MagOp LANGUAGES CXX) 
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

#Require C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#QT framework setting
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ค้นหา Qt6 
find_package(Qt6 COMPONENTS Widgets Core REQUIRED)

#find OpenCV
find_package(OpenCV REQUIRED)

#ONNX Runtime position
set(ONNX_ROOT "${CMAKE_SOURCE_DIR}/lib/onnxruntime")
include_directories(${ONNX_ROOT}/include)

# find_library in another OS automatically
find_library(ONNX_LIB 
    NAMES onnxruntime 
    PATHS ${ONNX_ROOT}/lib 
    NO_DEFAULT_PATH
)

if(NOT ONNX_LIB)
    message(WARNING "ONNX Runtime library not found in ${ONNX_ROOT}/lib")
endif()

#Headers position
include_directories(include)

# Source files
file(GLOB SOURCES "src/*.cpp")
file(GLOB HEADERS "include/*.h")

#Executable target
add_executable(MagOp_app ${SOURCES} ${HEADERS})

#Link libraries
target_link_libraries(MagOp_app PRIVATE
    Qt6::Widgets      
    Qt6::Core         
    ${OpenCV_LIBS}    
    ${ONNX_LIB}       
)

#Copy ONNX Runtime DLLs to output directory on Windows
if(WIN32)
    add_custom_command(TARGET MagOp_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ONNX_ROOT}/lib/onnxruntime.dll"
        $<TARGET_FILE_DIR:MagOp_app>)
endif()