/*
 * Copyright (C) 2011-2024 Doubango Telecom <https://www.doubango.org>
 * File author: AI Assistant (Based on Python version by Mamadou DIOP)
 * License: For non commercial use only.
 * Source code: https://github.com/DoubangoTelecom/ultimateALPR-SDK
 * WebSite: https://www.doubango.org/webapps/alpr/
 *
 * C++ version of the Python video recognizer
 * Usage: 
 *     videorecognizer \
 *         --video <path-to-video-with-plate-to-recognize> \
 *         [--assets <path-to-assets-folder>] \
 *         [--charset <recognition-charset:latin/korean/chinese>] \
 *         [--tokenfile <path-to-license-token-file>] \
 *         [--tokendata <base64-license-token-data>]
 * Example:
 *     videorecognizer \
 *         --video /path/to/traffic.mp4 \
 *         --charset "latin" \
 *         --assets /path/to/assets \
 *         --tokenfile /path/to/license.lic
 */

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <chrono>
#include <thread>
#include <filesystem>
#include <opencv2/opencv.hpp>
#include <nlohmann/json.hpp>
#include <cxxopts.hpp>

// Include the ultimateALPR SDK header
#include "ultimateALPR-SDK-API-PUBLIC.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

// Tag for logging
const std::string TAG = "[UltAlprSdk] ";

// These are the strips on which if vehicle comes, gets counted
const std::pair<double, double> checkBoxout = {0.554, 0.60};
const std::pair<double, double> checkBoxin = {0.36, 0.41};

// Global variables
cv::VideoCapture video;
cv::VideoWriter savedVideo;
std::string video_address;
cv::Size imageSize(1280, 720);
int count = 0;
ULTALPR_SDK_IMAGE_TYPE format = ULTALPR_SDK_IMAGE_TYPE_BGR24;

// Car tracking data structures
std::map<std::string, std::shared_ptr<class Car>> detectedCars;
std::map<std::string, std::shared_ptr<class Car>> lastFrameCars;
std::map<std::string, std::shared_ptr<class Car>> currFrameCars;

// Car class to represent detected vehicles
class Car {
public:
    static int id;
    static cv::Size imageSize;
    static int incomingCount;
    static int outgoingCount;
    static std::pair<double, double> checkBoxout;
    static std::pair<double, double> checkBoxin;

    std::string text;
    std::vector<double> plateCoordinates;
    std::vector<double> carCoordinates;
    int carId;
    double speed;
    bool countSet;
    int frameNo;

    Car(const json& detection, int frameNumber) 
        : text(detection["text"]), 
          plateCoordinates(detection["warpedBox"]),
          carCoordinates(detection["car"]["warpedBox"]),
          carId(id++),
          speed(0.0),
          countSet(false),
          frameNo(frameNumber) {
        setCount();
    }

    bool isCountSet() const { return countSet; }
    double getSpeed() const { return speed; }
    std::string getText() const { return text; }
    std::vector<double> getCarCoordinates() const { return carCoordinates; }
    std::vector<double> getPlateCoordinates() const { return plateCoordinates; }

    void setSpeed(const json& detection, int frameNo) {
        std::vector<double> newCarCoordinates = detection["car"]["warpedBox"];
        double v1 = (carCoordinates[1] + carCoordinates[7]) / 2.0;
        double v2 = (newCarCoordinates[1] + newCarCoordinates[7]) / 2.0;
        int t = this->frameNo - frameNo;
        if (t != 0) {
            double newSpeed = std::abs((v1 - v2) / t);
            if (newSpeed > 0 && newSpeed < 1e6) {
                speed = newSpeed;
            }
        }
    }

    void setCount(const json* detection = nullptr) {
        if (countSet) return;

        double a1 = checkBoxout.first;
        double b1 = checkBoxout.second;
        double a2 = checkBoxin.first;
        double b2 = checkBoxin.second;

        if (detection) {
            carCoordinates = (*detection)["car"]["warpedBox"];
        }

        double carCenterX = (carCoordinates[0] + carCoordinates[2]) / 2.0;
        double carCenterY = (carCoordinates[1] + carCoordinates[7]) / 2.0;

        if (carCenterX > imageSize.width / 2.0) {
            // Incoming car (right half)
            if (carCenterY > imageSize.height * a2 && carCenterY < imageSize.height * b2) {
                incomingCount++;
                countSet = true;
            }
        } else {
            // Outgoing car (left half)
            if (carCenterY > imageSize.height * a1 && carCenterY < imageSize.height * b1) {
                outgoingCount++;
                countSet = true;
            }
        }
    }
};

