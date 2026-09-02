// test_classify.cpp
// Unit tests for the StateMachine::classify logic.
// Compile: g++ -Wall -O2 -o test_classify test_classify.cpp
// Run:     ./test_classify

#include <cstdio>
#include <cstdlib>
#include <string>

// ─── Inline the StateMachine class for standalone testing ──────────────────────

static const int BATTERY_THRESHOLD = 20;

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

    static State classifyTemp(double temp_c) {
        if (temp_c > 80.0) return State::CRITICAL;
        if (temp_c >= 60.0) return State::WARNING;
        return State::NORMAL;
    }

    State classify(double temp_c, int battery) {
        if (battery >= 0 && battery <= BATTERY_THRESHOLD)
            return State::LOW_BATTERY;
        return classifyTemp(temp_c);
    }

    bool update(double temp_c, int battery) {
        State next = classify(temp_c, battery);
        if (next != current_) {
            current_ = next;
            return true;
        }
        return false;
    }

    State current() const { return current_; }

private:
    State current_;
};

// ─── Test helpers ──────────────────────────────────────────────────────────────

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT_EQ(a, b, msg) do { \
    tests_run++; \
    if ((a) == (b)) { \
        tests_passed++; \
    } else { \
        fprintf(stderr, "FAIL: %s (expected %s, got %s)\n", \
                msg, StateMachine::stateName(b), StateMachine::stateName(a)); \
    } \
} while(0)

#define ASSERT_TRUE(expr, msg) do { \
    tests_run++; \
    if (expr) { \
        tests_passed++; \
    } else { \
        fprintf(stderr, "FAIL: %s\n", msg); \
    } \
} while(0)

#define ASSERT_FALSE(expr, msg) do { \
    tests_run++; \
    if (!(expr)) { \
        tests_passed++; \
    } else { \
        fprintf(stderr, "FAIL: %s\n", msg); \
    } \
} while(0)

// ─── Tests ─────────────────────────────────────────────────────────────────────

void test_classify_temp_normal() {
    ASSERT_EQ(StateMachine::classifyTemp(0.0),   StateMachine::State::NORMAL,   "0.0C -> NORMAL");
    ASSERT_EQ(StateMachine::classifyTemp(25.0),  StateMachine::State::NORMAL,   "25.0C -> NORMAL");
    ASSERT_EQ(StateMachine::classifyTemp(59.9),  StateMachine::State::NORMAL,   "59.9C -> NORMAL");
}

void test_classify_temp_warning() {
    ASSERT_EQ(StateMachine::classifyTemp(60.0),  StateMachine::State::WARNING,  "60.0C -> WARNING");
    ASSERT_EQ(StateMachine::classifyTemp(70.0),  StateMachine::State::WARNING,  "70.0C -> WARNING");
    ASSERT_EQ(StateMachine::classifyTemp(80.0),  StateMachine::State::WARNING,  "80.0C -> WARNING");
}

void test_classify_temp_critical() {
    ASSERT_EQ(StateMachine::classifyTemp(80.1),  StateMachine::State::CRITICAL, "80.1C -> CRITICAL");
    ASSERT_EQ(StateMachine::classifyTemp(100.0), StateMachine::State::CRITICAL, "100.0C -> CRITICAL");
    ASSERT_EQ(StateMachine::classifyTemp(120.0), StateMachine::State::CRITICAL, "120.0C -> CRITICAL");
}

void test_classify_low_battery() {
    // Battery at or below threshold overrides thermal state
    ASSERT_EQ(StateMachine().classify(25.0, 0),  StateMachine::State::LOW_BATTERY, "25C + 0% batt -> LOW_BATTERY");
    ASSERT_EQ(StateMachine().classify(25.0, 10), StateMachine::State::LOW_BATTERY, "25C + 10% batt -> LOW_BATTERY");
    ASSERT_EQ(StateMachine().classify(25.0, 20), StateMachine::State::LOW_BATTERY, "25C + 20% batt -> LOW_BATTERY");
    ASSERT_EQ(StateMachine().classify(90.0, 5),  StateMachine::State::LOW_BATTERY, "90C + 5% batt -> LOW_BATTERY");
}

