#include "mqtt/MqttCommandParser.h"

#include <Arduino.h>
#include <ctype.h>
#include <string.h>

static uint8_t parseU8(const char* v) {
    if (!v || !*v) return 0;
    char* end = nullptr;
    unsigned long n = strtoul(v, &end, 10);
    if (!end || *end != '\0' || n > 255u) return 0;
    return static_cast<uint8_t>(n);
}

enum class Pending : uint8_t { None, Action, Program };

static void skipWs(const char*& p) {
    while (*p && isspace((unsigned char)*p)) p++;
}

/** Read one JSON string token (content only); `p` must point after opening quote. */
static bool readJsonStringContent(const char*& p, char* out, size_t outCap) {
    if (!out || outCap == 0) return false;
    size_t w = 0;
    while (*p && *p != '"') {
        if (*p == '\\') {
            p++;
            if (!*p) return false;
            if (w + 1 >= outCap) return false;
            out[w++] = *p++;
        } else {
            if (w + 1 >= outCap) return false;
            out[w++] = *p++;
        }
    }
    if (*p != '"') return false;
    p++;
    out[w] = '\0';
    return true;
}

/**
 * Flat object: only string keys "action"/"program" and string or integer values.
 * Example: {"action":"run","program":3} or {"action":"list_programs"}.
 */
static bool parseFlatObject(const char* json, char* actionOut, size_t actionCap, uint8_t* programOut) {
    if (!json || !actionOut || actionCap == 0 || !programOut) return false;
    actionOut[0] = '\0';
    *programOut = 0;

    const char* p = json;
    skipWs(p);
    if (*p != '{') return false;
    p++;

    for (;;) {
        skipWs(p);
        if (*p == '}') {
            p++;
            skipWs(p);
            return *p == '\0';
        }
        if (*p != '"') return false;
        p++;

        char keybuf[16];
        if (!readJsonStringContent(p, keybuf, sizeof keybuf)) return false;

        skipWs(p);
        if (*p != ':') return false;
        p++;
        skipWs(p);

        Pending pend = Pending::None;
        if (strcmp(keybuf, "action") == 0) pend = Pending::Action;
        else if (strcmp(keybuf, "program") == 0) pend = Pending::Program;

        if (*p == '"') {
            p++;
            char vbuf[48];
            if (!readJsonStringContent(p, vbuf, sizeof vbuf)) return false;
            if (pend == Pending::Action) strlcpy(actionOut, vbuf, actionCap);
            else if (pend == Pending::Program) *programOut = parseU8(vbuf);
        } else {
            const char* n0 = p;
            if (*p == '-') p++;
            while (*p &&
                   (isdigit((unsigned char)*p) || *p == '.' || *p == 'e' || *p == 'E' || *p == '+' || *p == '-')) {
                p++;
            }
            const size_t nlen = static_cast<size_t>(p - n0);
            char nbuf[24];
            if (nlen >= sizeof nbuf) return false;
            memcpy(nbuf, n0, nlen);
            nbuf[nlen] = '\0';
            if (pend == Pending::Program) *programOut = parseU8(nbuf);
        }

        skipWs(p);
        if (*p == ',') {
            p++;
            continue;
        }
        if (*p == '}') {
            p++;
            skipWs(p);
            return *p == '\0';
        }
        return false;
    }
}

bool parseMqttCommandJson(const char* json, MqttCommand& out) {
    out = MqttCommand{};
    if (!json || !*json) return false;

    char action[24]{};
    uint8_t program = 0;
    if (!parseFlatObject(json, action, sizeof action, &program)) return false;
    if (action[0] == '\0') return false;

    if (strcmp(action, "run") == 0 || strcmp(action, "run_program") == 0) {
        if (program == 0) return false;
        out.kind = MqttCommandKind::RunProgram;
        out.programId = program;
        return true;
    }
    if (strcmp(action, "list_programs") == 0) {
        out.kind = MqttCommandKind::ListPrograms;
        return true;
    }
    return false;
}