// Static member initialization
int Car::id = 1;
cv::Size Car::imageSize(1280, 720);
int Car::incomingCount = 0;
int Car::outgoingCount = 0;
std::pair<double, double> Car::checkBoxout = {0.554, 0.60};
std::pair<double, double> Car::checkBoxin = {0.36, 0.41};

// Intersection over Union of two bounding boxes
double IOU(const std::vector<double>& boxA, const std::vector<double>& boxB) {
    // Determine the (x, y)-coordinates of the intersection rectangle
    double xA = std::max(boxA[0], boxB[0]);
    double yA = std::max(boxA[1], boxB[1]);
    double xB = std::min(boxA[4], boxB[4]);
    double yB = std::min(boxA[5], boxB[5]);

    // Compute the area of intersection rectangle
    double interArea = std::max(0.0, xB - xA) * std::max(0.0, yB - yA);

    // Compute the area of both the prediction and ground-truth rectangles
    double boxAArea = (boxA[2] - boxA[0]) * (boxA[7] - boxA[1]);
    double boxBArea = (boxB[2] - boxB[0]) * (boxB[7] - boxB[1]);

    // Compute the intersection over union
    double iou = interArea / (boxAArea + boxBArea - interArea);
    return iou;
}

// Operate function to handle car detection and tracking
void operate(const json& detection, int frameNo) {
    std::string text = detection["text"];
    
    // Check if this text is already detected
    if (detectedCars.find(text) != detectedCars.end()) {
        auto& car = detectedCars[text];
        car->setSpeed(detection, frameNo);
        if (!car->isCountSet()) {
            car->setCount(&detection);
        }
        car->frameNo = frameNo;
        car->carCoordinates = detection["car"]["warpedBox"];
        car->plateCoordinates = detection["warpedBox"];
        currFrameCars[text] = car;
    } else {
        // Check if this car has IOU > 0.58 with any previously detected cars
        bool flag = false;
        std::vector<double> carCoordinates = detection["car"]["warpedBox"];
        
        for (auto it = lastFrameCars.begin(); it != lastFrameCars.end(); ++it) {
            auto& lfc = it->second;
            double iou = IOU(lfc->carCoordinates, carCoordinates);
            if (iou >= 0.58) {
                lfc->setSpeed(detection, frameNo);
                lfc->frameNo = frameNo;
                lfc->carCoordinates = carCoordinates;
                lfc->plateCoordinates = detection["warpedBox"];
                
                std::string oldText = it->first;
                std::string modifiedText = (oldText > text) ? oldText : text;
                lfc->text = modifiedText;
                
                lastFrameCars.erase(it);
                detectedCars.erase(oldText);
                detectedCars[modifiedText] = lfc;
                currFrameCars[modifiedText] = lfc;
                
                if (!lfc->isCountSet()) {
                    lfc->setCount();
                }
                flag = true;
                break;
            }
        }
        
        if (!flag) {
            auto newCar = std::make_shared<Car>(detection, frameNo);
            detectedCars[text] = newCar;
            currFrameCars[text] = newCar;
        }
    }
}

// Get texts and bounding boxes of cars in current frame
std::pair<std::vector<std::string>, std::vector<std::pair<std::vector<double>, std::vector<double>>>> getTW() {
    std::vector<std::string> texts_lst;
    std::vector<std::pair<std::vector<double>, std::vector<double>>> warpedBox;
    
    for (const auto& pair : currFrameCars) {
        const auto& car = pair.second;
        texts_lst.push_back(car->getText());
        warpedBox.push_back({car->getPlateCoordinates(), car->getCarCoordinates()});
    }
    
    return {texts_lst, warpedBox};
}

// Check result helper function
std::pair<std::vector<std::pair<std::vector<double>, std::vector<double>>>, std::vector<std::string>> 
checkResult(const std::string& operation, const UltAlprSdkResult& result) {
    std::vector<std::pair<std::vector<double>, std::vector<double>>> warpedBoxes;
    std::vector<std::string> texts_lst;
    
    if (!result.isOK()) {
        std::cout << TAG << operation << ": failed -> " << result.phrase() << std::endl;
    } else {
        try {
            json data = json::parse(result.json());
            if (data.contains("plates")) {
                std::cout << data["frame_id"] << std::endl;
                for (const auto& plate : data["plates"]) {
                    if (plate.contains("car")) {
                        std::cout << "car : " << plate["text"] << std::endl;
                        operate(plate, data["frame_id"]);
                    }
                }
            }
            auto result_pair = getTW();
            texts_lst = result_pair.first;
            warpedBoxes = result_pair.second;
            std::cout << "Detected texts: ";
            for (const auto& text : texts_lst) {
                std::cout << text << " ";
            }
            std::cout << std::endl;
        } catch (const json::exception& e) {
            std::cerr << "JSON parsing error: " << e.what() << std::endl;
        }
    }
    
    return {warpedBoxes, texts_lst};
}

