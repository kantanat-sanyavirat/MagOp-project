#include "ai_processing.h"
#include <QDateTime>
#include <QDebug>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>

// ─────────────────────────────────────────────────────────────
// Charset helpers
// ─────────────────────────────────────────────────────────────

std::vector<std::string> AI_Processing::loadCharset(const std::string& dictPath) {
    std::vector<std::string> result;
    std::ifstream f(dictPath);
    if (!f.is_open()) {
        qWarning() << "[AI] Cannot open dict:" << QString::fromStdString(dictPath);
        return result;
    }
    std::string line;
    while (std::getline(f, line))
        result.push_back(line.empty() ? " " : line);
    qDebug() << "[AI] Charset loaded:" << result.size() << "chars";
    return result;
}

// Split UTF-8 string into list of characters
static std::vector<std::string> splitUtf8(const std::string& s) {
    std::vector<std::string> chars;
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c = s[i];
        int len = (c & 0x80) == 0 ? 1 : (c & 0xE0) == 0xC0 ? 2 :
                  (c & 0xF0) == 0xE0 ? 3 : 4;
        chars.push_back(s.substr(i, len));
        i += len;
    }
    return chars;
}

// ─────────────────────────────────────────────────────────────
// Constructor / Destructor
// ─────────────────────────────────────────────────────────────

AI_Processing::AI_Processing(QObject *parent) : QObject(parent) {
    qRegisterMetaType<FrameResult>("FrameResult");

    // ── Hailo VDevice (Anomaly only) ──────────────────────────
    auto vdevice_exp = hailort::VDevice::create();
    if (!vdevice_exp) {
        qCritical() << "[AI] Hailo VDevice failed:" << vdevice_exp.status();
    } else {
        vdevice = vdevice_exp.release();

        if (!ANOMALY_HEF_PATH.empty()) {
            auto m = vdevice->create_infer_model(ANOMALY_HEF_PATH);
            if (!m) { qCritical() << "[AI] Anomaly hef failed:" << m.status(); }
            else {
                anomalyInferModel = m.release();
                anomalyInferModel->set_batch_size(1);
                anomalyInferModel->input()->set_format_type(HAILO_FORMAT_TYPE_UINT8);
                auto c = anomalyInferModel->configure();
                if (!c) { qCritical() << "[AI] Anomaly configure failed:" << c.status(); }
                else {
                    anomalyConfigured = std::move(c.release());
                    qDebug() << "[AI] Anomaly ready:" << QString::fromStdString(ANOMALY_HEF_PATH);
                }
            }
        }
    }

    // ── ONNX Runtime (YOLO + OCR CPU) ────────────────────────
    ortEnv  = new Ort::Env(ORT_LOGGING_LEVEL_ERROR, "MagOp");
    memInfo = new Ort::MemoryInfo(
        Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault));

    Ort::SessionOptions opts;
    opts.SetIntraOpNumThreads(4);
    opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

    if (!YOLO_ONNX_PATH.empty()) {
        try {
            yoloSession = std::make_unique<Ort::Session>(*ortEnv, YOLO_ONNX_PATH.c_str(), opts);
            qDebug() << "[AI] YOLO ready (ONNX CPU):" << QString::fromStdString(YOLO_ONNX_PATH);
        } catch (const Ort::Exception& e) {
            qCritical() << "[AI] YOLO failed:" << e.what();
        }
    }

    if (!OCR_REC_ONNX_PATH.empty()) {
        try {
            ocrRecSession = std::make_unique<Ort::Session>(*ortEnv, OCR_REC_ONNX_PATH.c_str(), opts);
            qDebug() << "[AI] OCR ready (ONNX CPU):" << QString::fromStdString(OCR_REC_ONNX_PATH);
        } catch (const Ort::Exception& e) {
            qCritical() << "[AI] OCR failed:" << e.what();
        }
    }

    // ── Load Thai charset ─────────────────────────────────────
    chars = loadCharset(OCR_DICT_PATH);
    if (chars.empty())
        qWarning() << "[AI] Charset empty — OCR will return blank";
}

