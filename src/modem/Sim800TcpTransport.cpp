#include "modem/Sim800TcpTransport.h"
#include <string.h>
#include "common/Constants.h"
#include "common/Logger.h"

namespace {
#if defined(SERIAL_DEBUG)
constexpr bool kTcpWantVerbose = true;
#else
constexpr bool kTcpWantVerbose = Sim800Tcp::TCP_VERBOSE_LOG;
#endif
} // namespace

// Narrative TCP lines always log; +IPD/CIPSTART-chatter only when SERIAL_DEBUG or Sim800Tcp::TCP_VERBOSE_LOG.

static bool isControlLine_(const char* s, size_t len) {
    if (!s || len == 0) return true;
    // Trim trailing CR/LF/spaces
    while (len > 0 && (s[len - 1] == '\r' || s[len - 1] == '\n' || s[len - 1] == ' ' || s[len - 1] == '\t')) {
        len--;
    }
    if (len == 0) return true;

    // Fast-path: common modem control lines / AT echo
    if (len >= 2 && s[0] == 'A' && s[1] == 'T') return true;       // "AT...", "ATE1", ...
    if (len == 2 && s[0] == 'O' && s[1] == 'K') return true;        // "OK"
    if (len >= 5 && memcmp(s, "ERROR", 5) == 0) return true;        // "ERROR"
    if (len >= 4 && memcmp(s, "RDY", 3) == 0) return true;          // "RDY"
    if (len >= 7 && memcmp(s, "CLOSED", 6) == 0) return true;       // "CLOSED"
    if (len >= 10 && memcmp(s, "CONNECT OK", 10) == 0) return true; // "CONNECT OK"
    if (len >= 7 && memcmp(s, "SEND OK", 7) == 0) return true;      // "SEND OK"
    if (len >= 9 && memcmp(s, "SEND FAIL", 9) == 0) return true;    // "SEND FAIL"
    if (len >= 1 && s[0] == '>') return true;                       // prompt line (">")

    // URCs typically start with '+'
    if (s[0] == '+') return true;

    return false;
}

void Sim800TcpTransport::reset() {
    _connected = false;
    _connecting = false;
    _sendInProgress = false;
    _ipConfigDone = false;
    _rawLineLen = 0;
    _promptLeak = 0;
    _port = 0;
    _host[0] = '\0';
    _cmdStart[0] = '\0';
    _cmdSend[0] = '\0';
    _rxHead = 0;
    _rxCount = 0;
    _txLen = 0;
    _txSent = 0;
    _ipState = IpState::Idle;
    _ipLen = 0;
    _ipRead = 0;
}

bool Sim800TcpTransport::connectStart(const char* host, uint16_t port) {
    if (!host || !host[0]) return false;
    if (_connecting || _connected) return false;
    strncpy(_host, host, sizeof(_host) - 1);
    _host[sizeof(_host) - 1] = '\0';
    _port = port;
    _connecting = true;
    logger.log("[Sim800Tcp] connectStart host=%s port=%u\n", _host, (unsigned)_port);
    startConnect_();
    return true;
}

void Sim800TcpTransport::stop(const char* reason) {
    const bool wasConnectedOrConnecting = (_connected || _connecting);
    _connected = false;
    _connecting = false;
    _sendInProgress = false;
    _txLen = 0;
    _txSent = 0;
    _promptLeak = 0;
    if (reason && reason[0]) {
        logger.log("[Sim800Tcp] stop(): %s\n", reason);
    } else {
        logger.log("[Sim800Tcp] stop()\n");
    }
    // Best-effort close.
    // IMPORTANT: If modem already emitted URC CLOSED (or connect failed), sending CIPCLOSE/CIP* often yields
    // `+CME ERROR: operation not allowed` and just adds noise. Only try CIPCLOSE when we *think* a socket exists.
    if (wasConnectedOrConnecting && !_closeQueued) {
        _closeQueued = true;
        // CIPMUX=0 (single connection): CIPCLOSE without link id.
        AtSession::Request r{ "AT+CIPCLOSE", 3000, atExpectMask(AtSession::Expect::Ok), nullptr, "CIPCLOSE" };
        (void)_at.enqueue(r);
    }
}

