// RPN (Reverse Polish Notation) scientific calculator for the M5Stack
// Cardputer ADV, in the classic HP style: a 4-level X/Y/Z/T register stack
// that is always visible on screen (not a typed-expression text buffer).
//
// Entering numbers:
//   Type digits/. then press Enter to push the number onto the stack
//   (X moves to Y, Y to Z, Z to T -- the old T is dropped). Pressing Enter
//   on an empty line duplicates X into Y (classic RPN "ENTER" behavior),
//   so "5 Enter Enter *" squares 5. There's no "-" key for negative
//   numbers (that's always subtract, see below, as on a real RPN
//   calculator) -- press opt+- (CHS) to flip the sign of the number being
//   typed, or of X itself when nothing is being typed, e.g. "5 opt+- Enter"
//   pushes -5.
//
// Operators (+ - * / ^ % !) apply immediately, no Enter needed:
//   + - * / ^ %   pop Y and X, push f(Y,X) (e.g. Y^X for ^, Y mod X for %)
//   !             factorial of X, in place (no Y involved)
//   Typing digits first (e.g. "12", then "-") finalizes that number as X
//   before applying the operator, exactly like a real RPN calculator.
//
// Functions are typed by name (letters, Tab-completes) and applied to the
// stack with Enter -- there are no parentheses in RPN, arguments already
// live on the stack:
//   1-arg (X only, e.g. "sqrt" Enter): sin cos tan asin acos atan
//     tanh sinh cosh asinh acosh atanh sqrt cbrt inv sq log ln log2 exp
//     abs floor ceil round int
//   2-arg (pops Y,X -> f(Y,X), e.g. "pow" Enter is the same as the ^ key):
//     pow mod atan2 min max gcd lcm ncr npr randint
//   3-arg (pops Z,Y,X -> f(Z,Y,X)): clamp
//   0-arg (pushes a new value, like typing a number): pi e rand
//
// Memory registers (independent of the X/Y/Z/T stack), 0-9:
//   sto(n)   store X into register n (X is left unchanged)
//   rcl(n)   push register n onto the stack, like typing a new number
//
// Keys:
//   Enter        push the typed number, or apply the typed function name
//   Backspace    delete last character while typing; CLX (clear X) if not
//   fn + Backspace   clear the whole stack
//   fn + ;       roll stack up   (R^, the ; key has an up-arrow icon)
//   fn + .       roll stack down (Rv, the . key has a down-arrow icon)
//   fn + S       swap X <-> Y
//   fn + ,       move the edit cursor left  (, key has a left-arrow icon)
//   fn + /       move the edit cursor right (/ key has a right-arrow icon)
//   opt + D      toggle DEG / RAD angle mode
//   opt + -      change sign (CHS) of the number being typed, or of X
//   Tab          complete the function/command name before the cursor;
//                press again to cycle other matches
//
// Type "help" and press Enter for an on-screen function reference
// (fn+;/. flips pages, Enter or Backspace exits back to the calculator).
//
// The stack (X/Y/Z/T) and the DEG/RAD setting are auto-saved to the
// ESP32's internal flash (NVS) after every calculation, and restored on
// boot -- this works even without an SD card inserted, and survives power
// loss.
//
// Type "save" and press Enter to additionally append a snapshot of the
// current stack to /rpn_log.txt on a microSD card, as plain text you can
// read on a PC.
//
// There's no RTC chip on this hardware, so there's no real clock (or
// calendar) unless you set one. "timeset(H,M,S)" / "dateset(Y,M,D)" each
// set a reference by hand (from millis() elapsed since, independently of
// each other); "time" / "date" show the current computed value. Both
// reset on every power-cycle -- re-run after each boot if you want them.
//
// Alternatively, "wifi(ssid,pass)" saves Wi-Fi credentials to flash and
// immediately syncs both the time and date via NTP (hardcoded to JST);
// "wifi()" retries with the saved credentials (e.g. after a reboot); bare
// "wifi" just shows what's saved. Nothing here ever prompts for Wi-Fi
// automatically -- it's entirely opt-in, and boot/typing/calculating is
// unaffected if unused.
//
// The calculator deep-sleeps after 10 minutes with no key press, or
// immediately if the physical G0/BtnA button (on top of the device) is
// pressed at any time -- this board has no PMIC for a true power-off, so
// deep sleep is the closest equivalent (a few tens of uA instead of a
// full shutdown). The same G0/BtnA button also wakes it back up (not a
// keyboard key -- the whole keyboard is powered down during sleep); if
// Wi-Fi credentials are saved, waking this way also silently retries an
// NTP time sync (a plain power-on never does this on its own). "sleeptime(n)"
// changes the idle timeout to n minutes (0 disables it); bare "sleeptime"
// shows the current setting. The setting is saved to flash and persists
// across power cycles.
//
// Type "usbdrive" and press Enter to expose the microSD card to a computer
// over the same USB-C cable, as an ordinary USB drive -- no card removal
// needed. This takes over the SD card and the USB port for that purpose;
// the calculator only works normally again after a reset/power-cycle. The
// raw SD-over-SPI block I/O routines are adapted from
// MOY-lightening-firmware's "M5-cardputer-mass-storage" (MIT License,
// Copyright (c) 2026 OZAN),
// https://github.com/MOY-lightening-firmware/M5-cardputer-mass-storage
// If "usbdrive" fails on a particular card, "usbdebug" (after a reset)
// shows the last low-level SD error recorded during that attempt.

#include <M5Cardputer.h>
#include <M5GFX.h>
#include <esp_random.h>
#include <Preferences.h>
#include <SPI.h>
#include <SD.h>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
#include <CardputerClock.h>
#include <CardputerUsbDrive.h>
#include <CardputerSleep.h>

static M5Canvas canvas(&M5Cardputer.Display);
static const int LINE_H = 12;    // px per text line at text size 1
static const int REG_LINE_H = 22; // px per register line at text size 2
static const int TOP_Y = 16;      // first register line's y (below the title)

