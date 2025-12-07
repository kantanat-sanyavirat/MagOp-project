//test file for checking OpenCV ONNX Runtime integration and Cmake setup
#include <iostream>
#include <vector>

// เรียกใช้ OpenCV
#include <opencv2/opencv.hpp>

// เรียกใช้ ONNX Runtime
#include <onnxruntime_cxx_api.h>

int main() {
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "MagOp System: Starting Initialization..." << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    // --- 1. ทดสอบ OpenCV ---
    std::cout << "[Check 1] OpenCV Version: " << CV_VERSION << std::endl;
    try {
        cv::Mat test_image = cv::Mat::zeros(100, 100, CV_8UC3);
        std::cout << "   -> OpenCV Mat created successfully." << std::endl;
    } catch (...) {
        std::cerr << "   -> Error initializing OpenCV!" << std::endl;
        return -1;
    }

    // --- 2. ทดสอบ ONNX Runtime ---
    std::cout << "[Check 2] ONNX Runtime..." << std::endl;
    try {
        Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "MagOp_Test");
        Ort::SessionOptions session_options;
        std::cout << "   -> ONNX Environment created successfully." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "   -> Error initializing ONNX Runtime: " << e.what() << std::endl;
        return -1;
    }

    std::cout << "----------------------------------------" << std::endl;
    std::cout << "SUCCESS: System is ready for development!" << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    return 0;
}