// Default JSON configuration
json getDefaultConfig() {
    return {
        {"debug_level", "info"},
        {"debug_write_input_image_enabled", false},
        {"debug_internal_data_path", "."},
        {"num_threads", -1},
        {"gpgpu_enabled", true},
        {"max_latency", -1},
        {"klass_vcr_gamma", 1.5},
        {"detect_roi", {0, 0, 0, 0}},
        {"detect_minscore", 0.1},
        {"car_noplate_detect_min_score", 0.8},
        {"pyramidal_search_enabled", true},
        {"pyramidal_search_sensitivity", 0.28},
        {"pyramidal_search_minscore", 0.3},
        {"pyramidal_search_min_image_size_inpixels", 800},
        {"recogn_minscore", 0.3},
        {"recogn_score_type", "min"}
    };
}

// Main predict function
std::pair<std::vector<std::pair<std::vector<double>, std::vector<double>>>, std::vector<std::string>> 
predict(const cxxopts::ParseResult& args, const cv::Mat& frame) {
    static bool initialized = false;
    
    if (!initialized) {
        // Update JSON configuration
        json config = getDefaultConfig();
        config["assets_folder"] = args["assets"].as<std::string>();
        config["charset"] = args["charset"].as<std::string>();
        config["car_noplate_detect_enabled"] = args["car_noplate_detect_enabled"].as<bool>();
        config["ienv_enabled"] = args["ienv_enabled"].as<bool>();
        config["openvino_enabled"] = args["openvino_enabled"].as<bool>();
        config["openvino_device"] = args["openvino_device"].as<std::string>();
        config["klass_lpci_enabled"] = args["klass_lpci_enabled"].as<bool>();
        config["klass_vcr_enabled"] = args["klass_vcr_enabled"].as<bool>();
        config["klass_vmmr_enabled"] = args["klass_vmmr_enabled"].as<bool>();
        config["klass_vbsr_enabled"] = args["klass_vbsr_enabled"].as<bool>();
        config["license_token_file"] = args["tokenfile"].as<std::string>();
        config["license_token_data"] = args["tokendata"].as<std::string>();

        // Initialize the engine
        checkResult("Init", UltAlprSdkEngine::init(config.dump().c_str()));
        initialized = true;
    }

    // Convert BGR to RGB for processing
    cv::Mat rgbFrame;
    cv::cvtColor(frame, rgbFrame, cv::COLOR_BGR2RGB);
    
    // Process the frame
    return checkResult("Process", 
        UltAlprSdkEngine::process(
            format,
            rgbFrame.data,
            rgbFrame.cols,
            rgbFrame.rows,
            0, // stride
            1  // exifOrientation
        )
    );
}

// Check FPS of input video
int checkFPS() {
    std::cout << "Checking FPS" << std::endl;
    auto t1 = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 300; i++) {
        cv::Mat frame;
        if (!video.read(frame)) break;
        std::cout << "." << std::flush;
    }
    
    auto t2 = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);
    int fps = static_cast<int>(300000.0 / duration.count());
    
    video.release();
    video.open(video_address);
    
    std::cout << "\nDone. FPS: " << fps << std::endl;
    
    // Get frame size
    cv::Mat frame;
    video.read(frame);
    imageSize = frame.size();
    Car::imageSize = imageSize;
    Car::checkBoxout = checkBoxout;
    Car::checkBoxin = checkBoxin;
    
    std::cout << "Image size: " << imageSize.width << "x" << imageSize.height << std::endl;
    
    return fps;
}

// Setup video writer
std::pair<cv::VideoWriter, int> videoWriterSetup(const std::string& output_file) {
    int frame_width = static_cast<int>(video.get(cv::CAP_PROP_FRAME_WIDTH));
    int frame_height = static_cast<int>(video.get(cv::CAP_PROP_FRAME_HEIGHT));
    cv::Size size(frame_width, frame_height);
    
    int fps = checkFPS();
    cv::VideoWriter writer(output_file, cv::VideoWriter::fourcc('m', 'p', '4', 'v'), fps, size);
    
    return {writer, fps};
}

