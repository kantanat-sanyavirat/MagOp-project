#ifndef AI_PROCESSING_H
#define AI_PROCESSING_H

#include <QObject>
#include <QMetaType>
#include <QMutex>
#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>
#include <vector>
#include <deque>
#include <string>
#include <filesystem>

// ─────────────────────────────────────────────────────────────
// Model paths — relative to working directory (MagOp-project/)
// ─────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────
// Helper — find first .onnx file in a directory
// ─────────────────────────────────────────────────────────────
static std::string findOnnx(const std::string& dir) {
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.path().extension() == ".onnx")
            return entry.path().string();
    }
    return "";
}

// ─────────────────────────────────────────────────────────────
// Model directories — just drop any .onnx file in here
// ─────────────────────────────────────────────────────────────
const std::string YOLO_MODEL_PATH    = findOnnx("models/yolo");
const std::string ANOMALY_MODEL_PATH = findOnnx("models/anomaly");
const std::string OCR_REC_PATH       = findOnnx("models/ocr");

// ─────────────────────────────────────────────────────────────
// Inference config
// ─────────────────────────────────────────────────────────────

constexpr float YOLO_CONF_THRESHOLD = 0.5f;
constexpr float YOLO_NMS_THRESHOLD  = 0.45f;
constexpr float ANOMALY_THRESHOLD   = 0.5f;
constexpr int   YOLO_INPUT_W        = 640;
constexpr int   YOLO_INPUT_H        = 640;
constexpr int   ANOMALY_INPUT_SIZE  = 256;
constexpr int   OCR_REC_H           = 48;

// PP-OCRv5 charset — must match training charset
const std::string CHARSET =
    " !\"#$%&'()*+,-./0123456789:;<=>?@"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
    "abcdefghijklmnopqrstuvwxyz{|}~";

// ─────────────────────────────────────────────────────────────
// Data Structures
// ─────────────────────────────────────────────────────────────

struct Detection {
    int      id;
    QString  label;
    float    confidence;
    cv::Rect boundingBox;
};

struct FrameResult {
    cv::Mat                originalImage;
    std::vector<Detection> detections;   // YOLOv8 output
    float                  anomalyScore = 0.0f; // Anomaly model output [0~1]
    QString                ocrText;     // PP-OCRv5 output
    QString                timestamp;

    FrameResult() {}
    FrameResult(const FrameResult& o)
        : originalImage(o.originalImage.clone()),
          detections(o.detections),
          anomalyScore(o.anomalyScore),
          ocrText(o.ocrText),
          timestamp(o.timestamp) {}
    ~FrameResult() {}
};

Q_DECLARE_METATYPE(FrameResult)

// ─────────────────────────────────────────────────────────────
// AI_Processing
// Pipeline: YOLOv8 → Anomaly → PP-OCRv5 (uses YOLO bbox)
// ─────────────────────────────────────────────────────────────

class AI_Processing : public QObject
{
    Q_OBJECT

public:
    explicit AI_Processing(QObject *parent = nullptr);
    ~AI_Processing();

    void addFrameToQueue(cv::Mat frame);
    void clearQueue();

    float anomalyThreshold = ANOMALY_THRESHOLD;

signals:
    void resultReady(FrameResult result);

private slots:
    void processNextFrame();

private:
    // ── ONNX Runtime ─────────────────────────────────────────
    Ort::Env*                          ortEnv      = nullptr;
    Ort::MemoryInfo*                   memInfo     = nullptr;
    std::unique_ptr<Ort::Session>      yoloSession;
    std::unique_ptr<Ort::Session>      anomalySession;
    std::unique_ptr<Ort::Session>      ocrRecSession;

    // ── Model runners ─────────────────────────────────────────
    std::vector<Detection> runYOLO(const cv::Mat& frame);
    float                  runAnomaly(const cv::Mat& frame);
    QString                runOCR(const cv::Mat& frame,
                                  const std::vector<Detection>& dets);

    // ── Preprocessing helpers ─────────────────────────────────
    std::vector<float> preprocessSquare(const cv::Mat& bgr, int size);
    std::pair<std::vector<float>, int> preprocessRec(const cv::Mat& crop);
    std::string decodeCTC(const float* data, int steps, int vocabSize);

    // ── Queue ─────────────────────────────────────────────────
    std::deque<cv::Mat> frameQueue;
    bool                isBusy = false;
    QMutex              mutex;
};

#endif // AI_PROCESSING_H