AI_Processing::~AI_Processing() {
    QMutexLocker locker(&mutex);
    frameQueue.clear();
    delete memInfo;
    delete ortEnv;
}

// ─────────────────────────────────────────────────────────────
// Queue
// ─────────────────────────────────────────────────────────────

void AI_Processing::clearQueue() {
    QMutexLocker locker(&mutex);
    frameQueue.clear();
}

void AI_Processing::addFrameToQueue(cv::Mat frame) {
    QMutexLocker locker(&mutex);
    if (frame.empty()) return;
    frameQueue.push_back(frame.clone());
    if (!isBusy)
        QMetaObject::invokeMethod(this, "processNextFrame", Qt::QueuedConnection);
}

void AI_Processing::processNextFrame() {
    cv::Mat frame;
    {
        QMutexLocker locker(&mutex);
        if (frameQueue.empty()) { isBusy = false; return; }
        frame = frameQueue.front();
        frameQueue.pop_front();
        isBusy = true;
    }

    FrameResult result;
    result.originalImage = frame.clone();
    result.timestamp     = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");

    try {
        result.detections   = runYOLO(frame);
        result.anomalyScore = runAnomaly(frame);
        if (!result.detections.empty())
            result.ocrText = runOCR(frame, result.detections);
    } catch (const std::exception& e) {
        qWarning() << "[AI] Inference error:" << e.what();
    }

    emit resultReady(result);
    QMetaObject::invokeMethod(this, "processNextFrame", Qt::QueuedConnection);
}

// ─────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────

cv::Rect AI_Processing::mergeBoxes(const std::vector<Detection>& dets,
                                    const cv::Mat& image, int pad)
{
    int x1 = INT_MAX, y1 = INT_MAX, x2 = 0, y2 = 0;
    for (const auto& d : dets) {
        x1 = std::min(x1, d.boundingBox.x);
        y1 = std::min(y1, d.boundingBox.y);
        x2 = std::max(x2, d.boundingBox.x + d.boundingBox.width);
        y2 = std::max(y2, d.boundingBox.y + d.boundingBox.height);
    }
    return cv::Rect(
        std::max(0, x1 - pad), std::max(0, y1 - pad),
        std::min(image.cols, x2 + pad) - std::max(0, x1 - pad),
        std::min(image.rows, y2 + pad) - std::max(0, y1 - pad));
}

std::pair<std::vector<float>, int>
AI_Processing::preprocessRec(const cv::Mat& crop) {
    cv::Mat rgb;
    cv::cvtColor(crop, rgb, cv::COLOR_BGR2RGB);
    int rec_w = std::max(1, int(crop.cols * float(OCR_REC_H) / crop.rows));
    cv::resize(rgb, rgb, cv::Size(rec_w, OCR_REC_H));
    rgb.convertTo(rgb, CV_32F, 1.0 / 255.0);

    std::vector<float> tensor(3 * OCR_REC_H * rec_w);
    for (int c = 0; c < 3; ++c)
        for (int y = 0; y < OCR_REC_H; ++y)
            for (int x = 0; x < rec_w; ++x)
                tensor[c * OCR_REC_H * rec_w + y * rec_w + x] = rgb.at<cv::Vec3f>(y, x)[c];
    return {tensor, rec_w};
}

std::string AI_Processing::decodeCTC(const float* data, int steps, int vocabSize) {
    std::string result; int prev = -1;
    for (int t = 0; t < steps; ++t) {
        int best = 0; float bv = data[t * vocabSize];
        for (int v = 1; v < vocabSize; ++v)
            if (data[t * vocabSize + v] > bv) { bv = data[t * vocabSize + v]; best = v; }
        // PP-OCRv5: blank = index 0, chars start at index 1
        if (best != 0 && best != prev && (best - 1) < (int)chars.size())
            result += chars[best - 1];
        prev = best;
    }
    return result;
}