// Display processed image
cv::Mat displayInCv2(const std::vector<std::pair<std::vector<double>, std::vector<double>>>& warpedBox, 
                     const std::vector<std::string>& texts, 
                     cv::Mat frame) {
    // Draw detection strips
    int x1 = 0;
    int x2 = imageSize.width / 2 - 1;
    int y1 = static_cast<int>(checkBoxout.first * imageSize.height);
    int y2 = static_cast<int>(checkBoxout.second * imageSize.height) - 1;
    
    cv::Mat shape = cv::Mat::zeros(frame.size(), frame.type());
    cv::rectangle(shape, cv::Point(x1, y1), cv::Point(x2, y2), cv::Scalar(0, 0, 255), cv::FILLED);
    
    x1 = imageSize.width / 2;
    x2 = imageSize.width - 1;
    y1 = static_cast<int>(checkBoxin.first * imageSize.height);
    y2 = static_cast<int>(checkBoxin.second * imageSize.height) - 1;
    cv::rectangle(shape, cv::Point(x1, y1), cv::Point(x2, y2), cv::Scalar(0, 0, 255), cv::FILLED);
    
    // Apply overlay
    double alpha = 0.5;
    cv::addWeighted(frame, alpha, shape, 1 - alpha, 0, frame);
    
    // Draw base for counts
    x1 = 30; x2 = 200; y1 = 20; y2 = 100;
    shape = cv::Mat::zeros(frame.size(), frame.type());
    cv::rectangle(shape, cv::Point(x1, y1), cv::Point(x2, y2), cv::Scalar(255, 255, 255), cv::FILLED);
    alpha = 0.2;
    cv::addWeighted(frame, alpha, shape, 1 - alpha, 0, frame);
    
    // Draw bounding boxes and text
    if (!warpedBox.empty()) {
        for (size_t i = 0; i < warpedBox.size() && i < texts.size(); i++) {
            const auto& obj = warpedBox[i];
            const std::string& text = texts[i];
            
            std::vector<int> box1, box2;
            for (size_t j = 0; j < obj.first.size(); j++) {
                box1.push_back(static_cast<int>(obj.first[j]));
            }
            for (size_t j = 0; j < obj.second.size(); j++) {
                box2.push_back(static_cast<int>(obj.second[j]));
            }
            
            // Draw plate bounding box (blue)
            cv::rectangle(frame, cv::Point(box1[0], box1[1]), cv::Point(box1[4], box1[5]), 
                         cv::Scalar(255, 0, 0), 1);
            
            // Draw plate text
            cv::putText(frame, text, cv::Point(box1[0] - 30, box1[1]), 
                       cv::FONT_HERSHEY_DUPLEX, 0.9, cv::Scalar(0, 200, 255), 2, cv::LINE_AA);
            
            // Draw car bounding box (green)
            cv::rectangle(frame, cv::Point(box2[0], box2[1]), cv::Point(box2[4], box2[5]), 
                         cv::Scalar(0, 255, 0), 2);
            
            // Draw speed
            if (currFrameCars.find(text) != currFrameCars.end()) {
                double speed = currFrameCars[text]->getSpeed();
                std::string speedText = std::to_string(speed).substr(0, std::to_string(speed).find('.') + 3);
                cv::putText(frame, speedText, cv::Point(box2[0], box2[1]), 
                           cv::FONT_HERSHEY_TRIPLEX, 0.7, cv::Scalar(0, 200, 255), 1, cv::LINE_AA);
            }
        }
    }
    
    // Draw counts
    cv::putText(frame, "out:" + std::to_string(Car::outgoingCount), cv::Point(50, 50), 
               cv::FONT_HERSHEY_DUPLEX, 1, cv::Scalar(255, 0, 0), 2, cv::LINE_AA);
    cv::putText(frame, "in:" + std::to_string(Car::incomingCount), cv::Point(50, 80), 
               cv::FONT_HERSHEY_DUPLEX, 1, cv::Scalar(255, 0, 0), 2, cv::LINE_AA);
    
    cv::imshow("Video Recognizer", frame);
    return frame;
}