// microSD wiring on both Cardputer and Cardputer ADV (per M5Stack's
// official examples) -- SPI bus is not shared with anything else.
static const int SD_SPI_SCK_PIN = 40;
static const int SD_SPI_MISO_PIN = 39;
static const int SD_SPI_MOSI_PIN = 14;
static const int SD_SPI_CS_PIN = 12;
static const char* SD_LOG_PATH = "/rpn_log.txt";
static const uint32_t SD_SECTOR_SIZE = 512; // must match CardputerUsbDrive's own sector size
static bool sdReady = false;
static uint32_t sdSectorCount = 0; // populated at boot while SD.h has it mounted

static Preferences prefs;
static const char* PREFS_NS = "rpn";

// USB Mass Storage (exposes the microSD card to a host computer as an
// ordinary USB drive) and the underlying raw SD-over-SPI I/O live in the
// shared CardputerUsbDrive library -- see cardputer-common.
static CardputerUsbDrive usbDrive;

// ---------------------------------------------------------------------
// The 4-level HP-style stack
// ---------------------------------------------------------------------
static double regX = 0.0, regY = 0.0, regZ = 0.0, regT = 0.0;
static double mem[10] = {0.0}; // STO/RCL numbered memory registers, independent of the stack
static std::string resultLine;   // status/error line shown below the stack
static bool degMode = false;     // false = radians, true = degrees

// Entry state: what's currently being typed (a number or an identifier),
// not yet applied to the stack.
static std::string entryBuf;
static bool entering = false;
static bool liftedThisEntry = false; // true if a numeric entry lifted the stack at its first char
static bool stackLiftEnabled = true; // classic HP "stack lift" flag
static size_t cursorPos = 0;         // edit cursor within entryBuf

static bool helpMode = false;
static int helpPage = 0;
static const std::vector<std::vector<std::string>> helpPages = {
    {"Entering numbers:", "digits/./- then Enter", "pushes onto the stack.", "Enter on blank line", "duplicates X.", "opt+- (CHS) = sign"},
    {"Operators (no Enter", "needed):", "+ - * / ^ %  pop Y,X", "push f(Y,X)", "!  factorial of X"},
    {"Trig & hyperbolic", "(type name + Enter):", "sin cos tan atan2", "(opt+D toggles deg/rad)", "sinh cosh tanh", "asinh acosh atanh"},
    {"Power/log & compare:", "sqrt cbrt inv sq pow", "log ln log2 exp", "min max gcd lcm", "mod clamp"},
    {"Combinatorics & round:", "ncr npr rand randint", "abs floor ceil round", "int  pi  e"},
    {"Memory registers 0-9:", "sto(n) stores X into", "  register n (X unchanged)", "rcl(n) pushes register n", "ex: 5 Enter sto(0)"},
    {"Stack keys:", "fn+; roll up (Rup)", "fn+. roll down (Rdn)", "fn+S swap X<->Y", "fn+Bksp clear all"},
    {"Saving:", "Stack auto-saves to", "flash (survives power", "off). save+Enter also", "appends to rpn_log.txt", "on a microSD card."},
    {"Clock (no RTC on this", "board, resets each boot):", "timeset(H,M,S) / time", "dateset(Y,M,D) / date", "ex: timeset(9,30,0)"},
    {"USB drive mode:", "usbdrive exposes the SD", "card to a computer over", "USB. Needs reset/power-", "cycle to return.", "usbdebug shows why it", "failed, after a reset."},
    {"Auto-sleep (no PMIC, so", "this is deep sleep):", "sleeptime(n) sets n min", "sleeptime shows current", "G0/BtnA sleeps/wakes;", "wake retries saved wifi"},
    {"Wifi time sync (opt-in,", "never asked automatically):", "wifi(ssid,pass) saves +", "syncs via NTP (JST)", "wifi() retries saved creds"},
    {"Battery & uptime:", "battery = level %/volts", "uptime = time since last", "  boot/wake"},
};

static const size_t MAX_ENTRY_LEN = 100;

// Idle-timeout deep sleep (no PMIC on this board) lives in the shared
// CardputerSleep library -- see cardputer-common.
static const int WAKE_BUTTON_PIN = 0; // G0 / BtnA, the side button
static CardputerSleep sleepMgr;

class EvalError : public std::exception {
public:
    explicit EvalError(const char* m) : msg(m) {}
    const char* what() const noexcept override { return msg; }
private:
    const char* msg;
};

// ---------------------------------------------------------------------
// Stack primitives
// ---------------------------------------------------------------------
static void liftStack() { regT = regZ; regZ = regY; regY = regX; }

// Approximate inverse of liftStack(), used when backspacing a fresh
// numeric entry away entirely. The pre-lift T is gone for good (exactly
// like a real HP stack loses the old T on any lift) -- T just replicates.
static void unliftStack() { regX = regY; regY = regZ; regZ = regT; }

// Replaces the top `consumed` registers with a single result, following
// the classic HP "stack drop" rule: registers above the consumed ones
// shift down, and T replicates (stays put) to fill the gap.
static void dropAndPush(int consumed, double result) {
    if (consumed <= 1) { regX = result; return; }
    if (consumed == 2) { regX = result; regY = regZ; regZ = regT; return; }
    regX = result; regY = regT; regZ = regT; // consumed == 3
}

static void rollUp() {
    double newX = regT, newY = regX, newZ = regY, newT = regZ;
    regX = newX; regY = newY; regZ = newZ; regT = newT;
}

static void rollDown() {
    double newX = regY, newY = regZ, newZ = regT, newT = regX;
    regX = newX; regY = newY; regZ = newZ; regT = newT;
}

static void swapXY() { std::swap(regX, regY); }

// ---------------------------------------------------------------------
// Formatting / parsing helpers
// ---------------------------------------------------------------------
static std::string formatNumber(double v) {
    if (std::isnan(v)) return "ERR";
    if (std::isinf(v)) return v > 0 ? "inf" : "-inf";
    char buf[64];
    snprintf(buf, sizeof(buf), "%.10g", v);
    return std::string(buf);
}

static bool equalsIgnoreCase(const std::string& a, const char* b) {
    size_t i = 0;
    for (; i < a.size() && b[i]; i++) {
        if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i])) return false;
    }
    return i == a.size() && b[i] == '\0';
}

