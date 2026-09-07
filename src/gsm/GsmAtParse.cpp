#include "gsm/GsmAtParse.h"

#include <stdlib.h>
#include <string.h>

namespace gsm_at {

bool parseCregStat(const char* line, int8_t& statOut) {
    if (!line) return false;
    const char* p = strstr(line, "+CREG:");
    if (!p) return false;
    p += 6;
    while (*p == ' ' || *p == ':') p++;
    char* end = nullptr;
    const long first = strtol(p, &end, 10);
    if (!end || end == p) return false;
    while (*end == ' ') end++;
    long stat = first;
    if (*end == ',') {
        end++;
        while (*end == ' ') end++;
        if (*end == '"') {
            stat = first;
        } else {
            stat = strtol(end, &end, 10);
        }
    }
    if (stat < 0 || stat > 5) return false;
    statOut = (int8_t)stat;
    return true;
}

bool parseCgattStat(const char* line, int8_t& statOut) {
    if (!line) return false;
    const char* p = strstr(line, "+CGATT:");
    if (!p) return false;
    p += 7;
    while (*p == ' ' || *p == ':') p++;
    char* end = nullptr;
    long v = strtol(p, &end, 10);
    if (v != 0 && v != 1) return false;
    statOut = (int8_t)v;
    return true;
}

bool isIpv4Line(const char* s) {
    if (!s) return false;
    int parts = 0;
    int acc = -1;
    for (const char* p = s; *p; p++) {
        const char c = *p;
        if (c >= '0' && c <= '9') {
            int d = c - '0';
            acc = (acc < 0) ? d : (acc * 10 + d);
            if (acc > 255) return false;
        } else if (c == '.') {
            if (acc < 0) return false;
            parts++;
            acc = -1;
        } else if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            break;
        } else {
            return false;
        }
    }
    return (parts == 3 && acc >= 0);
}

bool sapbrLineHasQuotedIpv4(const char* line) {
    if (!line) return false;
    const char* p = strstr(line, "+SAPBR:");
    if (!p) return false;
    const char* q1 = strchr(p, '\"');
    if (!q1) return false;
    q1++;
    const char* q2 = strchr(q1, '\"');
    if (!q2) return false;
    char ip[32];
    const size_t n = (size_t)(q2 - q1);
    if (n == 0 || n >= sizeof(ip)) return false;
    memcpy(ip, q1, n);
    ip[n] = '\0';
    int parts = 0;
    int acc = -1;
    for (const char* s = ip; *s; s++) {
        const char c = *s;
        if (c >= '0' && c <= '9') {
            const int d = c - '0';
            acc = (acc < 0) ? d : (acc * 10 + d);
            if (acc > 255) return false;
        } else if (c == '.') {
            if (acc < 0) return false;
            parts++;
            acc = -1;
        } else {
            return false;
        }
    }
    return (parts == 3 && acc >= 0);
}

} // namespace gsm_at
