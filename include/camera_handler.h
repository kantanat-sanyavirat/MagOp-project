#ifndef CAMERAHANDLER_H
#define CAMERAHANDLER_H

#include <QObject>
#include <QTimer>
#include <QDebug>
#include <QImage>
#include <QDir>
#include <QDateTime>
#include <opencv2/opencv.hpp>

class CameraHandler : public QObject
{
    Q_OBJECT

public:
    explicit CameraHandler(QObject *parent = nullptr);
    ~CameraHandler();

    // Open camera at given index and start the frame loop
    void startCamera(int camIndex = 0);

    // Stop the frame loop and release the camera
    void stopCamera();

    // Returns the most recent frame (may be empty if camera is disconnected)
    cv::Mat getCurrentFrame() const;

    // Save a raw backup image to disk (for debugging)
    void saveCapturedImage(const cv::Mat& frame);

signals:
    // Emitted every tick — empty Mat means camera is not available
    void frameReady(cv::Mat frame);

private slots:
    // Timer callback — grabs frame or attempts reconnect
    void loop();

private:
    cv::VideoCapture cap;
    cv::Mat          currentFrame;
    QTimer          *timer;
    int              m_camIndex;
};

#endif // CAMERAHANDLER_H