static bool startsWithIgnoreCase(const std::string& a, const char* prefix) {
    size_t i = 0;
    for (; prefix[i]; i++) {
        if (i >= a.size() || std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)prefix[i]))
            return false;
    }
    return true;
}

static bool parseNumeric(const std::string& s, double& out) {
    if (s.empty() || s == "-" || s == ".") return false;
    size_t idx = 0;
    try {
        out = std::stod(s, &idx);
    } catch (...) {
        return false;
    }
    return idx == s.size();
}

static double toRad(double v) { return degMode ? v * M_PI / 180.0 : v; }
static double fromRad(double v) { return degMode ? v * 180.0 / M_PI : v; }

static double gcd2(double a, double b) {
    long long x = std::llround(std::fabs(a));
    long long y = std::llround(std::fabs(b));
    while (y != 0) { long long t = y; y = x % y; x = t; }
    return (double)x;
}

static double lcm2(double a, double b) {
    long long x = std::llround(std::fabs(a));
    long long y = std::llround(std::fabs(b));
    if (x == 0 || y == 0) return 0;
    long long g = (long long)gcd2((double)x, (double)y);
    return (double)(x / g * y);
}

static double nPr(double nd, double rd) {
    long long n = std::llround(nd), r = std::llround(rd);
    if (n < 0 || r < 0 || r > n) throw EvalError("bad nPr");
    double result = 1;
    for (long long i = 0; i < r; i++) result *= (double)(n - i);
    return result;
}

static double nCr(double nd, double rd) {
    long long n = std::llround(nd), r = std::llround(rd);
    if (n < 0 || r < 0 || r > n) throw EvalError("bad nCr");
    if (r > n - r) r = n - r;
    double result = 1;
    for (long long i = 0; i < r; i++) { result *= (double)(n - i); result /= (double)(i + 1); }
    return result;
}

static double factorial(double v) {
    if (v < 0 || v != std::floor(v) || v > 170) throw EvalError("bad factorial");
    double r = 1.0;
    for (int i = 2; i <= (int)v; i++) r *= i;
    return r;
}

// Uniform double in [0, 1) from the ESP32's hardware RNG.
static double randomUnit() { return (double)esp_random() / 4294967296.0; }

// ---------------------------------------------------------------------
// Software clock (no RTC on this hardware) and Wi-Fi/NTP sync live in
// the shared CardputerClock library -- see cardputer-common.
// ---------------------------------------------------------------------
static CardputerClock clock_;

// ---------------------------------------------------------------------
// Persistence: stack + settings auto-saved to internal flash (NVS), and
// an explicit text export to a microSD card.
// ---------------------------------------------------------------------
static void saveStateToFlash() {
    prefs.begin(PREFS_NS, false);
    prefs.putBool("deg", degMode);
    prefs.putDouble("x", regX);
    prefs.putDouble("y", regY);
    prefs.putDouble("z", regZ);
    prefs.putDouble("t", regT);
    for (int i = 0; i < 10; i++) {
        char key[4];
        snprintf(key, sizeof(key), "m%d", i);
        prefs.putDouble(key, mem[i]);
    }
    prefs.end();
}

static void loadStateFromFlash() {
    prefs.begin(PREFS_NS, true);
    degMode = prefs.getBool("deg", false);
    regX = prefs.getDouble("x", 0.0);
    regY = prefs.getDouble("y", 0.0);
    regZ = prefs.getDouble("z", 0.0);
    regT = prefs.getDouble("t", 0.0);
    for (int i = 0; i < 10; i++) {
        char key[4];
        snprintf(key, sizeof(key), "m%d", i);
        mem[i] = prefs.getDouble(key, 0.0);
    }
    prefs.end();
}

// Appends a snapshot of the current stack to /rpn_log.txt on the SD card
// as a labeled block, so re-running "save" doesn't overwrite older saves.
static bool saveStackToSD() {
    if (!sdReady) return false;
    File f = SD.open(SD_LOG_PATH, FILE_APPEND);
    if (!f) return false;

    prefs.begin(PREFS_NS, false);
    uint32_t saveNum = prefs.getUInt("savenum", 0) + 1;
    prefs.putUInt("savenum", saveNum);
    prefs.end();

    std::string tsSuffix;
    if (clock_.isDateSet()) {
        tsSuffix += " @ " + clock_.dateString();
        if (clock_.isTimeSet()) tsSuffix += " " + clock_.timeString();
    } else if (clock_.isTimeSet()) {
        tsSuffix = " @ " + clock_.timeString();
    }
    f.printf("---- save #%u (%s)%s ----\n", (unsigned)saveNum, degMode ? "DEG" : "RAD", tsSuffix.c_str());
    f.printf("T = %s\n", formatNumber(regT).c_str());
    f.printf("Z = %s\n", formatNumber(regZ).c_str());
    f.printf("Y = %s\n", formatNumber(regY).c_str());
    f.printf("X = %s\n", formatNumber(regX).c_str());
    f.println();
    f.close();
    return true;
}

// ---------------------------------------------------------------------
// Entry finalization: turns a pending numeric entry into regX before an
// operator/function consumes it. Returns false (and sets an error) if a
// pending identifier entry can't be used this way.
// ---------------------------------------------------------------------
static bool isIdentifierEntry() {
    return entering && !entryBuf.empty() && std::isalpha((unsigned char)entryBuf[0]);
}

static bool finalizeEntryForOperation() {
    if (!entering) return true;
    if (isIdentifierEntry()) {
        resultLine = "ERR: incomplete token";
        entering = false;
        entryBuf.clear();
        cursorPos = 0;
        return false;
    }
    double v;
    if (!parseNumeric(entryBuf, v)) {
        resultLine = "ERR: bad number";
        entering = false;
        entryBuf.clear();
        cursorPos = 0;
        return false;
    }
    regX = v; // already lifted (or not) when this entry started -- see handleChar()
    entering = false;
    entryBuf.clear();
    cursorPos = 0;
    liftedThisEntry = false;
    return true;
}

static void doUnary(double (*f)(double)) {
    if (!finalizeEntryForOperation()) return;
    try {
        double r = f(regX);
        dropAndPush(1, r);
        resultLine.clear();
    } catch (const std::exception& ex) {
        resultLine = std::string("ERR: ") + ex.what();
    }
    stackLiftEnabled = true;
}