// ─────────────────────────────────────────────────────────────
// 1. YOLO via ONNX CPU
// ─────────────────────────────────────────────────────────────

std::vector<Detection> AI_Processing::runYOLO(const cv::Mat& frame) {
    if (!yoloSession) return {};

    float scale_x = float(frame.cols) / YOLO_INPUT_SIZE;
    float scale_y = float(frame.rows) / YOLO_INPUT_SIZE;

    cv::Mat rgb;
    cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);
    cv::resize(rgb, rgb, cv::Size(YOLO_INPUT_SIZE, YOLO_INPUT_SIZE));
    rgb.convertTo(rgb, CV_32F, 1.0 / 255.0);

    std::vector<float> input(3 * YOLO_INPUT_SIZE * YOLO_INPUT_SIZE);
    for (int c = 0; c < 3; ++c)
        for (int y = 0; y < YOLO_INPUT_SIZE; ++y)
            for (int x = 0; x < YOLO_INPUT_SIZE; ++x)
                input[c * YOLO_INPUT_SIZE * YOLO_INPUT_SIZE + y * YOLO_INPUT_SIZE + x] =
                    rgb.at<cv::Vec3f>(y, x)[c];

    std::array<int64_t, 4> shape = {1, 3, YOLO_INPUT_SIZE, YOLO_INPUT_SIZE};
    auto tensor = Ort::Value::CreateTensor<float>(
        *memInfo, input.data(), input.size(), shape.data(), shape.size());

    Ort::AllocatorWithDefaultOptions alloc;
    auto in_name  = yoloSession->GetInputNameAllocated(0, alloc);
    auto out_name = yoloSession->GetOutputNameAllocated(0, alloc);
    const char* ins[]  = {in_name.get()};
    const char* outs[] = {out_name.get()};

    auto outputs   = yoloSession->Run(Ort::RunOptions{nullptr}, ins, &tensor, 1, outs, 1);
    auto out_shape = outputs[0].GetTensorTypeAndShapeInfo().GetShape();
    int num_ch    = static_cast<int>(out_shape[1]);
    int num_boxes = static_cast<int>(out_shape[2]);
    int num_cls   = num_ch - 4;
    const float* raw = outputs[0].GetTensorData<float>();

    std::vector<cv::Rect> boxes;
    std::vector<float>    scores;

    for (int i = 0; i < num_boxes; ++i) {
        float max_score = 0.0f;
        for (int c = 0; c < num_cls; ++c) {
            float s = raw[(4 + c) * num_boxes + i];
            if (s > max_score) max_score = s;
        }
        if (max_score < YOLO_CONF_THRESHOLD) continue;

        float cx = raw[0 * num_boxes + i] * scale_x;
        float cy = raw[1 * num_boxes + i] * scale_y;
        float bw = raw[2 * num_boxes + i] * scale_x;
        float bh = raw[3 * num_boxes + i] * scale_y;
        int x1 = std::max(0, int(cx - bw/2));
        int y1 = std::max(0, int(cy - bh/2));
        int w  = std::min(int(bw), frame.cols - x1);
        int h  = std::min(int(bh), frame.rows - y1);
        if (w <= 0 || h <= 0) continue;
        boxes.push_back(cv::Rect(x1, y1, w, h));
        scores.push_back(max_score);
    }

    std::vector<int> indices;
    cv::dnn::NMSBoxes(boxes, scores, YOLO_CONF_THRESHOLD, YOLO_NMS_THRESHOLD, indices);

    std::vector<Detection> dets;
    for (int idx : indices) {
        Detection d;
        d.id          = idx;
        d.label       = "Serial Number";
        d.confidence  = scores[idx];
        d.boundingBox = boxes[idx];
        dets.push_back(d);
    }

    qDebug() << "[AI] YOLO:" << dets.size() << "detection(s)";
    return dets;
}

// ─────────────────────────────────────────────────────────────
// 2. Anomaly via Hailo NPU
// ─────────────────────────────────────────────────────────────

