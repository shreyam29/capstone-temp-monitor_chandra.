// temp_monitor.cpp
// Opens /dev/tempsensor fresh each second, drives a small state machine
// (NORMAL -> WARNING -> CRITICAL -> LOW_BATTERY), and logs a message on
// every transition.
//
// Usage:
//   sudo ./temp_monitor                 run the monitoring loop
//   sudo ./temp_monitor --reset         send TEMP_IOC_RESET, then exit
//   sudo ./temp_monitor --drift N       send TEMP_IOC_SET_DRIFT with value N
//                                       (tenths of a degree), then run the loop
//   sudo ./temp_monitor --battery N     send TEMP_IOC_SET_BATTERY with value N
//                                       (percent 0-100), then run the loop
//
// Capstone teaching app - not for production use.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <string>
#include <chrono>
#include <thread>
#include <ctime>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "../driver/tempsensor_ioctl.h"

static const char *DEV_PATH = "/dev/tempsensor";
static const int BATTERY_THRESHOLD = 20;  // LOW_BATTERY when battery <= 20%

// ─── Logger ────────────────────────────────────────────────────────────────────

class Logger {
public:
    static std::string timestamp() {
        std::time_t t = std::time(nullptr);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%H:%M:%S", std::localtime(&t));
        return std::string(buf);
    }

    static void info(const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        printf("[%s] ", timestamp().c_str());
        vprintf(fmt, args);
        printf("\n");
        va_end(args);
    }

    static void transition(const char *from, const char *to, double temp) {
        printf("[%s] TRANSITION: %s -> %s  (temp = %.1f C)\n",
               timestamp().c_str(), from, to, temp);
    }

    static void reading(double temp, const char *state) {
        printf("[%s] temp = %.1f C  (state: %s)\n",
               timestamp().c_str(), temp, state);
    }

    static void error(const char *msg) {
        fprintf(stderr, "[%s] %s\n", timestamp().c_str(), msg);
    }
};

// ─── TempReader ────────────────────────────────────────────────────────────────

class TempReader {
public:
    TempReader() : fd_(-1) {}

    ~TempReader() {
        if (fd_ >= 0)
            close(fd_);
    }

    bool openDevice() {
        fd_ = open(DEV_PATH, O_RDONLY);
        if (fd_ < 0) {
            perror("open (is the module loaded? are you root?)");
            return false;
        }
        return true;
    }

    void closeDevice() {
        if (fd_ >= 0) {
            close(fd_);
            fd_ = -1;
        }
    }

    double readTemperature() {
        // Open fresh each time (driver returns one reading per open/read cycle)
        int fd = open(DEV_PATH, O_RDONLY);
        if (fd < 0) {
            perror("open");
            return -1.0;
        }

        char buf[16] = {0};
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        close(fd);

        if (n <= 0) {
            fprintf(stderr, "read returned %zd\n", n);
            return -1.0;
        }
        buf[n] = '\0';
        return std::atof(buf);
    }

    bool sendReset() {
        int fd = open(DEV_PATH, O_RDONLY);
        if (fd < 0) {
            perror("open (is the module loaded? are you root?)");
            return false;
        }
        bool ok = (ioctl(fd, TEMP_IOC_RESET) >= 0);
        if (!ok) perror("ioctl RESET");
        close(fd);
        return ok;
    }

    bool sendSetDrift(int drift) {
        int fd = open(DEV_PATH, O_RDONLY);
        if (fd < 0) {
            perror("open (is the module loaded? are you root?)");
            return false;
        }
        bool ok = (ioctl(fd, TEMP_IOC_SET_DRIFT, &drift) >= 0);
        if (!ok) perror("ioctl SET_DRIFT");
        close(fd);
        return ok;
    }

    bool sendSetBattery(int percent) {
        int fd = open(DEV_PATH, O_RDONLY);
        if (fd < 0) {
            perror("open (is the module loaded? are you root?)");
            return false;
        }
        bool ok = (ioctl(fd, TEMP_IOC_SET_BATTERY, &percent) >= 0);
        if (!ok) perror("ioctl SET_BATTERY");
        close(fd);
        return ok;
    }

