#include <opencv2/opencv.hpp>
#include <iostream>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <cmath>
#include <filesystem>
#include <queue>
#include <random>
#include <sys/statvfs.h>
#include <sys/resource.h>

using namespace std;
using namespace cv;
namespace fs = std::filesystem;

// Shared variables with mutex protection
struct SystemStats {
    float cpuUsage = 0.0f;
    float ramUsage = 0.0f;
    float storageUsage = 0.0f;
    float fps = 0.0f;
    string netStatus = "Disconnected";
    string batteryStatus = "Unknown";
    string dateTime = "";
};

// Kernel log structure
struct KernelLog {
    string timestamp;
    string message;
    int severity; // 0-3: info, notice, warning, error
};

static mutex statsMutex;
static mutex logMutex;
static long double prevTotal = 0, prevIdle = 0;
static bool running = true;
static queue<KernelLog> kernelLogs;
static const int MAX_LOG_ENTRIES = 8;

// Face detection tracking
static bool faceDetected = false;
static Rect lastFaceRect;
static const int FACE_POSITION_THRESHOLD = 100;
static chrono::steady_clock::time_point lastCaptureTimePoint;
static chrono::steady_clock::time_point faceDetectionStartTime;
static const int COOLDOWN_SECONDS = 5;
static const int DETECTION_WINDOW_SECONDS = 1;
static bool isInCooldown = false;
static int noFaceCounter = 0;
static const int NO_FACE_THRESHOLD = 10;
static bool pictureTaken = false;
static chrono::steady_clock::time_point lastFaceSeenTime;
static bool isNewPerson = true;

// Kernel log messages
const vector<string> INFO_MESSAGES = {
    "System initialized",
    "Memory block allocated",
    "CPU core scaling: performance",
    "Processing unit online",
    "Tracking algorithm loaded",
    "Connection established",
    "System active",
    "Analysis running",
    "Processing initialized",
    "Scanning active"
};

const vector<string> WARNING_MESSAGES = {
    "CPU threshold approaching",
    "Memory fragmentation detected",
    "Network latency increasing",
    "I/O bottleneck detected",
    "Identification timeout",
    "Buffer overflow prevented",
    "Resource contention detected"
};

const vector<string> ERROR_MESSAGES = {
    "Database access failed",
    "Network corruption",
    "Security breach detected",
    "Invalid memory address",
    "System error prevented"
};

const vector<string> SECURITY_MESSAGES = {
    "Subject identified: Processing",
    "Database search: In progress",
    "Scan: Active",
    "Level: Low",
    "Confidence: 78.2%",
    "Analysis: Normal",
    "Access: Restricted"
};

const vector<string> TARGET_ACQUIRED_MESSAGES = {
    "Processing data",
    "Image captured",
    "Analysis in progress",
    "Verification: Active",
    "Saving data",
    "Tracking: Active",
    "Protocols engaged"
};

// Add these constants at the top with other constants
static const size_t MAX_MEMORY_USAGE = 300 * 1024 * 1024; // 300MB in bytes
static const int MAX_QUEUE_SIZE = 100; // Limit log queue size

double calculateRectDistance(const Rect& rect1, const Rect& rect2) {
    Point center1(rect1.x + rect1.width/2, rect1.y + rect1.height/2);
    Point center2(rect2.x + rect2.width/2, rect2.y + rect2.height/2);
    return sqrt(pow(center1.x - center2.x, 2) + pow(center1.y - center2.y, 2));
}