static void doBinary(double (*f)(double, double)) {
    if (!finalizeEntryForOperation()) return;
    try {
        double r = f(regY, regX);
        dropAndPush(2, r);
        resultLine.clear();
    } catch (const std::exception& ex) {
        resultLine = std::string("ERR: ") + ex.what();
    }
    stackLiftEnabled = true;
}

static void doTernary(double (*f)(double, double, double)) {
    if (!finalizeEntryForOperation()) return;
    try {
        double r = f(regZ, regY, regX);
        dropAndPush(3, r);
        resultLine.clear();
    } catch (const std::exception& ex) {
        resultLine = std::string("ERR: ") + ex.what();
    }
    stackLiftEnabled = true;
}

// ---------------------------------------------------------------------
// Named function/command dispatch (invoked on Enter for an identifier
// entry). Math functions transform the stack; utility commands (help,
// save, time, wifi, ...) just set resultLine.
// ---------------------------------------------------------------------
static double f_add(double y, double x) { return y + x; }
static double f_sub(double y, double x) { return y - x; }
static double f_mul(double y, double x) { return y * x; }
static double f_div(double y, double x) { if (x == 0.0) throw EvalError("div by zero"); return y / x; }
static double f_pow(double y, double x) { return std::pow(y, x); }
static double f_fmod(double y, double x) { if (x == 0.0) throw EvalError("div by zero"); return std::fmod(y, x); }
static double f_mod(double y, double x) {
    if (x == 0.0) throw EvalError("div by zero");
    double r = std::fmod(y, x);
    if (r != 0.0 && ((r < 0) != (x < 0))) r += x;
    return r;
}
static double f_sqrt(double x) { if (x < 0) throw EvalError("neg sqrt"); return std::sqrt(x); }
static double f_inv(double x) { if (x == 0.0) throw EvalError("div by zero"); return 1.0 / x; }
static double f_sq(double x) { return x * x; }
static double f_log(double x) { if (x <= 0) throw EvalError("bad log"); return std::log10(x); }
static double f_ln(double x) { if (x <= 0) throw EvalError("bad log"); return std::log(x); }
static double f_log2(double x) { if (x <= 0) throw EvalError("bad log2"); return std::log2(x); }
static double f_asin(double x) { return fromRad(std::asin(x)); }
static double f_acos(double x) { return fromRad(std::acos(x)); }
static double f_atan(double x) { return fromRad(std::atan(x)); }
static double f_sin(double x) { return std::sin(toRad(x)); }
static double f_cos(double x) { return std::cos(toRad(x)); }
static double f_tan(double x) { return std::tan(toRad(x)); }
static double f_acosh(double x) { if (x < 1) throw EvalError("bad acosh"); return std::acosh(x); }
static double f_atanh(double x) { if (x <= -1 || x >= 1) throw EvalError("bad atanh"); return std::atanh(x); }
static double f_atan2(double y, double x) { return fromRad(std::atan2(y, x)); }
static double f_clamp(double x, double lo, double hi) {
    if (lo > hi) std::swap(lo, hi);
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}
static double f_randint(double lo_, double hi_) {
    long long lo = std::llround(lo_), hi = std::llround(hi_);
    if (lo > hi) std::swap(lo, hi);
    long long range = hi - lo + 1;
    long long r = lo + (long long)(randomUnit() * (double)range);
    if (r > hi) r = hi;
    return (double)r;
}

