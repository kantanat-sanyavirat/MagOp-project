#ifndef AI_PROCESSING_H
#define AI_PROCESSING_H

#include <QObject>
#include <QMetaType>
#include <QMutex>
#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>
#include <hailo/hailort.hpp>
#include <vector>
#include <deque>
#include <string>
#include <filesystem>
#include <memory>
#include <optional>
#include <fstream>

// ─────────────────────────────────────────────────────────────
// Auto-detect model files
// ─────────────────────────────────────────────────────────────

static inline std::string findModel(const std::string& dir, const std::string& ext) {
    if (!std::filesystem::exists(dir)) return "";
    for (const auto& e : std::filesystem::directory_iterator(dir))
        if (e.path().extension() == ext) return e.path().string();
    return "";
}

// YOLO    → .onnx (CPU)   — swap to .hef when ready
// Anomaly → .hef  (Hailo NPU)
// OCR     → .onnx (CPU)
const std::string YOLO_ONNX_PATH     = findModel("models/yolo",    ".onnx");
const std::string ANOMALY_HEF_PATH   = findModel("models/anomaly", ".hef");
const std::string OCR_REC_ONNX_PATH  = findModel("models/ocr",     ".onnx");
const std::string OCR_DICT_PATH      = findModel("models/ocr",     ".txt");

const std::string THAI_FONT_PATH = "/usr/share/fonts/truetype/tlwg/Garuda.ttf";

// ─────────────────────────────────────────────────────────────
// Inference config
// ─────────────────────────────────────────────────────────────

constexpr float YOLO_CONF_THRESHOLD = 0.3f;
constexpr float YOLO_NMS_THRESHOLD  = 0.45f;
constexpr float ANOMALY_THRESHOLD   = 0.5f;
constexpr int   YOLO_INPUT_SIZE     = 640;
constexpr int   OCR_REC_H           = 48;

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
    std::vector<Detection> detections;
    float                  anomalyScore = 0.0f;
    QString                ocrText;
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
//
// YOLO    → ONNX CPU  (.onnx)
// Anomaly → Hailo NPU (.hef)
// OCR     → ONNX CPU  (.onnx) + Thai charset dict
// ─────────────────────────────────────────────────────────────

class AI_Processing : public QObject {
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
    // ── Hailo (Anomaly) ───────────────────────────────────────
    std::shared_ptr<hailort::VDevice>            vdevice;
    std::shared_ptr<hailort::InferModel>         anomalyInferModel;
    std::optional<hailort::ConfiguredInferModel> anomalyConfigured;

    // ── ONNX Runtime (YOLO + OCR CPU) ────────────────────────
    Ort::Env*                         ortEnv    = nullptr;
    Ort::MemoryInfo*                  memInfo   = nullptr;
    std::unique_ptr<Ort::Session>     yoloSession;
    std::unique_ptr<Ort::Session>     ocrRecSession;

    // ── Thai charset ──────────────────────────────────────────
    std::vector<std::string>          chars;    // loaded from dict file

    // ── Model runners ─────────────────────────────────────────
    std::vector<Detection> runYOLO(const cv::Mat& frame);
    float                  runAnomaly(const cv::Mat& frame);
    QString                runOCR(const cv::Mat& frame,
                                  const std::vector<Detection>& dets);

    // ── Helpers ───────────────────────────────────────────────
    std::pair<std::vector<float>, int> preprocessRec(const cv::Mat& crop);
    std::string decodeCTC(const float* data, int steps, int vocabSize);
    static std::vector<std::string> loadCharset(const std::string& dictPath);

    // ── Merge bboxes for OCR ──────────────────────────────────
    static cv::Rect mergeBoxes(const std::vector<Detection>& dets,
                               const cv::Mat& image, int pad = 20);

    // ── Queue ─────────────────────────────────────────────────
    std::deque<cv::Mat> frameQueue;
    bool                isBusy = false;
    QMutex              mutex;
};

#endif // AI_PROCESSING_H