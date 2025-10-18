#include "WiFiManager.h"

WiFiManager::WiFiManager(const WifiCandidate *candidates,
                         size_t numCandidates,
                         uint32_t wifiCheckIntervalMs)
    : _candidates(candidates),
      _numCandidates(numCandidates),
      _wifiCheckIntervalMs(wifiCheckIntervalMs),
      _lastWifiCheck(0),
      _lastSwitchAt(0),
      _lastHttpAt(0),
      _scanning(false),
      _bestWinStreak(0),
      _lastBestIdx(-1),
      _lastRssiCurrent(-127)
{
}

bool WiFiManager::begin(uint32_t timeoutMs)
{
    // Try to connect to the first candidate that exists in range (quick heuristic)
    // If none are known yet, just try #0; later the scan/switcher will improve it.
    if (_numCandidates == 0)
        return false;

    // If already connected, keep it
    if (WiFi.isConnected())
        return true;

    WiFi.mode(WIFI_STA);
    // Quick attempt to candidate[0]; app can still call connect() explicitly if desired.
    return connect(_candidates[0].ssid, _candidates[0].password, timeoutMs);
}

bool WiFiManager::connect(const char *ssid, const char *password, uint32_t timeoutMs)
{
    WiFi.disconnect(true, true);
    delay(150);
    WiFi.begin(ssid, password);

    Serial.print("[WiFi] Connecting to ");
    Serial.print(ssid);
    Serial.print(" ");

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs)
    {
        Serial.print('.');
        delay(300);
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.print("[WiFi] Connected! IP=");
        Serial.print(WiFi.localIP());
        Serial.print("  SSID=");
        Serial.print(WiFi.SSID());
        Serial.print("  RSSI=");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");
        _lastSwitchAt = millis();
        return true;
    }
    else
    {
        Serial.println("[WiFi] Connect timeout.");
        return false;
    }
}

void WiFiManager::startLightScan()
{
    // async=true, show_hidden=true, passive=true, max_ms_per_chan=SCAN_MS_PER_CHANNEL
    WiFi.scanNetworks(true, true, true, SCAN_MS_PER_CHANNEL);
}

bool WiFiManager::connectToCandidateIndex(int idx, uint32_t timeoutMs)
{
    if (idx < 0 || (size_t)idx >= _numCandidates)
        return false;
    return connect(_candidates[idx].ssid, _candidates[idx].password, timeoutMs);
}

int WiFiManager::pickBestCandidateFromScan(int &outBestRssi)
{
    struct ScanResult
    {
        bool seen;
        int rssi;
    };
    if (_numCandidates == 0)
    {
        outBestRssi = -127;
        return -1;
    }

    // Build result table
    ScanResult *results = (ScanResult *)alloca(sizeof(ScanResult) * _numCandidates);
    for (size_t k = 0; k < _numCandidates; ++k)
    {
        results[k].seen = false;
        results[k].rssi = -127;
    }

    int n = WiFi.scanComplete();
    if (n <= 0)
    {
        outBestRssi = -127;
        return -1;
    }

    for (int i = 0; i < n; ++i)
    {
        String ssid = WiFi.SSID(i);
        int rssi = WiFi.RSSI(i);
        for (size_t k = 0; k < _numCandidates; ++k)
        {
            if (ssid == _candidates[k].ssid)
            {
                results[k].seen = true;
                if (rssi > results[k].rssi)
                    results[k].rssi = rssi;
            }
        }
    }

    // Determine best
    int bestIdx = -1;
    int bestRssi = -127;
    for (size_t k = 0; k < _numCandidates; ++k)
    {
        if (results[k].seen && results[k].rssi > bestRssi)
        {
            bestRssi = results[k].rssi;
            bestIdx = (int)k;
        }
    }
    outBestRssi = bestRssi;
    return bestIdx;
}