// Result: true if `id` was recognized (and handled), false = unknown token.
static bool evaluateIdentifier(const std::string& id) {
    // 0-arg: pushes a brand-new value, like typing a number then Enter.
    if (equalsIgnoreCase(id, "pi")) { liftStack(); regX = M_PI; stackLiftEnabled = true; resultLine.clear(); return true; }
    if (equalsIgnoreCase(id, "e")) { liftStack(); regX = M_E; stackLiftEnabled = true; resultLine.clear(); return true; }
    if (equalsIgnoreCase(id, "rand")) { liftStack(); regX = randomUnit(); stackLiftEnabled = true; resultLine.clear(); return true; }

    // 1-arg
    if (equalsIgnoreCase(id, "sin")) { doUnary(f_sin); return true; }
    if (equalsIgnoreCase(id, "cos")) { doUnary(f_cos); return true; }
    if (equalsIgnoreCase(id, "tan")) { doUnary(f_tan); return true; }
    if (equalsIgnoreCase(id, "asin")) { doUnary(f_asin); return true; }
    if (equalsIgnoreCase(id, "acos")) { doUnary(f_acos); return true; }
    if (equalsIgnoreCase(id, "atan")) { doUnary(f_atan); return true; }
    if (equalsIgnoreCase(id, "sinh")) { doUnary(static_cast<double(*)(double)>(std::sinh)); return true; }
    if (equalsIgnoreCase(id, "cosh")) { doUnary(static_cast<double(*)(double)>(std::cosh)); return true; }
    if (equalsIgnoreCase(id, "tanh")) { doUnary(static_cast<double(*)(double)>(std::tanh)); return true; }
    if (equalsIgnoreCase(id, "asinh")) { doUnary(static_cast<double(*)(double)>(std::asinh)); return true; }
    if (equalsIgnoreCase(id, "acosh")) { doUnary(f_acosh); return true; }
    if (equalsIgnoreCase(id, "atanh")) { doUnary(f_atanh); return true; }
    if (equalsIgnoreCase(id, "sqrt")) { doUnary(f_sqrt); return true; }
    if (equalsIgnoreCase(id, "inv")) { doUnary(f_inv); return true; }
    if (equalsIgnoreCase(id, "sq")) { doUnary(f_sq); return true; }
    if (equalsIgnoreCase(id, "cbrt")) { doUnary(static_cast<double(*)(double)>(std::cbrt)); return true; }
    if (equalsIgnoreCase(id, "log")) { doUnary(f_log); return true; }
    if (equalsIgnoreCase(id, "ln")) { doUnary(f_ln); return true; }
    if (equalsIgnoreCase(id, "log2")) { doUnary(f_log2); return true; }
    if (equalsIgnoreCase(id, "exp")) { doUnary(static_cast<double(*)(double)>(std::exp)); return true; }
    if (equalsIgnoreCase(id, "abs")) { doUnary(static_cast<double(*)(double)>(std::fabs)); return true; }
    if (equalsIgnoreCase(id, "floor")) { doUnary(static_cast<double(*)(double)>(std::floor)); return true; }
    if (equalsIgnoreCase(id, "ceil")) { doUnary(static_cast<double(*)(double)>(std::ceil)); return true; }
    if (equalsIgnoreCase(id, "round")) { doUnary(static_cast<double(*)(double)>(std::round)); return true; }
    if (equalsIgnoreCase(id, "int")) { doUnary(static_cast<double(*)(double)>(std::trunc)); return true; }
    if (equalsIgnoreCase(id, "fact")) { doUnary(factorial); return true; }

    // 2-arg: f(Y, X)
    if (equalsIgnoreCase(id, "pow")) { doBinary(f_pow); return true; }
    if (equalsIgnoreCase(id, "mod")) { doBinary(f_mod); return true; }
    if (equalsIgnoreCase(id, "atan2")) { doBinary(f_atan2); return true; }
    if (equalsIgnoreCase(id, "min")) { doBinary(static_cast<double(*)(double, double)>([](double a, double b){ return std::min(a, b); })); return true; }
    if (equalsIgnoreCase(id, "max")) { doBinary(static_cast<double(*)(double, double)>([](double a, double b){ return std::max(a, b); })); return true; }
    if (equalsIgnoreCase(id, "gcd")) { doBinary(gcd2); return true; }
    if (equalsIgnoreCase(id, "lcm")) { doBinary(lcm2); return true; }
    if (equalsIgnoreCase(id, "ncr")) { doBinary(nCr); return true; }
    if (equalsIgnoreCase(id, "npr")) { doBinary(nPr); return true; }
    if (equalsIgnoreCase(id, "randint")) { doBinary(f_randint); return true; }

    // 3-arg: f(Z, Y, X)
    if (equalsIgnoreCase(id, "clamp")) { doTernary(f_clamp); return true; }

    // STO(n)/RCL(n): numbered memory registers 0-9, independent of the
    // stack -- STO leaves X untouched, RCL pushes like a new number.
    if ((startsWithIgnoreCase(id, "sto(") || startsWithIgnoreCase(id, "rcl(")) &&
        !id.empty() && id.back() == ')') {
        std::string inner = id.substr(4, id.size() - 5);
        size_t a = inner.find_first_not_of(' ');
        size_t b = inner.find_last_not_of(' ');
        if (a != std::string::npos) inner = inner.substr(a, b - a + 1);
        bool isStore = startsWithIgnoreCase(id, "sto(");
        if (inner.size() != 1 || !std::isdigit((unsigned char)inner[0])) {
            resultLine = isStore ? "ERR: sto(0-9)" : "ERR: rcl(0-9)";
            return true;
        }
        int n = inner[0] - '0';
        if (isStore) {
            mem[n] = regX;
            char buf[24];
            snprintf(buf, sizeof(buf), "M%d = %s", n, formatNumber(regX).c_str());
            resultLine = buf;
        } else {
            liftStack();
            regX = mem[n];
            stackLiftEnabled = true;
            resultLine.clear();
        }
        return true;
    }

    return false; // not a math function -- caller checks utility commands next
}