void Sim800TcpTransport::startConnect_() {
    // One-time IP stack policy.
    // RX strategy: push mode (+IPD). Manual CIPRXGET is not reliable across SIM800C firmwares.
    // CIPMUX=0: single socket.
    if (!_ipConfigDone) {
        (void)_at.enqueue({ "AT+CIPRXGET=0", 3000, atExpectMask(AtSession::Expect::Ok), nullptr, "CIPRXGET0" });
        (void)_at.enqueue({ "AT+CIPMUX=0", 3000, atExpectMask(AtSession::Expect::Ok), nullptr, "CIPMUX0" });
        _ipConfigDone = true;
    }

    // CIPMUX=0: CIPSTART without link id.
    snprintf(_cmdStart, sizeof(_cmdStart), "AT+CIPSTART=\"TCP\",\"%s\",%u", _host, (unsigned)_port);
    // We will detect CONNECT OK via URC; here we only wait for OK/ERROR from command acceptance.
    (void)_at.enqueue({ _cmdStart, Sim800Tcp::CIPSTART_ACCEPT_TIMEOUT_MS, atExpectMask(AtSession::Expect::Ok), nullptr, "CIPSTART" });
}

void Sim800TcpTransport::tick(uint32_t nowMs) {
    _lastNowMs = nowMs;
    _at.tick(nowMs);
    // Drain AtSession completions so the queue can progress.
    if (_at.hasResult()) {
        const AtSession::Result r = _at.takeResult();
        if (r.tag && strcmp(r.tag, "CIPCLOSE") == 0) {
            _closeQueued = false;
        }
        if (r.tag && strcmp(r.tag, "CIPSTART") == 0) {
            // Command acceptance; actual connect is signaled via URC CONNECT OK / CONNECT FAIL.
            if (r.timedOut || r.error) {
                logger.log("[Sim800Tcp] CIPSTART accept failed (timeout=%u error=%u)\n",
                           (unsigned)r.timedOut, (unsigned)r.error);
                _connecting = false;
                _connected = false;
            } else if (r.ok) {
                if (kTcpWantVerbose) {
                    logger.log("[Sim800Tcp] CIPSTART accepted (waiting URC)\n");
                }
            }
        } else if (r.tag && strcmp(r.tag, "CIPSEND") == 0) {
            if (r.gotPrompt) {
                // CIPSEND=<len>: send exactly len bytes (no Ctrl+Z in len mode).
                _at.uart().writeBytes(_tx, _txLen);
            } else if (r.timedOut || r.error) {
                _sendInProgress = false;
                _txLen = 0;
                _txSent = 0;
            }
        } else {
            // CIPRXGET0/CIPMUX0/etc: just drain.
        }
    }
    // Sending is driven by prompt + write burst, but we intentionally keep it minimal now:
    // once connected, if tx buffer has data and no send in progress, start send.
    if (_connected && !_sendInProgress && _txLen > 0) {
        startSend_();
    }
}

void Sim800TcpTransport::onLine(const char* line) {
    if (!line || !line[0]) return;
    // Connection URCs
    if (strstr(line, "CONNECT OK") != nullptr) {
        _lastConnectOkMs = _lastNowMs;
        logger.log("[Sim800Tcp] TCP connected\n");
        _connected = true;
        _connecting = false;
        return;
    }
    if (strstr(line, "ALREADY CONNECT") != nullptr) {
        logger.log("[Sim800Tcp] TCP already connected\n");
        _connected = true;
        _connecting = false;
        return;
    }
    if (strcmp(line, "CLOSED") == 0 || strstr(line, "CLOSED") != nullptr) {
        const uint32_t dt = (_lastConnectOkMs != 0) ? (_lastNowMs - _lastConnectOkMs) : 0;
        logger.log("[Sim800Tcp] TCP closed (uptime=%ums)\n", (unsigned)dt);
        _connected = false;
        _connecting = false;
        return;
    }
    if (strstr(line, "CONNECT FAIL") != nullptr || strstr(line, "ERROR") == line) {
        logger.log("[Sim800Tcp] TCP connect failed\n");
        _connected = false;
        _connecting = false;
        return;
    }
    if (strcmp(line, "SEND OK") == 0) {
        // Completed send.
        _sendInProgress = false;
        _txLen = 0;
        _txSent = 0;
        return;
    }
    if (strstr(line, "SEND FAIL") != nullptr) {
        _sendInProgress = false;
        _txLen = 0;
        _txSent = 0;
        return;
    }
}