bool WiFiManager::indexOfCurrentCandidate(int &outIdx) const
{
    if (!WiFi.isConnected())
        return false;
    String cur = WiFi.SSID();
    for (size_t k = 0; k < _numCandidates; ++k)
    {
        if (cur == _candidates[k].ssid)
        {
            outIdx = (int)k;
            return true;
        }
    }
    return false;
}

void WiFiManager::checkAndMaybeSwitchWiFi()
{
    const uint32_t now = millis();

    // Throttle: only every _wifiCheckIntervalMs
    if (now - _lastWifiCheck < _wifiCheckIntervalMs)
        return;
    _lastWifiCheck = now;

    // Avoid scanning too soon after HTTP traffic
    if (now - _lastHttpAt < QUIET_WINDOW_AFTER_HTTP_MS)
        return;

    // Respect dwell time after a switch
    if (now - _lastSwitchAt < MIN_DWELL_AFTER_SWITCH_MS)
        return;

    // If not already scanning, start
    if (!_scanning)
    {
        startLightScan();
        _scanning = true;
        return; // let it run asynchronously
    }

    // If scanning, see if results are ready
    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING)
    {
        return; // still scanning
    }

    // Finish the scan session
    _scanning = false;

    if (n <= 0)
    {
        WiFi.scanDelete(); // clean up
        // nothing found or failed
        return;
    }

    Serial.println();
    Serial.println("=== WiFi scan (light) result ===");
    Serial.println("SSID\t\tRSSI (dBm)");

    // (Optional) print only candidate SSIDs found
    for (int i = 0; i < n; ++i)
    {
        Serial.print(WiFi.SSID(i));
        Serial.print("\t");
        Serial.println(WiFi.RSSI(i));
    }

    // current stats
    const bool connected = WiFi.isConnected();
    const String ssidCurrent = connected ? WiFi.SSID() : String();
    const int rssiCurrent = connected ? WiFi.RSSI() : -127;
    _lastRssiCurrent = rssiCurrent;

    int bestRssi = -127;
    const int bestIdx = pickBestCandidateFromScan(bestRssi);
    WiFi.scanDelete();

    if (bestIdx < 0)
    {
        Serial.println("No candidate SSIDs found. Staying put.");
        return;
    }

    // If not connected, go to best immediately
    if (!connected)
    {
        Serial.printf("Not connected. Connecting to %s\n", _candidates[bestIdx].ssid);
        if (connectToCandidateIndex(bestIdx))
        {
            _bestWinStreak = 0;
            _lastBestIdx = bestIdx;
        }
        return;
    }

    // Already on a candidate?
    int curIdx = -1;
    const bool onCandidate = indexOfCurrentCandidate(curIdx);
    if (onCandidate && curIdx == bestIdx)
    {
        _bestWinStreak = 0;
        _lastBestIdx = bestIdx;
        Serial.printf("Already on strongest SSID: %s (RSSI %d dBm). No switch.\n",
                      ssidCurrent.c_str(), rssiCurrent);
        return;
    }

    // Margin check
    const int delta = bestRssi - rssiCurrent;
    if (delta >= SWITCH_MARGIN_DB)
    {
        if (_lastBestIdx == bestIdx)
            _bestWinStreak++;
        else
            _bestWinStreak = 1;
        _lastBestIdx = bestIdx;

        Serial.printf("Candidate stronger by %d dB (streak %u/%u)\n",
                      delta, _bestWinStreak, CONSECUTIVE_WINS_REQUIRED);

        if (_bestWinStreak >= CONSECUTIVE_WINS_REQUIRED)
        {
            Serial.printf("Switching to %s (%d dBm)\n",
                          _candidates[bestIdx].ssid, bestRssi);
            if (connectToCandidateIndex(bestIdx))
            {
                _lastSwitchAt = millis();
            }
            _bestWinStreak = 0;
        }
    }
    else
    {
        _bestWinStreak = 0;
        _lastBestIdx = -1;
        Serial.printf("No switch. Improvement %d dB < margin %d\n",
                      delta, SWITCH_MARGIN_DB);
    }
}