// ---------------------------------------------------------------------
// Utility (non-math) commands, unchanged in spirit from the algebraic
// calculator: identified by exact/prefix text match on the entry buffer.
// ---------------------------------------------------------------------
static bool evaluateUtilityCommand(const std::string& cmd) {
    if (equalsIgnoreCase(cmd, "help")) {
        helpMode = true;
        helpPage = 0;
        return true;
    }
    if (equalsIgnoreCase(cmd, "save")) {
        bool ok = saveStackToSD();
        resultLine = ok ? ("Saved to " + std::string(SD_LOG_PATH)) : "SD ERR (no card?)";
        return true;
    }
    if (equalsIgnoreCase(cmd, "time")) {
        resultLine = clock_.isTimeSet() ? clock_.timeString() : "Time not set (timeset(H,M,S))";
        return true;
    }
    if (startsWithIgnoreCase(cmd, "timeset(") && !cmd.empty() && cmd.back() == ')') {
        int h, m, s;
        if (CardputerClock::parseThreeIntArgs(cmd, h, m, s) && clock_.setTime(h, m, s)) {
            resultLine = "Time set to " + clock_.timeString();
        } else {
            resultLine = "ERR: timeset(H,M,S) 0-23,0-59,0-59";
        }
        return true;
    }
    if (equalsIgnoreCase(cmd, "date")) {
        resultLine = clock_.isDateSet() ? clock_.dateString() : "Date not set (dateset(Y,M,D))";
        return true;
    }
    if (startsWithIgnoreCase(cmd, "dateset(") && !cmd.empty() && cmd.back() == ')') {
        int y, m, d;
        if (CardputerClock::parseThreeIntArgs(cmd, y, m, d) && clock_.setDate(y, m, d)) {
            resultLine = "Date set to " + clock_.dateString();
        } else {
            resultLine = "ERR: dateset(Y,M,D) e.g. dateset(2026,9,13)";
        }
        return true;
    }
    if (equalsIgnoreCase(cmd, "wifi")) {
        resultLine = clock_.hasSavedWifi() ? ("saved: " + clock_.savedSsid() + " (wifi() to sync)") : "no wifi saved (wifi(ssid,pass))";
        return true;
    }
    if (startsWithIgnoreCase(cmd, "wifi(") && !cmd.empty() && cmd.back() == ')') {
        size_t open = cmd.find('(');
        size_t close = cmd.rfind(')');
        std::string inner = cmd.substr(open + 1, close - open - 1);
        if (inner.empty()) {
            if (!clock_.hasSavedWifi()) {
                resultLine = "ERR: no saved wifi (use wifi(ssid,pass))";
            } else {
                resultLine = clock_.wifiRetry() ? ("Time synced: " + clock_.timeString())
                                                 : "Wifi/NTP failed (time unchanged)";
            }
        } else {
            size_t comma = inner.find(',');
            if (comma == std::string::npos) {
                resultLine = "ERR: wifi(ssid,pass)";
            } else {
                std::string ssid = inner.substr(0, comma);
                std::string pass = inner.substr(comma + 1);
                resultLine = clock_.wifiSync(ssid, pass) ? ("Time synced: " + clock_.timeString())
                                                          : "Saved. Wifi/NTP failed (time unchanged)";
            }
        }
        return true;
    }
    if (equalsIgnoreCase(cmd, "usbdrive")) {
        usbDrive.setSdInfo(sdReady, sdSectorCount);
        if (!usbDrive.enter()) {
            resultLine = "SD ERR (no card?)";
        }
        return true;
    }
    if (equalsIgnoreCase(cmd, "usbdebug")) {
        resultLine = CardputerUsbDrive::lastDebugInfo();
        return true;
    }
    if (equalsIgnoreCase(cmd, "sleeptime")) {
        char buf[48];
        uint32_t mins = sleepMgr.idleMinutes();
        if (mins == 0) snprintf(buf, sizeof(buf), "auto-sleep disabled");
        else snprintf(buf, sizeof(buf), "auto-sleep after %lu min", (unsigned long)mins);
        resultLine = buf;
        return true;
    }
    if (startsWithIgnoreCase(cmd, "sleeptime(") && !cmd.empty() && cmd.back() == ')') {
        size_t open = cmd.find('(');
        size_t close = cmd.rfind(')');
        std::string inner = (open != std::string::npos && close != std::string::npos && close > open)
                                 ? cmd.substr(open + 1, close - open - 1)
                                 : "";
        size_t a = inner.find_first_not_of(' ');
        bool valid = a != std::string::npos;
        if (valid) {
            size_t b = inner.find_last_not_of(' ');
            inner = inner.substr(a, b - a + 1);
        }
        for (char c : inner)
            if (!std::isdigit((unsigned char)c)) valid = false;
        long n = valid && !inner.empty() ? atol(inner.c_str()) : -1;
        if (valid && n >= 0 && n <= 1440 && sleepMgr.setIdleMinutes((uint32_t)n)) {
            char buf[48];
            uint32_t mins = sleepMgr.idleMinutes();
            if (mins == 0) snprintf(buf, sizeof(buf), "auto-sleep disabled");
            else snprintf(buf, sizeof(buf), "auto-sleep after %lu min", (unsigned long)mins);
            resultLine = buf;
        } else {
            resultLine = "ERR: sleeptime(0-1440)";
        }
        return true;
    }
    if (equalsIgnoreCase(cmd, "battery")) {
        int32_t level = M5Cardputer.Power.getBatteryLevel();
        int16_t mv = M5Cardputer.Power.getBatteryVoltage();
        char buf[48];
        if (level < 0) snprintf(buf, sizeof(buf), "battery: unavailable");
        else snprintf(buf, sizeof(buf), "battery: %ld%% (%.2fV)", (long)level, mv / 1000.0f);
        resultLine = buf;
        return true;
    }
    if (equalsIgnoreCase(cmd, "uptime")) {
        uint32_t totalSeconds = millis() / 1000;
        uint32_t days = totalSeconds / 86400;
        int32_t secOfDay = (int32_t)(totalSeconds % 86400);
        char buf[32];
        if (days > 0) snprintf(buf, sizeof(buf), "%lud %s", (unsigned long)days, CardputerClock::formatHMS(secOfDay).c_str());
        else snprintf(buf, sizeof(buf), "%s", CardputerClock::formatHMS(secOfDay).c_str());
        resultLine = buf;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------
// Entry point dispatch: Enter key
// ---------------------------------------------------------------------
static void onEnter() {
    if (!entering) {
        // Bare Enter: duplicate X into Y, and disable stack lift so the
        // very next digit typed overwrites X instead of lifting again.
        liftStack();
        stackLiftEnabled = false;
        resultLine.clear();
        return;
    }

    if (isIdentifierEntry()) {
        std::string id = entryBuf;
        entering = false;
        entryBuf.clear();
        cursorPos = 0;
        if (evaluateIdentifier(id)) {
            saveStateToFlash();
            return;
        }
        if (evaluateUtilityCommand(id)) {
            return; // utility commands never touch the stack
        }
        resultLine = "ERR: unknown '" + id + "'";
        return;
    }

    // Numeric entry: finalize into X (already lifted, or not, at entry
    // start -- see handleChar()).
    double v;
    if (!parseNumeric(entryBuf, v)) {
        resultLine = "ERR: bad number";
    } else {
        regX = v;
        resultLine.clear();
        saveStateToFlash();
    }
    entering = false;
    entryBuf.clear();
    cursorPos = 0;
    stackLiftEnabled = true;
    liftedThisEntry = false;
}

// ---------------------------------------------------------------------
// Character / operator / editing input
// ---------------------------------------------------------------------
static void clearAll() {
    regX = regY = regZ = regT = 0.0;
    entryBuf.clear();
    entering = false;
    cursorPos = 0;
    resultLine.clear();
    stackLiftEnabled = true;
    liftedThisEntry = false;
    saveStateToFlash();
}

static void handleChar(char c) {
    static const std::string allowed = "0123456789.()!,-_ ";
    bool isFuncLetter = std::isalpha((unsigned char)c);
    if (allowed.find(c) == std::string::npos && !isFuncLetter) return;

    if (!entering) {
        entering = true;
        entryBuf.clear();
        cursorPos = 0;
        bool numericStart = std::isdigit((unsigned char)c) || c == '.';
        liftedThisEntry = false;
        if (numericStart && stackLiftEnabled) {
            liftStack();
            liftedThisEntry = true;
        }
        // Identifier entries never lift here -- functions transform the
        // existing stack, they don't push a new value (except the 0-arg
        // ones, which lift explicitly in evaluateIdentifier()).
    }

    if (entryBuf.size() < MAX_ENTRY_LEN) {
        entryBuf.insert(entryBuf.begin() + cursorPos, c);
        cursorPos++;
    }
}

// '-' key is always subtract, exactly like a real RPN calculator -- there
// is no "leading minus" number entry; negative numbers are typed with
// opt+- (CHS) instead. This just finalizes any pending entry (if any)
// and then pops Y,X and pushes Y-X. The one exception is while typing a
// command's text argument (e.g. wifi(my-ssid,pass)), where '-' is just a
// literal character of the SSID/password.
static void handleMinusKey() {
    if (isIdentifierEntry()) { handleChar('-'); return; }
    doBinary(f_sub);
}

static void handleBackspace() {
    if (entering) {
        if (cursorPos > 0) {
            entryBuf.erase(entryBuf.begin() + (cursorPos - 1));
            cursorPos--;
        }
        if (entryBuf.empty()) {
            entering = false;
            if (liftedThisEntry) unliftStack();
            liftedThisEntry = false;
            stackLiftEnabled = true;
        }
        return;
    }
    // CLX: clear X, and the next digit typed overwrites it (no lift).
    regX = 0.0;
    stackLiftEnabled = false;
}

// opt+- : change sign (CHS)
static void handleChs() {
    if (entering) {
        if (isIdentifierEntry()) return; // no sign concept for a function name
        if (!entryBuf.empty() && entryBuf[0] == '-') {
            entryBuf.erase(entryBuf.begin());
            if (cursorPos > 0) cursorPos--;
        } else {
            entryBuf.insert(entryBuf.begin(), '-');
            cursorPos++;
        }
        return;
    }
    regX = -regX;
    saveStateToFlash();
}

static void cursorLeft() { if (entering && cursorPos > 0) cursorPos--; }
static void cursorRight() { if (entering && cursorPos < entryBuf.size()) cursorPos++; }

// Names Tab-completion offers: every math function plus the utility
// commands. None of them take an auto-appended "(" except timeset/dateset/
// sto/rcl, which are the only ones that need a typed argument right after
// the name (everything else, math functions included, takes its arguments
// from the stack, not from parentheses).
static const std::vector<std::string> FUNCTION_NAMES = {
    "pi", "e", "rand", "randint",
    "help", "save", "time", "timeset", "date", "dateset",
    "usbdrive", "usbdebug", "sleeptime", "wifi", "battery", "uptime",
    "sto", "rcl",
    "sin", "cos", "tan", "asin", "acos", "atan", "atan2",
    "tanh", "sinh", "cosh", "asinh", "acosh", "atanh",
    "sqrt", "cbrt", "inv", "sq", "pow", "exp", "log", "ln", "log2",
    "abs", "floor", "ceil", "round", "int", "fact",
    "mod", "min", "max", "clamp", "gcd", "lcm", "ncr", "npr",
};

static bool isBareWord(const std::string& w) {
    return w != "timeset" && w != "dateset" && w != "sto" && w != "rcl";
}

static bool tabActive = false;
static std::vector<std::string> tabCandidates;
static size_t tabCandidateIdx = 0;

static void handleTab() {
    // Tab only completes the bare function/command name -- once "(" has
    // been typed (e.g. mid-way through "timeset(9,"), there's nothing
    // left to complete.
    if (!entering || isIdentifierEntry() == false) return;
    if (entryBuf.find('(') != std::string::npos) return;

    if (!tabActive) {
        std::vector<std::string> matches;
        for (auto& name : FUNCTION_NAMES) {
            if (name.size() >= entryBuf.size() && name.compare(0, entryBuf.size(), entryBuf) == 0) {
                matches.push_back(name);
            }
        }
        if (matches.empty()) return;
        tabCandidates = matches;
        tabCandidateIdx = 0;
        tabActive = true;
    } else {
        tabCandidateIdx = (tabCandidateIdx + 1) % tabCandidates.size();
    }

    const std::string& chosen = tabCandidates[tabCandidateIdx];
    entryBuf = chosen;
    cursorPos = entryBuf.size();

    if (!isBareWord(chosen)) {
        entryBuf += '(';
        cursorPos++;
    }
}

// ---------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------
static void renderUsbDriveScreen() {
    canvas.fillSprite(TFT_BLACK);
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_GREEN, TFT_BLACK);
    canvas.setCursor(2, 2);
    canvas.print("USB Mass Storage");
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);
    canvas.setCursor(2, TOP_Y);
    canvas.print("SD card exposed over USB.");
    canvas.setCursor(2, TOP_Y + LINE_H);
    canvas.print("Find it as a drive on your");
    canvas.setCursor(2, TOP_Y + 2 * LINE_H);
    canvas.print("computer.");
    canvas.setTextColor(TFT_YELLOW, TFT_BLACK);
    canvas.setCursor(2, TOP_Y + 4 * LINE_H);
    canvas.print("Reset/power-cycle to return");
    canvas.setCursor(2, TOP_Y + 5 * LINE_H);
    canvas.print("to the calculator.");
    canvas.pushSprite(0, 0);
}