    int getBattery() {
        int fd = open(DEV_PATH, O_RDONLY);
        if (fd < 0) {
            perror("open");
            return -1;
        }
        int level = -1;
        if (ioctl(fd, TEMP_IOC_GET_BATTERY, &level) < 0) {
            perror("ioctl GET_BATTERY");
            close(fd);
            return -1;
        }
        close(fd);
        return level;
    }

private:
    int fd_;
};

// ─── StateMachine ──────────────────────────────────────────────────────────────

class StateMachine {
public:
    enum class State { NORMAL, WARNING, CRITICAL, LOW_BATTERY };

    StateMachine() : current_(State::NORMAL) {}

    static const char *stateName(State s) {
        switch (s) {
            case State::NORMAL:      return "NORMAL";
            case State::WARNING:     return "WARNING";
            case State::CRITICAL:    return "CRITICAL";
            case State::LOW_BATTERY: return "LOW_BATTERY";
        }
        return "UNKNOWN";
    }

    // Classify temperature into thermal state (ignoring battery)
    static State classifyTemp(double temp_c) {
        if (temp_c > 80.0) return State::CRITICAL;
        if (temp_c >= 60.0) return State::WARNING;
        return State::NORMAL;
    }

    // Full classify: battery overrides thermal states when low
    State classify(double temp_c, int battery) {
        if (battery >= 0 && battery <= BATTERY_THRESHOLD)
            return State::LOW_BATTERY;
        return classifyTemp(temp_c);
    }

    // Update state, return true if transition occurred
    bool update(double temp_c, int battery) {
        State next = classify(temp_c, battery);
        if (next != current_) {
            const char *from = stateName(current_);
            current_ = next;
            // Store transition info for caller
            lastTransitionFrom = from;
            lastTransitionTemp = temp_c;
            return true;
        }
        return false;
    }

    State current() const { return current_; }
    const char *currentName() const { return stateName(current_); }
    const char *lastFrom() const { return lastTransitionFrom; }
    double lastTemp() const { return lastTransitionTemp; }

private:
    State current_;
    const char *lastTransitionFrom = "";
    double lastTransitionTemp = 0.0;
};

// ─── main ──────────────────────────────────────────────────────────────────────

int main(int argc, char *argv[])
{
    TempReader reader;
    StateMachine sm;

    // --reset : one-shot ioctl, then exit
    if (argc == 2 && std::strcmp(argv[1], "--reset") == 0) {
        if (!reader.sendReset()) return 1;
        Logger::info("Sensor reset to baseline.");
        return 0;
    }

    // --drift N : set drift, then continue into the monitoring loop
    if (argc == 3 && std::strcmp(argv[1], "--drift") == 0) {
        int drift = std::atoi(argv[2]);
        if (!reader.sendSetDrift(drift)) return 1;
        Logger::info("Drift set to %.1f C per reading.", drift / 10.0);
    }

    // --battery N : set battery level, then continue into the monitoring loop
    if (argc == 3 && std::strcmp(argv[1], "--battery") == 0) {
        int batt = std::atoi(argv[2]);
        if (!reader.sendSetBattery(batt)) return 1;
        Logger::info("Battery set to %d%%.", batt);
    }

    // sanity check the device exists / is openable before starting the loop
    {
        int fd = open(DEV_PATH, O_RDONLY);
        if (fd < 0) {
            perror("open (is the module loaded? are you root?)");
            return 1;
        }
        close(fd);
    }

    Logger::info("Monitoring started. Press Ctrl+C to stop.");
    Logger::info("Initial state: %s", sm.currentName());

    while (true) {
        double temp = reader.readTemperature();
        int battery = reader.getBattery();

        if (temp < 0) {
            Logger::error("Failed to read sensor, retrying...");
        } else {
            if (sm.update(temp, battery)) {
                Logger::transition(sm.lastFrom(), sm.currentName(), sm.lastTemp());
            } else {
                Logger::reading(temp, sm.currentName());
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
