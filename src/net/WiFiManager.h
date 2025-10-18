#pragma once
#include <Arduino.h>
#include <WiFi.h>

struct WifiCandidate
{
    const char *ssid;
    const char *password;
};

class WiFiManager
{
public:
    // Construct with a list of candidate SSIDs and the periodic scan interval
    WiFiManager(const WifiCandidate *candidates,
                size_t numCandidates,
                uint32_t wifiCheckIntervalMs = 45000);

    // Connect to the "best" candidate at startup (or after a failure).
    // Returns true on success.
    bool begin(uint32_t timeoutMs = 15000);

    // Connect to a specific SSID/password (blocking with timeout). Returns true on success.
    bool connect(const char *ssid, const char *password, uint32_t timeoutMs = 15000);

    // Periodically call this from loop(); it may launch a light scan and decide to switch.
    void checkAndMaybeSwitchWiFi();

    // Call this from each HTTP handler you serve (so we avoid scanning right after traffic)
    inline void markHttpActivity() { _lastHttpAt = millis(); }

    // Read-only helpers
    inline bool isConnected() const { return WiFi.isConnected(); }
    inline IPAddress ip() const { return WiFi.localIP(); }
    inline String currentSsid() const { return WiFi.isConnected() ? WiFi.SSID() : String(); }
    inline int currentRssi() const { return WiFi.isConnected() ? WiFi.RSSI() : -127; }
    inline uint32_t checkIntervalMs() const { return _wifiCheckIntervalMs; }

    // Tunables (public on purpose so you can tweak from app code if needed)
    uint16_t SCAN_MS_PER_CHANNEL = 180;          // per-channel dwell during scan (ms)
    uint32_t QUIET_WINDOW_AFTER_HTTP_MS = 4000;  // skip scans right after HTTP activity
    uint32_t MIN_DWELL_AFTER_SWITCH_MS = 120000; // 2 minutes dwell after switching
    int SWITCH_MARGIN_DB = 8;                    // require this much RSSI improvement
    uint8_t CONSECUTIVE_WINS_REQUIRED = 3;       // best candidate must win N scans in a row

private:
    // Internal helpers
    void startLightScan();
    bool connectToCandidateIndex(int idx, uint32_t timeoutMs = 15000);
    int pickBestCandidateFromScan(int &outBestRssi);
    bool indexOfCurrentCandidate(int &outIdx) const;

    // Members
    const WifiCandidate *_candidates = nullptr;
    size_t _numCandidates = 0;

    uint32_t _wifiCheckIntervalMs = 45000;

    // state
    uint32_t _lastWifiCheck = 0;
    uint32_t _lastSwitchAt = 0;
    uint32_t _lastHttpAt = 0;

    bool _scanning = false;
    uint8_t _bestWinStreak = 0;
    int _lastBestIdx = -1;

    // cached from last scan decision
    int _lastRssiCurrent = -127;
};
