#include "config/Config.h"
#include "fs/FSManager.h"
#include "common/Logger.h"
#include "common/Utils.h"
#include "config/internal/ProgramJsonIo.h"
#include "config/internal/ConfigStorageInternal.h"

#include <cstring>
#include <stdlib.h>

using namespace config_storage_internal;

namespace {

size_t encodeProgramIndexArray(Print& p, void* ctx) {
    struct Ctx {
        const program_json::IndexRow* rows;
        size_t n;
    };
    auto* box = reinterpret_cast<Ctx*>(ctx);
    return program_json::emitProgramIndexArrayPrint(box->rows, box->n, p);
}

bool loadRowsFromIndexFile(program_json::IndexRow* rows, size_t cap, size_t* outCount) {
    File f = fileSystem.openRead("/programs/index.json");
    if (!f) {
        return false;
    }
    const bool ok = program_json::parseProgramIndexFile(f, rows, cap, outCount);
    f.close();
    return ok;
}

} // namespace

bool Config::emitProgramListWrapped(Print& p) const {
    program_json::IndexRow rows[Limits::MAX_PROGRAMS];
    size_t n = 0;
    File f = fileSystem.openRead("/programs/index.json");
    if (!f) {
        return false;
    }
    const bool parsed = program_json::parseProgramIndexFile(f, rows, Limits::MAX_PROGRAMS, &n);
    f.close();
    if (!parsed) {
        return false;
    }
    program_json::emitProgramListWrappedPrint(rows, n, p);
    return true;
}

bool Config::ensureProgramIndex() {
    bool present[256] = { false };
    bool inIndex[256] = { false };
    size_t filesCount = 0;

    logger.log("[Config] ensureProgramIndex: scanning /programs\n");
    Dir dir = fileSystem.openDir("/programs");
    while (dir.next()) {
        wdtPort_.feedNow();
        wdtPort_.feedNow();
        const String& fn = dir.fileName();
        const char* fns = fn.c_str();
        size_t n = fn.length();
        if (n < 6) continue;
        if (strcmp(fns + (n - 5), ".json") != 0) continue;
        if (n >= 10 && strcmp(fns + (n - 10), "/index.json") == 0) continue;
        if (strcmp(fns, "index.json") == 0) continue;

        const char* base = strrchr(fns, '/');
        base = base ? (base + 1) : fns;
        char* endptr = nullptr;
        long idInt = strtol(base, &endptr, 10);
        if (!endptr || strcmp(endptr, ".json") != 0) continue;
        if (idInt <= 0 || idInt > 255) continue;

        present[(uint8_t)idInt] = true;
        filesCount++;
    }

    bool mismatch = true;
    size_t indexCount = 0;
    program_json::IndexRow rows[Limits::MAX_PROGRAMS]{};

    {
        size_t n = 0;
        if (!loadRowsFromIndexFile(rows, Limits::MAX_PROGRAMS, &n)) {
            logger.log("[Config] ensureProgramIndex: no valid index, rebuilding after releasing JSON pool\n");
            mismatch = true;
        } else {
            mismatch = false;
            indexCount = n;
            for (size_t i = 0; i < n; i++) {
                const uint8_t id = rows[i].id;
                if (id == 0 || id > 255) {
                    mismatch = true;
                    break;
                }
                inIndex[id] = true;
                if (!present[id]) {
                    mismatch = true;
                    break;
                }
            }

            if (!mismatch) {
                for (uint16_t pid = 1; pid <= 255; pid++) {
                    wdtPort_.feedNow();
                    wdtPort_.feedNow();
                    if (present[pid] && !inIndex[pid]) {
                        mismatch = true;
                        break;
                    }
                }
            }
        }
    }

    if (mismatch) {
        logger.log("[Config] ensureProgramIndex: mismatch detected (files=%u, index=%u), rebuilding...\n",
                   (unsigned)filesCount, (unsigned)indexCount);
        return rebuildProgramIndex();
    }

    logger.log("[Config] ensureProgramIndex: OK (files=%u index=%u)\n",
               (unsigned)filesCount, (unsigned)indexCount);
    return true;
}

bool Config::rebuildProgramIndex() {
    logger.log("[Config] rebuildProgramIndex: building /programs/index.json\n");
    if (!ensureProgramsDir()) return false;

    program_json::IndexRow items[Limits::MAX_PROGRAMS]{};
    size_t count = 0;

    logger.log("[Config] rebuildProgramIndex: scanning /programs\n");
    ESP.wdtFeed();
    Dir dir = fileSystem.openDir("/programs");
    while (dir.next()) {
        wdtPort_.feedNow();
        wdtPort_.feedNow();
        const String& fn = dir.fileName();
        const char* fns = fn.c_str();
        size_t n = fn.length();
        if (n < 6) continue;
        if (strcmp(fns + (n - 5), ".json") != 0) continue;
        if (n >= 10 && strcmp(fns + (n - 10), "/index.json") == 0) continue;
        if (strcmp(fns, "index.json") == 0) continue;

        const char* base = strrchr(fns, '/');
        base = base ? (base + 1) : fns;
        char* endptr = nullptr;
        long idInt = strtol(base, &endptr, 10);
        if (!endptr || strcmp(endptr, ".json") != 0) continue;
        if (idInt <= 0 || idInt > 255) continue;
        const uint8_t id = static_cast<uint8_t>(idInt);

        File f = dir.openFile("r");
        if (!f) continue;

        uint8_t rid = 0;
        char name[sizeof program_json::IndexRow::name];
        name[0] = '\0';
        const bool okHdr = program_json::parseProgramHeaderFile(f, id, &rid, name, sizeof name);
        f.close();
        if (!okHdr) {
            logger.log("[Config] rebuildProgramIndex: skip %s (header parse)\n", fns);
            continue;
        }
        const uint8_t realId = rid ? rid : id;
        if (realId != id) {
            logger.log("[Config] rebuildProgramIndex: skip %s (id mismatch %u!=%u)\n",
                       fns, (unsigned)realId, (unsigned)id);
            continue;
        }

        if (count >= Limits::MAX_PROGRAMS) {
            logger.log("[Config] rebuildProgramIndex: too many programs\n");
            break;
        }
        items[count].id = id;
        strlcpy(items[count].name, name, sizeof items[count].name);
        count++;
    }

    ESP.wdtFeed();

    for (size_t i = 0; i + 1 < count; i++) {
        wdtPort_.feedNow();
        wdtPort_.feedNow();
        for (size_t j = i + 1; j < count; j++) {
            if (items[j].id < items[i].id) {
                const program_json::IndexRow tmp = items[i];
                items[i] = items[j];
                items[j] = tmp;
            }
        }
    }

    ESP.wdtFeed();

    struct Ctx {
        const program_json::IndexRow* rows;
        size_t n;
    } ctx{items, count};

    const bool wrote =
        fileSystem.writeJsonAtomicStream("/programs/index.json", encodeProgramIndexArray, &ctx, Limits::CONFIG_JSON_SIZE);
    logger.log("[Config] rebuildProgramIndex: %s (%u items)\n", wrote ? "OK" : "FAILED", (unsigned)count);
    return wrote;
}