string getCurrentDateTime() {
    auto now = chrono::system_clock::now();
    time_t now_c = chrono::system_clock::to_time_t(now);
    tm now_tm = *localtime(&now_c);
    stringstream ss;
    ss << put_time(&now_tm, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

string getKernelLogTimestamp() {
    auto now = chrono::system_clock::now();
    time_t now_c = chrono::system_clock::to_time_t(now);
    tm now_tm = *localtime(&now_c);
    stringstream ss;
    ss << put_time(&now_tm, "%H:%M:%S.") << setw(3) << setfill('0') << rand() % 1000;
    return ss.str();
}

void addKernelLog(const string& message, int severity) {
    lock_guard<mutex> lock(logMutex);
    
    // Check current memory usage
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        size_t currentMemory = usage.ru_maxrss * 1024; // Convert from KB to bytes
        if (currentMemory > MAX_MEMORY_USAGE) {
            // Clear some old logs if memory usage is too high
            while (!kernelLogs.empty() && currentMemory > MAX_MEMORY_USAGE) {
                kernelLogs.pop();
                if (getrusage(RUSAGE_SELF, &usage) == 0) {
                    currentMemory = usage.ru_maxrss * 1024;
                }
            }
        }
    }

    // Add new log with size limit
    if (kernelLogs.size() < MAX_QUEUE_SIZE) {
        kernelLogs.push({getKernelLogTimestamp(), message, severity});
    }
}

void generateRandomLogs() {
    while (running) {
        // Generate random logs periodically
        this_thread::sleep_for(chrono::milliseconds(800 + (rand() % 1500)));

        int logType = rand() % 20;

        // If picture was taken, prioritize target acquired messages
        if (pictureTaken && (logType < 12)) {
            addKernelLog(TARGET_ACQUIRED_MESSAGES[rand() % TARGET_ACQUIRED_MESSAGES.size()], 3); // Use severity 3 for red color
        } else if (logType < 10) {
            // Info logs are most common
            addKernelLog(INFO_MESSAGES[rand() % INFO_MESSAGES.size()], 0);
        } else if (logType < 17) {
            // Security logs are next most common when face is detected
            if (faceDetected) {
                addKernelLog(SECURITY_MESSAGES[rand() % SECURITY_MESSAGES.size()], 1);
            } else {
                addKernelLog(INFO_MESSAGES[rand() % INFO_MESSAGES.size()], 0);
            }
        } else if (logType < 19) {
            // Warnings are less common
            addKernelLog(WARNING_MESSAGES[rand() % WARNING_MESSAGES.size()], 2);
        } else {
            // Errors are rare
            addKernelLog(ERROR_MESSAGES[rand() % ERROR_MESSAGES.size()], 3);
        }
    }
}

void systemMonitor(SystemStats& stats) {
    while (running) {
        ifstream statFile("/proc/stat");
        if (statFile.is_open()) {
            string line;
            getline(statFile, line);
            statFile.close();
            istringstream ss(line);
            string cpu;
            long double user, nice, system, idle, iowait, irq, softirq, steal;
            ss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
            long double totalCpu = user + nice + system + idle + iowait + irq + softirq + steal;
            long double totalIdle = idle;
            float cpuUsage = (prevTotal == 0) ? 0 : 100.0 * (1 - (totalIdle - prevIdle) / (totalCpu - prevTotal));
            prevTotal = totalCpu;
            prevIdle = totalIdle;

            ifstream meminfo("/proc/meminfo");
            long long total = 0, free = 0, available = 0;
            while (getline(meminfo, line)) {
                if (line.find("MemTotal:") == 0) sscanf(line.c_str(), "MemTotal: %lld kB", &total);
                else if (line.find("MemFree:") == 0) sscanf(line.c_str(), "MemFree: %lld kB", &free);
                else if (line.find("MemAvailable:") == 0) sscanf(line.c_str(), "MemAvailable: %lld kB", &available);
            }
            meminfo.close();
            float ramUsage = (available > 0) ?
            ((total - available) / static_cast<float>(total)) * 100.0 :
            ((total - free) / static_cast<float>(total)) * 100.0;

            string batteryStatus;
            ifstream batteryFile("/sys/class/power_supply/BAT0/capacity");
            if (batteryFile.is_open()) {
                int capacity;
                batteryFile >> capacity;
                batteryStatus = "Battery: " + to_string(capacity) + "%";
                batteryFile.close();
            } else {
                batteryStatus = "Battery: Unknown";
            }

            // Get storage usage
            struct statvfs stat;
            if (statvfs("/", &stat) == 0) {
                float total = stat.f_blocks * stat.f_frsize;
                float available = stat.f_bfree * stat.f_frsize;
                float used = total - available;
                float storageUsage = (used / total) * 100.0;
                
                lock_guard<mutex> lock(statsMutex);
                stats.cpuUsage = cpuUsage;
                stats.ramUsage = ramUsage;
                stats.storageUsage = storageUsage;
                stats.batteryStatus = batteryStatus;
                stats.dateTime = getCurrentDateTime();
            }
        }
        this_thread::sleep_for(chrono::seconds(1));
    }
}

void pingNetwork(SystemStats& stats) {
    while (running) {
        const char* cmd = "ping -c 1 google.com > /dev/null 2>&1";
        int result = system(cmd);
        {
            lock_guard<mutex> lock(statsMutex);
            stats.netStatus = (result == 0) ? "Connected" : "Disconnected";
        }
        this_thread::sleep_for(chrono::seconds(5));
    }
}

void applyTint(Mat& frame) {
    frame.forEach<Vec3b>([](Vec3b& pixel, const int* position) -> void {
        if (pictureTaken) {
            pixel[0] = static_cast<uchar>(min(pixel[0] * 1.5, 255.0));
            pixel[1] = static_cast<uchar>(pixel[1] * 0.5);
            pixel[2] = static_cast<uchar>(pixel[2] * 0.5);
        } else {
            pixel[0] = static_cast<uchar>(pixel[0] * 0.5);
            pixel[1] = static_cast<uchar>(pixel[1] * 0.5);
            pixel[2] = static_cast<uchar>(min(pixel[2] * 1.5, 255.0));
        }
    });
}

void drawKernelLogs(Mat& frame) {
    lock_guard<mutex> lock(logMutex);

    // Create a semi-transparent black background for the logs
    int logHeight = MAX_LOG_ENTRIES * 18 + 20;
    int logWidth = 220; // Further reduced width
    int startX = frame.cols - logWidth + 5; // Push even more to the right
    int startY = frame.rows - logHeight - 10;

    // Draw header based on state
    Scalar headerColor;
    string headerText;
    if (pictureTaken) {
        headerColor = Scalar(50, 50, 255);
        headerText = "[ LOG ]";
    } else {
        headerColor = Scalar(50, 230, 50);
        headerText = "[ LOG ]";
    }

    putText(frame, headerText, Point(startX + 2, startY + 15),
            FONT_HERSHEY_PLAIN, 0.7, headerColor, 1, LINE_AA);

    // Draw logs with color coding that matches the active state
    int i = 0;
    queue<KernelLog> logs = kernelLogs;
    while (!logs.empty()) {
        KernelLog log = logs.front();
        logs.pop();

        Scalar color;
        if (pictureTaken) {
            switch (log.severity) {
                case 0: color = Scalar(100, 100, 200); break;
                case 1: color = Scalar(50, 120, 220); break;
                case 2: color = Scalar(30, 70, 255); break;
                case 3: color = Scalar(30, 30, 255); break;
            }
        } else {
            switch (log.severity) {
                case 0: color = Scalar(50, 230, 50); break;
                case 1: color = Scalar(80, 220, 200); break;
                case 2: color = Scalar(50, 200, 255); break;
                case 3: color = Scalar(50, 50, 255); break;
            }
        }

        string logText = "[" + log.timestamp + "] " + log.message;
        putText(frame, logText, Point(startX + 2, startY + 40 + (i * 18)),
                FONT_HERSHEY_PLAIN, 0.6, color, 1, LINE_AA);
        i++;
    }
}

int main() {
    // Seed random number generator
    srand(time(nullptr));

    if (!fs::exists("snapshot")) fs::create_directory("snapshot");

    CascadeClassifier faceCascade;
    if (!faceCascade.load("/usr/share/opencv4/haarcascades/haarcascade_frontalface_default.xml")) {
        cerr << "Error loading Haar cascade file!" << endl;
        return -1;
    }

    VideoCapture capture;
    capture.open(0, CAP_V4L2); // Use V4L2 backend explicitly
    if (!capture.isOpened()) {
        cerr << "Error opening video stream! Trying fallback..." << endl;
        capture.open(0); // Fallback to default
        if (!capture.isOpened()) {
            cerr << "Failed to open camera!" << endl;
            return -1;
        }
    }

    // Set camera properties
    capture.set(CAP_PROP_FPS, 30);
    capture.set(CAP_PROP_FRAME_WIDTH, 640);
    capture.set(CAP_PROP_FRAME_HEIGHT, 480);

    SystemStats stats;
    thread systemThread(systemMonitor, ref(stats));
    thread networkThread(pingNetwork, ref(stats));
    thread logGeneratorThread(generateRandomLogs);

    // Add initial kernel logs
    addKernelLog("System initialized", 0);
    addKernelLog("Camera active", 0);
    addKernelLog("Face detection ready", 0);
    addKernelLog("Monitoring active", 0);

    Mat frame;
    int frameCount = 0;
    auto lastFpsTime = chrono::steady_clock::now();
    int frameSkip = 3;

    while (running) {
        auto frameStart = chrono::steady_clock::now();
        
        if (!capture.read(frame) || frame.empty()) {
            cerr << "Failed to capture frame!" << endl;
            break;
        }

        // Calculate FPS every second
        frameCount++;
        auto currentTime = chrono::steady_clock::now();
        auto fpsElapsed = chrono::duration_cast<chrono::milliseconds>(currentTime - lastFpsTime).count();
        if (fpsElapsed >= 1000) {  // Update FPS every second
            float fps = (frameCount * 1000.0f) / fpsElapsed;
            {
                lock_guard<mutex> lock(statsMutex);
                stats.fps = fps;
            }
            frameCount = 0;
            lastFpsTime = currentTime;
        }

        Mat resizedFrame, gray;
        resize(frame, resizedFrame, Size(640, 480));
        cvtColor(resizedFrame, gray, COLOR_BGR2GRAY);

        auto currentTimePoint = chrono::steady_clock::now();
        if (isInCooldown) {
            auto cooldownElapsed = chrono::duration_cast<chrono::seconds>(currentTimePoint - lastCaptureTimePoint).count();
            if (cooldownElapsed >= COOLDOWN_SECONDS) {
                isInCooldown = false;
                if (pictureTaken) {
                    addKernelLog("Analysis complete", 0);
                    pictureTaken = false;
                }
            }
        }

        if (frameCount % frameSkip == 0) {
            vector<Rect> faces;
            faceCascade.detectMultiScale(gray, faces, 1.1, 4, 0, Size(30, 30));

            // Create a clean frame for saving (without rectangle)
            Mat cleanFrame = resizedFrame.clone();

            for (const auto& face : faces) {
                // Draw rectangle only on the display frame
                rectangle(resizedFrame, face, Scalar(255, 255, 255), 2);

                if (faceDetected) {
                    double distance = calculateRectDistance(face, lastFaceRect);
                    isNewPerson = (distance > FACE_POSITION_THRESHOLD);

                    if (isNewPerson) {
                        addKernelLog("New subject detected", 2);
                    }
                }

                if (!faceDetected) {
                    faceDetectionStartTime = currentTimePoint;
                    faceDetected = true;
                    lastFaceRect = face;
                    noFaceCounter = 0;
                    lastFaceSeenTime = currentTimePoint;
                    addKernelLog("Human subject detected in frame", 0);
                }

                auto detectionElapsed = chrono::duration_cast<chrono::seconds>(currentTimePoint - faceDetectionStartTime).count();
                if (!isInCooldown && detectionElapsed <= DETECTION_WINDOW_SECONDS) {
                    string timestamp = getCurrentDateTime();
                    replace(timestamp.begin(), timestamp.end(), ' ', '_');
                    replace(timestamp.begin(), timestamp.end(), ':', '_');
                    string filename = "snapshot/face_detected_" + timestamp + ".jpg";
                    if (imwrite(filename, cleanFrame)) {  // Save the clean frame without rectangle
                        cout << "Picture saved: " << filename << endl;
                        addKernelLog("Image captured", 3);
                        pictureTaken = true;

                        // Clear all logs and add target acquisition messages when picture is taken
                        {
                            lock_guard<mutex> lock(logMutex);
                            queue<KernelLog> empty;
                            swap(kernelLogs, empty);
                        }
                        addKernelLog("Analysis in progress", 3);
                        addKernelLog("Processing data", 3);
                        addKernelLog("Scan in progress", 3);
                        addKernelLog("Searching database", 3);
                    } else {
                        cerr << "Failed to save picture!" << endl;
                        addKernelLog("Failed to capture image", 3);
                    }
                    lastCaptureTimePoint = currentTimePoint;
                    isInCooldown = true;
                }
            }

            if (faces.empty()) {
                noFaceCounter++;
                auto timeSinceLastFace = chrono::duration_cast<chrono::seconds>(currentTimePoint - lastFaceSeenTime).count();
                if (noFaceCounter >= NO_FACE_THRESHOLD || timeSinceLastFace >= 1) {
                    if (faceDetected) {
                        addKernelLog("Subject lost from view", 1);
                    }
                    faceDetected = false;
                    noFaceCounter = 0;
                }
            } else {
                lastFaceSeenTime = currentTimePoint;
            }
        }

        applyTint(resizedFrame);

        // Draw kernel logs
        drawKernelLogs(resizedFrame);

        {
            lock_guard<mutex> lock(statsMutex);
            stringstream cpuText, ramText, storageText, fpsText;
            cpuText << "CPU: " << fixed << setprecision(2) << stats.cpuUsage << "%";
            ramText << "RAM: " << fixed << setprecision(2) << stats.ramUsage << "%";
            storageText << "STO: " << fixed << setprecision(2) << stats.storageUsage << "%";
            fpsText << "FPS: " << fixed << setprecision(1) << stats.fps;
            string netText = "NET: " + stats.netStatus;

            Scalar textColor = pictureTaken ? Scalar(255, 255, 255) : Scalar(255, 255, 255);

            putText(resizedFrame, fpsText.str(), Point(10, 20), FONT_HERSHEY_SIMPLEX, 0.4, textColor, 1);
            putText(resizedFrame, cpuText.str(), Point(10, 35), FONT_HERSHEY_SIMPLEX, 0.4, textColor, 1);
            putText(resizedFrame, ramText.str(), Point(10, 50), FONT_HERSHEY_SIMPLEX, 0.4, textColor, 1);
            putText(resizedFrame, storageText.str(), Point(10, 65), FONT_HERSHEY_SIMPLEX, 0.4, textColor, 1);
            putText(resizedFrame, netText, Point(10, 80), FONT_HERSHEY_SIMPLEX, 0.4, textColor, 1);

            if (pictureTaken) {
                string statusText = "ANALYSIS ACTIVE";
                Scalar statusColor = Scalar(30, 30, 255);
                putText(resizedFrame, statusText, Point(resizedFrame.cols - 150, 20),
                        FONT_HERSHEY_SIMPLEX, 0.4, statusColor, 1);
            }

            putText(resizedFrame, stats.dateTime, Point(resizedFrame.cols - 160, 35),
                    FONT_HERSHEY_SIMPLEX, 0.4, textColor, 1);
        }

        imshow("Face Detection", resizedFrame);
        int key = waitKey(1);
        if (key == 'q' || key == 27) { // 'q' or ESC
            running = false;
            break;
        }

        // Clean up frames after use
        frame.release();
        gray.release();
        resizedFrame.release();

        // Calculate time spent processing this frame
        auto frameEnd = chrono::steady_clock::now();
        auto processingTime = chrono::duration_cast<chrono::microseconds>(frameEnd - frameStart).count();
        
        // Target frame time for 24 FPS (41666 microseconds)
        const int targetFrameTime = 41666;
        
        // Calculate required sleep time
        int sleepTime = targetFrameTime - processingTime;
        if (sleepTime > 0) {
            usleep(sleepTime);
        }
    }

    // Clean up resources
    capture.release();
    destroyAllWindows();
    
    // Clear the log queue
    {
        lock_guard<mutex> lock(logMutex);
        queue<KernelLog> empty;
        swap(kernelLogs, empty);
    }

    running = false;
    systemThread.join();
    networkThread.join();
    logGeneratorThread.join();

    return 0;
}