static void renderRegisterLine(int y, const char* label, const std::string& text, uint16_t color) {
    canvas.setTextSize(2);
    canvas.setTextColor(color, TFT_BLACK);
    canvas.setCursor(2, y);
    canvas.print(label);
    canvas.print(text.c_str());
}

static void render() {
    if (usbDrive.isActive()) {
        renderUsbDriveScreen();
        return;
    }

    int h = canvas.height();
    canvas.fillSprite(TFT_BLACK);

    if (helpMode) {
        canvas.setTextSize(1);
        canvas.setTextColor(TFT_GREEN, TFT_BLACK);
        canvas.setCursor(2, 2);
        canvas.printf("Help  page %d/%d", helpPage + 1, (int)helpPages.size());

        canvas.setTextColor(TFT_WHITE, TFT_BLACK);
        int hy = TOP_Y;
        for (auto& line : helpPages[helpPage]) {
            canvas.setCursor(2, hy);
            canvas.print(line.c_str());
            hy += LINE_H;
        }

        canvas.setTextColor(TFT_DARKGREY, TFT_BLACK);
        canvas.setCursor(2, h - LINE_H - 2);
        canvas.print("fn+;/. page   Enter/BkSp exit");

        canvas.pushSprite(0, 0);
        return;
    }

    canvas.setTextSize(1);
    canvas.setTextColor(TFT_GREEN, TFT_BLACK);
    canvas.setCursor(2, 2);
    canvas.printf("RPN Calc  [%s]", degMode ? "DEG" : "RAD");

    int y = TOP_Y;
    renderRegisterLine(y, "T ", formatNumber(regT), TFT_DARKGREY); y += REG_LINE_H;
    renderRegisterLine(y, "Z ", formatNumber(regZ), TFT_DARKGREY); y += REG_LINE_H;
    renderRegisterLine(y, "Y ", formatNumber(regY), TFT_DARKGREY); y += REG_LINE_H;

    // X is the "live" register: while typing, it shows the entry buffer
    // (with a cursor) instead of the committed numeric value.
    if (entering) {
        std::string shown = entryBuf.substr(0, cursorPos) + "|" + entryBuf.substr(cursorPos);
        renderRegisterLine(y, "X ", shown, TFT_WHITE);
    } else {
        renderRegisterLine(y, "X ", formatNumber(regX), TFT_WHITE);
    }
    y += REG_LINE_H;

    canvas.setTextSize(1);
    canvas.setTextColor(resultLine.rfind("ERR", 0) == 0 ? TFT_RED : TFT_YELLOW, TFT_BLACK);
    canvas.setCursor(2, y + 2);
    canvas.print(resultLine.c_str());

    canvas.pushSprite(0, 0);
}