int main(int argc, char* argv[]) {
    try {
        cxxopts::Options options("videorecognizer", "C++ Video Recognizer using ultimateALPR SDK");
        
        options.add_options()
            ("v,video", "Path to the video with ALPR data to recognize", cxxopts::value<std::string>())
            ("a,assets", "Path to the assets folder", cxxopts::value<std::string>()->default_value("../../../assets"))
            ("d,duration", "Maximum duration to process in seconds", cxxopts::value<int>())
            ("c,charset", "Recognition charset (latin, korean, chinese)", cxxopts::value<std::string>()->default_value("latin"))
            ("car_noplate_detect_enabled", "Detect cars with no plate", cxxopts::value<bool>()->default_value(false))
            ("ienv_enabled", "Enable Image Enhancement for Night-Vision", cxxopts::value<bool>()->default_value(false))
            ("openvino_enabled", "Enable OpenVINO", cxxopts::value<bool>()->default_value(true))
            ("openvino_device", "OpenVINO device (CPU, GPU, FPGA)", cxxopts::value<std::string>()->default_value("CPU"))
            ("klass_lpci_enabled", "Enable License Plate Country Identification", cxxopts::value<bool>()->default_value(false))
            ("klass_vcr_enabled", "Enable Vehicle Color Recognition", cxxopts::value<bool>()->default_value(false))
            ("klass_vmmr_enabled", "Enable Vehicle Make Model Recognition", cxxopts::value<bool>()->default_value(false))
            ("klass_vbsr_enabled", "Enable Vehicle Body Style Recognition", cxxopts::value<bool>()->default_value(false))
            ("tokenfile", "Path to license token file", cxxopts::value<std::string>()->default_value(""))
            ("tokendata", "Base64 license token data", cxxopts::value<std::string>()->default_value(""))
            ("h,help", "Print usage");
        
        auto args = options.parse(argc, argv);
        
        if (args.count("help") || !args.count("video")) {
            std::cout << options.help() << std::endl;
            return 0;
        }
        
        video_address = args["video"].as<std::string>();
        
        // Create output filename
        fs::path videoPath(video_address);
        std::string outputName = videoPath.stem().string() + "_annotated" + videoPath.extension().string();
        std::string outputPath = (videoPath.parent_path() / outputName).string();
        
        std::cout << "Processing video file: " << video_address << std::endl;
        std::cout << "Output will be written to: " << outputPath << std::endl;
        
        // Open video
        video.open(video_address);
        if (!video.isOpened()) {
            std::cerr << "Error: Could not open video file: " << video_address << std::endl;
            return -1;
        }
        
        // Setup video writer
        auto [writer, fps] = videoWriterSetup(outputPath);
        savedVideo = writer;
        
        // Calculate maximum frames to process
        int max_frames = -1;
        if (args.count("duration")) {
            max_frames = fps * args["duration"].as<int>();
            std::cout << "Processing first " << args["duration"].as<int>() << " seconds (" << max_frames << " frames)" << std::endl;
        } else {
            std::cout << "Processing entire video" << std::endl;
        }
        
        int frame_count = 0;
        
        try {
            while (true) {
                cv::Mat frame;
                if (!video.read(frame)) {
                    break; // End of video
                }
                
                // Check if we've reached the maximum frames limit
                if (max_frames > 0 && frame_count >= max_frames) {
                    std::cout << "Reached maximum duration limit" << std::endl;
                    break;
                }
                
                char key = cv::waitKey(1);
                if (key == 27) { // ESC key
                    std::cout << "Mission abort" << std::endl;
                    break;
                }
                
                frame_count++;
                
                // Process frame
                auto [warpedBox, texts] = predict(args, frame);
                
                // Display and save frame
                frame = displayInCv2(warpedBox, texts, frame);
                savedVideo.write(frame);
                
                // Update tracking
                lastFrameCars = currFrameCars;
                currFrameCars.clear();
                
                // Print progress every 100 frames
                if (frame_count % 100 == 0) {
                    std::cout << "Processed " << frame_count << " frames..." << std::endl;
                }
            }
            
            std::cout << "Completed processing " << frame_count << " frames" << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "Error during processing: " << e.what() << std::endl;
        }
        
        // Cleanup
        cv::destroyAllWindows();
        video.release();
        savedVideo.release();
        
        // Deinitialize the engine
        checkResult("DeInit", UltAlprSdkEngine::deInit());
        
        // Save detected number plates
        std::vector<std::string> numberplates;
        for (const auto& pair : detectedCars) {
            numberplates.push_back(pair.first);
        }
        
        std::ofstream outFile("numberplates.txt");
        if (outFile.is_open()) {
            for (const auto& plate : numberplates) {
                outFile << plate << std::endl;
            }
            outFile.close();
        }
        
        std::cout << "Detected number plates: ";
        for (const auto& plate : numberplates) {
            std::cout << plate << " ";
        }
        std::cout << std::endl;
        
    } catch (const cxxopts::OptionException& e) {
        std::cerr << "Error parsing options: " << e.what() << std::endl;
        return -1;
    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
        return -1;
    }
    
    return 0;
} 