void Sim800TcpTransport::onByte(char c) {
    // Parse +IPD,<len>:<data> if present (some configs/firmwares output it),
    // otherwise accept raw pushed bytes (CIPRXGET=0) while filtering modem control lines.
    switch (_ipState) {
        case IpState::Idle:
            if (c == '+') _ipState = IpState::MatchI;
            break;
        case IpState::MatchI:
            _ipState = (c == 'I') ? IpState::MatchP : IpState::Idle;
            break;
        case IpState::MatchP:
            _ipState = (c == 'P') ? IpState::MatchD : IpState::Idle;
            break;
        case IpState::MatchD:
            _ipState = (c == 'D') ? IpState::MatchComma : IpState::Idle;
            break;
        case IpState::MatchComma:
            if (c == ',') {
                _ipLen = 0;
                _ipRead = 0;
                _ipState = IpState::ReadLen;
            } else {
                _ipState = IpState::Idle;
            }
            break;
        case IpState::ReadLen:
            // Some firmwares may insert spaces/CRLF around length, be tolerant.
            if (c == ' ' || c == '\r' || c == '\n' || c == '\t') {
                break;
            }
            if (c >= '0' && c <= '9') {
                const uint32_t v = (uint32_t)_ipLen * 10UL + (uint32_t)(c - '0');
                _ipLen = (v > 65535UL) ? 65535 : (uint16_t)v;
            } else if (c == ':') {
                if (kTcpWantVerbose) {
                    logger.log("[Sim800Tcp] +IPD len=%u\n", (unsigned)_ipLen);
                }
                _ipRead = 0;
                _at.uart().setDataMode(true);
                _ipState = IpState::ReadData;
            } else {
                _ipState = IpState::Idle;
            }
            break;
        case IpState::ReadData:
            pushRx_((uint8_t)c);
            if (++_ipRead >= _ipLen) {
                _at.uart().setDataMode(false);
                _ipState = IpState::Idle;
            }
            break;
        default:
            _ipState = IpState::Idle;
            break;
    }

    // If we're in +IPD data mode, raw sniffing must not run (payload may be binary).
    if (_ipState == IpState::ReadData) return;

    // Raw push mode handling (CIPRXGET=0): accept binary stream, but drop CRLF-terminated control lines.
    // Critical: MQTT CONNACK starts with 0x20 (ASCII space). Our older heuristic treated 0x20 as "printable text"
    // and buffered it until '\n', which effectively swallowed CONNACK forever.
    const uint8_t ub = (uint8_t)c;
    const bool printable = (ub == '\r' || ub == '\n' || ub == '\t' || (ub >= 0x20 && ub <= 0x7E));

    // Resolve leaked CIPSEND '>' (see _promptLeak in header): do this before printable/binary split.
    if (_connected && _promptLeak != 0) {
        if (_promptLeak == 1) {
            if (ub == '\r') {
                _promptLeak = 2;
                return;
            }
            if (ub == '\n') {
                _promptLeak = 0;
                return;
            }
            _promptLeak = 0;
            // Fall through: first real TCP byte after a lone '>'.
        } else if (_promptLeak == 2) {
            if (ub == '\n') {
                _promptLeak = 0;
                return;
            }
            _promptLeak = 0;
            // Fall through
        }
    }

    if (!printable) {
        // Binary byte: flush any pending line bytes as payload and pass through.
        _promptLeak = 0;
        for (uint8_t i = 0; i < _rawLineLen; i++) pushRx_((uint8_t)_rawLineBuf[i]);
        _rawLineLen = 0;
        if (_connected) pushRx_(ub);
        return;
    }

    // New prompt leak: '>' must never be line-buffered as modem text (would glue to MQTT bytes).
    if (_connected && _rawLineLen == 0 && ub == '>') {
        _promptLeak = 1;
        return;
    }

    // If we're at the beginning of a "line" and connected, only buffer bytes that plausibly start modem text.
    // Everything else is treated as TCP payload immediately (MQTT frames, HTTP bodies, JSON, etc.).
    if (_connected && _rawLineLen == 0) {
        if (ub == '\r' || ub == '\n') {
            return;
        }
        const bool maybeModemTextStart =
            (ub == '+') || // URCs like +CREG, +CSQ, +IPD...
            (ub == 'A') || // AT echoes
            (ub == 'O') || // OK
            (ub == 'E') || // ERROR
            (ub == 'C') || // CLOSED / CONNECT / Call Ready...
            (ub == 'S') || // SEND OK/FAIL
            (ub == 'R') || // RDY
            (ub == 'N') || // NO CARRIER (rare)
            (ub == 'F');   // FAIL fragments

        if (!maybeModemTextStart) {
            pushRx_(ub);
            return;
        }
    }

    if (_rawLineLen + 1 >= RAW_LINE_BUF_SIZE) {
        // Too long for a control line: treat as payload.
        for (uint8_t i = 0; i < _rawLineLen; i++) pushRx_((uint8_t)_rawLineBuf[i]);
        _rawLineLen = 0;
        if (_connected) pushRx_(ub);
        return;
    }

    _rawLineBuf[_rawLineLen++] = c;
    if (c != '\n') {
        return;
    }

    // Got a CRLF-terminated line candidate.
    if (!isControlLine_(_rawLineBuf, _rawLineLen) && _connected) {
        for (uint8_t i = 0; i < _rawLineLen; i++) pushRx_((uint8_t)_rawLineBuf[i]);
    }
    _rawLineLen = 0;
}

