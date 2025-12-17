#include <QApplication>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>
#include "camera_handler.h"

// --- ฟังก์ชันเช็คระบบ ---
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
    // ใช้ QApplication เพื่อรองรับ GUI (แก้ปัญหา GLib Critical Error)
    QApplication app(argc, argv);

    // เช็คระบบก่อนเริ่ม (ถ้าไม่ผ่านให้ปิดโปรแกรม)
    // if (performSystemCheck() != 0) return -1; 

    std::cout << "System Ready! Opening Camera..." << std::endl;
    std::cout << "Controls: 's' = Save Image, 'q' = Quit" << std::endl;

    CameraHandler camera;

    // เชื่อมต่อสัญญาณจากกล้อง
    QObject::connect(&camera, &CameraHandler::frameReady, [&](cv::Mat frame){
        cv::imshow("MagOp Camera", frame);
        
        // รอรับปุ่มกด 1ms
        char key = (char)cv::waitKey(1);

        if (key == 'q') {
            app.quit(); // สั่งปิดโปรแกรม
        } 
        else if (key == 's') {
            camera.saveCapturedImage(frame); // สั่งเซฟรูป
        }
    });

    camera.startCamera(0);

    // เข้าสู่ Loop การทำงาน
    int ret = app.exec();
    
    // คืนค่าหน่วยความจำเมื่อจบการทำงาน
    cv::destroyAllWindows();
    std::cout << "Application exited successfully." << std::endl;
    
    return ret;
}