void test_classify_battery_above_threshold() {
    // Battery above threshold, use thermal classification
    ASSERT_EQ(StateMachine().classify(25.0, 21), StateMachine::State::NORMAL,   "25C + 21% batt -> NORMAL");
    ASSERT_EQ(StateMachine().classify(25.0, 50), StateMachine::State::NORMAL,   "25C + 50% batt -> NORMAL");
    ASSERT_EQ(StateMachine().classify(25.0, 100),StateMachine::State::NORMAL,   "25C + 100% batt -> NORMAL");
    ASSERT_EQ(StateMachine().classify(70.0, 50), StateMachine::State::WARNING,  "70C + 50% batt -> WARNING");
    ASSERT_EQ(StateMachine().classify(90.0, 50), StateMachine::State::CRITICAL, "90C + 50% batt -> CRITICAL");
}

void test_classify_negative_battery() {
    // Negative battery means "no battery info" -> use thermal only
    ASSERT_EQ(StateMachine().classify(25.0, -1), StateMachine::State::NORMAL,   "25C + -1 batt -> NORMAL");
    ASSERT_EQ(StateMachine().classify(90.0, -1), StateMachine::State::CRITICAL, "90C + -1 batt -> CRITICAL");
}

void test_state_transitions() {
    StateMachine sm;
    ASSERT_EQ(sm.current(), StateMachine::State::NORMAL, "starts NORMAL");

    // NORMAL -> WARNING
    ASSERT_TRUE(sm.update(65.0, 50), "transition to WARNING");
    ASSERT_EQ(sm.current(), StateMachine::State::WARNING, "now WARNING");

    // WARNING -> CRITICAL
    ASSERT_TRUE(sm.update(85.0, 50), "transition to CRITICAL");
    ASSERT_EQ(sm.current(), StateMachine::State::CRITICAL, "now CRITICAL");

    // CRITICAL -> LOW_BATTERY (battery dies)
    ASSERT_TRUE(sm.update(85.0, 10), "transition to LOW_BATTERY");
    ASSERT_EQ(sm.current(), StateMachine::State::LOW_BATTERY, "now LOW_BATTERY");

    // LOW_BATTERY -> NORMAL (battery charged, temp normal)
    ASSERT_TRUE(sm.update(25.0, 80), "transition back to NORMAL");
    ASSERT_EQ(sm.current(), StateMachine::State::NORMAL, "back to NORMAL");
}

void test_no_transition_same_state() {
    StateMachine sm;
    ASSERT_FALSE(sm.update(30.0, 50), "no transition at 30C");
    ASSERT_FALSE(sm.update(35.0, 50), "no transition at 35C");
    ASSERT_FALSE(sm.update(40.0, 50), "no transition at 40C");
}

void test_boundary_values() {
    // Exact boundary: 60.0 should be WARNING (>= 60)
    ASSERT_EQ(StateMachine::classifyTemp(60.0), StateMachine::State::WARNING, "60.0 exactly -> WARNING");
    // Just below: 59.999 should be NORMAL
    ASSERT_EQ(StateMachine::classifyTemp(59.999), StateMachine::State::NORMAL, "59.999 -> NORMAL");
    // Exact boundary: 80.0 should be WARNING (not > 80)
    ASSERT_EQ(StateMachine::classifyTemp(80.0), StateMachine::State::WARNING, "80.0 exactly -> WARNING");
    // Just above: 80.001 should be CRITICAL
    ASSERT_EQ(StateMachine::classifyTemp(80.001), StateMachine::State::CRITICAL, "80.001 -> CRITICAL");
    // Battery boundary: 20 should be LOW_BATTERY (<= 20)
    ASSERT_EQ(StateMachine().classify(25.0, 20), StateMachine::State::LOW_BATTERY, "20% batt -> LOW_BATTERY");
    // Just above: 21 should NOT be LOW_BATTERY
    ASSERT_EQ(StateMachine().classify(25.0, 21), StateMachine::State::NORMAL, "21% batt -> NORMAL");
}

// ─── Main ──────────────────────────────────────────────────────────────────────

int main() {
    printf("Running classify() unit tests...\n\n");

    test_classify_temp_normal();
    test_classify_temp_warning();
    test_classify_temp_critical();
    test_classify_low_battery();
    test_classify_battery_above_threshold();
    test_classify_negative_battery();
    test_state_transitions();
    test_no_transition_same_state();
    test_boundary_values();

    printf("\n%d/%d tests passed.\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