float AI_Processing::runAnomaly(const cv::Mat& frame) {
    if (!anomalyConfigured) return 0.0f;

    int iw = anomalyInferModel->input()->shape().width;
    int ih = anomalyInferModel->input()->shape().height;

    cv::Mat rgb;
    cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);
    cv::resize(rgb, rgb, cv::Size(iw, ih));

    auto bindings_exp = anomalyConfigured->create_bindings();
    if (!bindings_exp) return 0.0f;
    auto bindings = bindings_exp.release();

    // Aligned input buffer
    size_t in_size = rgb.total() * rgb.elemSize();
    void* in_ptr = nullptr;
    posix_memalign(&in_ptr, 16384, in_size);
    memcpy(in_ptr, rgb.data, in_size);
    bindings.input()->set_buffer(hailort::MemoryView(in_ptr, in_size));

    const std::string OUT_NAME = "model_fixed/normalization4";
    float qp_scale = 1.0f; int qp_zp = 0; size_t frame_size = 0;
    for (auto& out : anomalyInferModel->outputs()) {
        if (out.name() == OUT_NAME) {
            frame_size = out.get_frame_size();
            auto infos = out.get_quant_infos();
            if (!infos.empty()) { qp_scale = infos[0].qp_scale; qp_zp = infos[0].qp_zp; }
            break;
        }
    }

    void* out_ptr = nullptr;
    posix_memalign(&out_ptr, 16384, frame_size);
    memset(out_ptr, 0, frame_size);
    bindings.output(OUT_NAME)->set_buffer(hailort::MemoryView(out_ptr, frame_size));

    auto status = anomalyConfigured->run(bindings, std::chrono::milliseconds(5000));
    free(in_ptr);

    float score = 0.0f;
    if (status == HAILO_SUCCESS) {
        auto* buf = static_cast<uint8_t*>(out_ptr);
        float max_val = 0.0f;
        for (size_t i = 0; i < frame_size; ++i) {
            float dq = (buf[i] - qp_zp) * qp_scale;
            if (dq > max_val) max_val = dq;
        }
        score = 1.0f / (1.0f + std::exp(-max_val));
        score = std::max(0.0f, std::min(1.0f, score));
    } else {
        qWarning() << "[AI] Anomaly run failed:" << status;
    }
    free(out_ptr);

    qDebug() << "[AI] Anomaly:" << score;
    return score;
}

// ─────────────────────────────────────────────────────────────
// 3. OCR via ONNX CPU — merged bbox + Thai charset
// ─────────────────────────────────────────────────────────────

QString AI_Processing::runOCR(const cv::Mat& frame,
                               const std::vector<Detection>& dets) {
    if (!ocrRecSession || dets.empty() || chars.empty()) return "";

    cv::Rect merged = mergeBoxes(dets, frame, 20);
    if (merged.area() < 100) return "";

    cv::Mat crop = frame(merged);
    auto [rec_data, rec_w] = preprocessRec(crop);

    Ort::AllocatorWithDefaultOptions alloc;
    std::array<int64_t, 4> shape = {1, 3, OCR_REC_H, rec_w};
    auto tensor = Ort::Value::CreateTensor<float>(
        *memInfo, rec_data.data(), rec_data.size(),
        shape.data(), shape.size());

    auto in  = ocrRecSession->GetInputNameAllocated(0, alloc);
    auto out = ocrRecSession->GetOutputNameAllocated(0, alloc);
    const char* ins[]  = {in.get()};
    const char* outs[] = {out.get()};

    auto outputs = ocrRecSession->Run(Ort::RunOptions{nullptr}, ins, &tensor, 1, outs, 1);
    auto oshape  = outputs[0].GetTensorTypeAndShapeInfo().GetShape();
    std::string text = decodeCTC(outputs[0].GetTensorData<float>(),
                                  oshape[1], oshape[2]);

    qDebug() << "[AI] OCR:" << QString::fromStdString(text);
    return QString::fromStdString(text);
}