void Sim800TcpTransport::pushRx_(uint8_t b) {
    if (_rxCount >= RX_SIZE) {
        // drop oldest
        _rxHead = (uint16_t)((_rxHead + 1) % RX_SIZE);
        _rxCount--;
    }
    const uint16_t idx = (uint16_t)((_rxHead + _rxCount) % RX_SIZE);
    _rx[idx] = b;
    _rxCount++;
}

int Sim800TcpTransport::available() const {
    return (int)_rxCount;
}

int Sim800TcpTransport::read() {
    if (_rxCount == 0) return -1;
    const uint8_t b = _rx[_rxHead];
    _rxHead = (uint16_t)((_rxHead + 1) % RX_SIZE);
    _rxCount--;
    return (int)b;
}

int Sim800TcpTransport::peek() const {
    if (_rxCount == 0) return -1;
    return (int)_rx[_rxHead];
}

size_t Sim800TcpTransport::write(const uint8_t* data, size_t len) {
    if (!data || len == 0) return 0;
    const size_t room = TX_SIZE - _txLen;
    const size_t n = (len < room) ? len : room;
    memcpy(_tx + _txLen, data, n);
    _txLen += (uint16_t)n;
    return n;
}

void Sim800TcpTransport::startSend_() {
    if (_sendInProgress) return;
    if (_txLen == 0) return;
    // CIPMUX=0: CIPSEND without link id.
    snprintf(_cmdSend, sizeof(_cmdSend), "AT+CIPSEND=%u", (unsigned)_txLen);
    // Expect prompt; when got prompt, bytes will be written directly by Client adapter using write().
    (void)_at.enqueue({ _cmdSend, 5000, atExpectMask(AtSession::Expect::Prompt), nullptr, "CIPSEND" });
    _sendInProgress = true;
    _txSent = 0;
    // NOTE: actual data emission will be handled by Client adapter by writing bytes after prompt.
}

int Sim800ClientAdapter::connect(IPAddress ip, uint16_t port) {
    char host[16];
    snprintf(host, sizeof(host), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    return connect(host, port);
}

int Sim800ClientAdapter::connect(const char* host, uint16_t port) {
    // Non-blocking FSM transport: pump UART while waiting for CONNECT OK URC.
    if (!_t.isConnected() && !_t.isConnecting()) {
        (void)_t.connectStart(host, port);
    }
    const uint32_t t0 = millis();
    while (!_t.isConnected()) {
        pump_();
        if (!_t.isConnecting()) break; // CONNECT FAIL/ERROR/CLOSED
        if (millis() - t0 > Sim800Tcp::CONNECT_TIMEOUT_MS) break;
    }
    return _t.isConnected() ? 1 : 0;
}

size_t Sim800ClientAdapter::write(const uint8_t* buf, size_t size) {
    pump_();
    const size_t n = _t.write(buf, size);
    pump_();
    // If we buffered data, give the modem a chance to enter send state promptly.
    // This reduces the chance of long blocking higher up (e.g. MQTT handshake).
    const uint32_t t0 = millis();
    while (n > 0 && _t.isConnected()) {
        pump_();
        // Stop early if TX buffer drained (transport will clear it on SEND OK).
        // We don't have a direct accessor; heuristic: if no longer connected/connecting, break.
        if (!_t.isConnected()) break;
        if (millis() - t0 > Sim800Tcp::WRITE_PUMP_BUDGET_MS) break; // micro-budget: keep loop responsive
    }
    return n;
}

int Sim800ClientAdapter::read(uint8_t* buf, size_t size) {
    if (!buf || size == 0) return 0;
    pump_();
    size_t n = 0;
    while (n < size) {
        int c = _t.read();
        if (c < 0) break;
        buf[n++] = (uint8_t)c;
    }
    return (int)n;
}
