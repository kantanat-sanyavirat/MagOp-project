#include <QCoreApplication>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>
#include <csignal> // สำหรับการดักจับ Signal ctrl+c
#include "camera_handler.h"


// ตัวแปร Global pointer เพื่อให้ Signal Handler เรียกใช้ได้
QCoreApplication* appPtr = nullptr;

// ฟังก์ชันที่จะทำงานเมื่อกด Ctrl+C
void handleSignal(int signal) {
    if (signal == SIGINT) {
        std::cout << "\nWarning: Ctrl+C pressed! Shutting down gracefully..." << std::endl;
        if (appPtr) {
            appPtr->quit(); // สั่งให้ Qt ปิดโปรแกรมอย่างถูกต้อง
        }
    }
}

// --- ฟังก์ชันเช็คระบบ  ---
int performSystemCheck() {
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "MagOp System: Starting Initialization..." << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    bool success = true;

    // 1. เช็ค OpenCV
    std::cout << "[Check 1] OpenCV Version: " << CV_VERSION << " ... ";
    try {
        cv::Mat test = cv::Mat::zeros(10, 10, CV_8UC1);
        std::cout << "PASSED" << std::endl;
    } catch (...) {
        std::cerr << "FAILED" << std::endl;
        success = false;
    }

    // 2. เช็ค ONNX Runtime
    std::cout << "[Check 2] ONNX Runtime ... ";
    try {
        // ใช้ Level Error จะได้ไม่รกหน้าจอ
        Ort::Env env(ORT_LOGGING_LEVEL_ERROR, "MagOp_Check");
        Ort::SessionOptions options;
        std::cout << "PASSED" << std::endl;
    } catch (std::exception &e) {
        std::cerr << "FAILED (" << e.what() << ")" << std::endl;
        success = false;
    }

    std::cout << "----------------------------------------" << std::endl;
    return success ? 0 : -1;
}

// --- ฟังก์ชันหลัก ---
int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    appPtr = &app; 

 
    std::signal(SIGINT, handleSignal);

     if (performSystemCheck() != 0) return -1;

    std::cout << "System Ready! Opening Camera..." << std::endl;
    std::cout << "Controls: 's' = Save Image, 'q' = Quit" << std::endl;

    CameraHandler camera;

    QObject::connect(&camera, &CameraHandler::frameReady, [&](cv::Mat frame){
        cv::imshow("MagOp Camera", frame);
        char key = (char)cv::waitKey(1);

        if (key == 'q') {
            app.quit();
        } 
        else if (key == 's') {
            camera.saveCapturedImage(frame);
        }
    });

    camera.startCamera(0);


    int ret = app.exec();
    
    cv::destroyAllWindows();
    std::cout << "Application exited successfully." << std::endl;
    
    return ret;
}