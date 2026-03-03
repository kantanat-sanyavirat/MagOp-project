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

    void startCamera(int camIndex = 0);
    void stopCamera();
    cv::Mat getCurrentFrame() const;
    
    void saveCapturedImage(const cv::Mat& frame); 

signals:
    void frameReady(cv::Mat frame);

private slots:
    void loop();

private:
    cv::VideoCapture cap;
    cv::Mat currentFrame;
    QTimer *timer;
    int m_camIndex;
};

#endif // CAMERAHANDLER_H