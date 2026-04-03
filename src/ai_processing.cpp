#include "ai_processing.h"
#include <QDateTime>
#include <QDebug>
#include <algorithm>

// ─────────────────────────────────────────────────────────────
// Constructor / Destructor
// ─────────────────────────────────────────────────────────────

AI_Processing::AI_Processing(QObject *parent) : QObject(parent) {
    qRegisterMetaType<FrameResult>("FrameResult");

    ortEnv  = new Ort::Env(ORT_LOGGING_LEVEL_WARNING, "MagOp");
    memInfo = new Ort::MemoryInfo(
        Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault));

    // Session options — tuned for Raspberry Pi 5 (4 cores)
    Ort::SessionOptions opts;
    opts.SetIntraOpNumThreads(4);
    opts.SetInterOpNumThreads(1);
    opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

    try {
        yoloSession    = std::make_unique<Ort::Session>(*ortEnv, YOLO_MODEL_PATH.c_str(), opts);
        anomalySession = std::make_unique<Ort::Session>(*ortEnv, ANOMALY_MODEL_PATH.c_str(), opts);
        ocrRecSession  = std::make_unique<Ort::Session>(*ortEnv, OCR_REC_PATH.c_str(), opts);
        qDebug() << "[AI] All models loaded successfully";
    } catch (const Ort::Exception& e) {
        qCritical() << "[AI] Failed to load model:" << e.what();
    }
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
        result.anomalyScore = runAnomaly(frame); // always run — works on full image

        if (result.detections.empty()) {
            // YOLO found nothing — skip OCR only
            qDebug() << "[AI] No detections — skipping OCR";
            result.ocrText = "";
        } else {
            result.ocrText = runOCR(frame, result.detections);
        }
    } catch (const Ort::Exception& e) {
        qWarning() << "[AI] Inference error:" << e.what();
    }

    emit resultReady(result);
    QMetaObject::invokeMethod(this, "processNextFrame", Qt::QueuedConnection);
}

// ─────────────────────────────────────────────────────────────
// Preprocessing
// ─────────────────────────────────────────────────────────────

std::vector<float> AI_Processing::preprocessSquare(const cv::Mat& bgr, int size) {
    cv::Mat rgb;
    cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);
    cv::resize(rgb, rgb, cv::Size(size, size));
    rgb.convertTo(rgb, CV_32F, 1.0 / 255.0);

    std::vector<float> tensor(3 * size * size);
    for (int c = 0; c < 3; ++c)
        for (int h = 0; h < size; ++h)
            for (int w = 0; w < size; ++w)
                tensor[c * size * size + h * size + w] = rgb.at<cv::Vec3f>(h, w)[c];
    return tensor;
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
        for (int h = 0; h < OCR_REC_H; ++h)
            for (int w = 0; w < rec_w; ++w)
                tensor[c * OCR_REC_H * rec_w + h * rec_w + w] = rgb.at<cv::Vec3f>(h, w)[c];
    return {tensor, rec_w};
}

std::string AI_Processing::decodeCTC(const float* data, int steps, int vocabSize) {
    std::string result;
    int prev = -1;
    for (int t = 0; t < steps; ++t) {
        int   best = 0;
        float best_val = data[t * vocabSize];
        for (int v = 1; v < vocabSize; ++v)
            if (data[t * vocabSize + v] > best_val) { best_val = data[t * vocabSize + v]; best = v; }
        int blank = vocabSize - 1;
        if (best != blank && best != prev && best < (int)CHARSET.size())
            result += CHARSET[best];
        prev = best;
    }
    return result;
}

// ─────────────────────────────────────────────────────────────
// 1. YOLOv8
// ─────────────────────────────────────────────────────────────