// ---------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------
static bool wordHas(const Keyboard_Class::KeysState& s, char c) {
    return std::find(s.word.begin(), s.word.end(), c) != s.word.end();
}

void setup() {
    Serial.begin(115200);
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setBrightness(80);
    canvas.setColorDepth(8);
    canvas.createSprite(M5Cardputer.Display.width(), M5Cardputer.Display.height());
    canvas.setTextFont(1);

    loadStateFromFlash(); // restore stack + DEG/RAD from before power-off
    clock_.begin();       // restore saved Wi-Fi credentials (not the clock itself)
    sleepMgr.begin(WAKE_BUTTON_PIN);

    // SD card is optional: the calculator works fine without one, "save"
    // just reports an error until a card is present.
    SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);
    sdReady = SD.begin(SD_SPI_CS_PIN, SPI, 25000000);
    if (sdReady) sdSectorCount = (uint32_t)(SD.totalBytes() / SD_SECTOR_SIZE);
    usbDrive.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN,
                   "RpnCard", "Cardputer", "1.0", "CardputerRpn", "SD Card");

    // Only right after waking from deep sleep via the G0 button -- never on
    // a plain power-on -- silently retry a saved Wi-Fi sync, since that's
    // exactly when the software clock has just been wiped.
    if (sleepMgr.wokenByButton() && clock_.hasSavedWifi()) {
        canvas.fillSprite(TFT_BLACK);
        canvas.setTextColor(TFT_YELLOW, TFT_BLACK);
        canvas.setCursor(2, 2);
        canvas.print("Syncing time via Wi-Fi...");
        canvas.pushSprite(0, 0);
        clock_.wifiRetry(); // best-effort; failure just leaves the clock unset
    }

    render();
}

void loop() {
    if (usbDrive.isActive()) {
        // The SD card now belongs entirely to the raw MSC callbacks; do
        // nothing at all here, not even redraw the display -- the display
        // is also SPI, and any activity on it while a raw SD transaction
        // from the host is in flight was corrupting reads (this was the
        // actual bug behind read failures during mount, in the sibling
        // algebraic-calculator project).
        delay(1000);
        return;
    }

    if (sleepMgr.idleTimeoutReached()) {
        sleepMgr.enterDeepSleep(saveStateToFlash); // never returns
    }

    // G0/BtnA also works as an immediate manual sleep button, not just as
    // the wake source: press it any time to skip the idle timeout.
    if (sleepMgr.buttonPressedEdge()) {
        sleepMgr.enterDeepSleep(saveStateToFlash); // never returns
    }

    M5Cardputer.update();

    if (M5Cardputer.Keyboard.isChange()) {
        if (M5Cardputer.Keyboard.isPressed()) {
            sleepMgr.noteActivity();
            Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();

            // This library's `fn`/`opt` are plain modifier flags: holding
            // either still lets the pressed key populate `word` normally,
            // so combos are detected by checking the flag alongside `word`.
            bool optD = status.opt && (wordHas(status, 'd') || wordHas(status, 'D'));
            bool optChs = status.opt && wordHas(status, '-');
            bool fnUp = status.fn && wordHas(status, ';');
            bool fnDown = status.fn && wordHas(status, '.');
            bool fnLeft = status.fn && wordHas(status, ',');
            bool fnRight = status.fn && wordHas(status, '/');
            bool fnSwap = status.fn && (wordHas(status, 's') || wordHas(status, 'S'));

            if (helpMode) {
                // While the help screen is up, fn+;/. flips pages and
                // Enter/Backspace returns to the calculator; everything
                // else is ignored.
                if (fnUp) {
                    helpPage = (helpPage + (int)helpPages.size() - 1) % (int)helpPages.size();
                } else if (fnDown) {
                    helpPage = (helpPage + 1) % (int)helpPages.size();
                } else if (status.enter || status.del) {
                    helpMode = false;
                }
                render();
                return;
            }

            if (status.tab) {
                handleTab();
                render();
                return;
            }
            tabActive = false; // any other key ends a completion cycle

            if (status.fn && status.del) {
                clearAll();
            } else if (fnUp) {
                rollUp();
            } else if (fnDown) {
                rollDown();
            } else if (fnSwap) {
                swapXY();
            } else if (fnLeft) {
                cursorLeft();
            } else if (fnRight) {
                cursorRight();
            } else if (optChs) {
                handleChs();
            } else if (optD) {
                degMode = !degMode;
                saveStateToFlash();
            } else if (status.del) {
                handleBackspace();
            } else if (status.enter) {
                onEnter();
            } else {
                for (char c : status.word) {
                    if (c == '+') { doBinary(f_add); }
                    else if (c == '-') { handleMinusKey(); }
                    else if (c == '*') { doBinary(f_mul); }
                    else if (c == '/') { doBinary(f_div); }
                    else if (c == '^') { doBinary(f_pow); }
                    else if (c == '%') { doBinary(f_fmod); }
                    else if (c == '!') { doUnary(factorial); }
                    else { handleChar(c); }
                }
            }
            render();
        }
    }
}