std::vector<Detection> AI_Processing::runYOLO(const cv::Mat& frame) {
    float scale_x = float(frame.cols) / YOLO_INPUT_W;
    float scale_y = float(frame.rows) / YOLO_INPUT_H;

    auto input_data = preprocessSquare(frame, YOLO_INPUT_W);
    std::array<int64_t, 4> shape = {1, 3, YOLO_INPUT_H, YOLO_INPUT_W};
    auto tensor = Ort::Value::CreateTensor<float>(
        *memInfo, input_data.data(), input_data.size(),
        shape.data(), shape.size());

    Ort::AllocatorWithDefaultOptions alloc;
    auto in_name  = yoloSession->GetInputNameAllocated(0, alloc);
    auto out_name = yoloSession->GetOutputNameAllocated(0, alloc);
    const char* in[]  = {in_name.get()};
    const char* out[] = {out_name.get()};

    auto outputs = yoloSession->Run(Ort::RunOptions{nullptr}, in, &tensor, 1, out, 1);

    auto   out_shape   = outputs[0].GetTensorTypeAndShapeInfo().GetShape();
    int    num_ch      = static_cast<int>(out_shape[1]);
    int    num_boxes   = static_cast<int>(out_shape[2]);
    int    num_classes = num_ch - 4;
    const float* raw   = outputs[0].GetTensorData<float>();

    std::vector<cv::Rect> boxes;
    std::vector<float>    scores;
    std::vector<int>      class_ids;

    for (int i = 0; i < num_boxes; ++i) {
        float max_score = 0.0f; int best_cls = 0;
        for (int c = 0; c < num_classes; ++c) {
            float s = raw[(4 + c) * num_boxes + i];
            if (s > max_score) { max_score = s; best_cls = c; }
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
        class_ids.push_back(best_cls);
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
// 2. Anomaly
// ─────────────────────────────────────────────────────────────

float AI_Processing::runAnomaly(const cv::Mat& frame) {
    cv::Mat rgb;
    cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);
    cv::resize(rgb, rgb, cv::Size(ANOMALY_INPUT_SIZE, ANOMALY_INPUT_SIZE));
    rgb.convertTo(rgb, CV_32F, 1.0 / 255.0);

    constexpr float mean[3] = {0.485f, 0.456f, 0.406f};
    constexpr float std_[3] = {0.229f, 0.224f, 0.225f};

    std::vector<float> tensor(3 * ANOMALY_INPUT_SIZE * ANOMALY_INPUT_SIZE);
    for (int c = 0; c < 3; ++c)
        for (int h = 0; h < ANOMALY_INPUT_SIZE; ++h)
            for (int w = 0; w < ANOMALY_INPUT_SIZE; ++w)
                tensor[c * ANOMALY_INPUT_SIZE * ANOMALY_INPUT_SIZE + h * ANOMALY_INPUT_SIZE + w]
                    = (rgb.at<cv::Vec3f>(h, w)[c] - mean[c]) / std_[c];

    std::array<int64_t, 4> shape = {1, 3, ANOMALY_INPUT_SIZE, ANOMALY_INPUT_SIZE};
    auto input_tensor = Ort::Value::CreateTensor<float>(
        *memInfo, tensor.data(), tensor.size(), shape.data(), shape.size());

    Ort::AllocatorWithDefaultOptions alloc;
    auto in_name  = anomalySession->GetInputNameAllocated(0, alloc);
    auto out_name = anomalySession->GetOutputNameAllocated(0, alloc);
    const char* in[]  = {in_name.get()};
    const char* out[] = {out_name.get()};

    auto outputs = anomalySession->Run(Ort::RunOptions{nullptr}, in, &input_tensor, 1, out, 1);

    auto   out_shape = outputs[0].GetTensorTypeAndShapeInfo().GetShape();
    size_t count = 1;
    for (auto d : out_shape) count *= static_cast<size_t>(d);
    const float* data = outputs[0].GetTensorData<float>();
    float score = std::max(0.0f, std::min(1.0f, *std::max_element(data, data + count)));

    qDebug() << "[AI] Anomaly score:" << score
             << (score > anomalyThreshold ? "→ ANOMALY" : "→ NORMAL");
    return score;
}

// ─────────────────────────────────────────────────────────────
// 3. PP-OCRv5 Recognition (uses YOLO bbox)
// ─────────────────────────────────────────────────────────────

QString AI_Processing::runOCR(const cv::Mat& frame,
                               const std::vector<Detection>& dets) {
    if (dets.empty()) return "";

    Ort::AllocatorWithDefaultOptions alloc;
    QStringList texts;

    for (const auto& det : dets) {
        cv::Rect roi = det.boundingBox & cv::Rect(0, 0, frame.cols, frame.rows);
        if (roi.area() < 64) continue;

        cv::Mat crop = frame(roi);
        auto [rec_data, rec_w] = preprocessRec(crop);

        std::array<int64_t, 4> rec_shape = {1, 3, OCR_REC_H, rec_w};
        auto rec_tensor = Ort::Value::CreateTensor<float>(
            *memInfo, rec_data.data(), rec_data.size(),
            rec_shape.data(), rec_shape.size());

        auto in_name  = ocrRecSession->GetInputNameAllocated(0, alloc);
        auto out_name = ocrRecSession->GetOutputNameAllocated(0, alloc);
        const char* in[]  = {in_name.get()};
        const char* out[] = {out_name.get()};

        auto rec_outputs = ocrRecSession->Run(
            Ort::RunOptions{nullptr}, in, &rec_tensor, 1, out, 1);

        auto shape     = rec_outputs[0].GetTensorTypeAndShapeInfo().GetShape();
        int  steps     = static_cast<int>(shape[1]);
        int  vocab_sz  = static_cast<int>(shape[2]);
        const float* out_data = rec_outputs[0].GetTensorData<float>();

        std::string text = decodeCTC(out_data, steps, vocab_sz);
        if (!text.empty()) {
            texts << QString::fromStdString(text);
            qDebug() << "[AI] OCR region:" << QString::fromStdString(text);
        }
    }

    QString result = texts.join(" ");
    qDebug() << "[AI] OCR result:" << result;
    return result;
}