#include "config/Config.h"
#include "fs/FSManager.h"
#include "common/Logger.h"
#include "common/Utils.h"
#include "config/internal/BaseConfigJsonIo.h"
#include "config/internal/ProgramJsonIo.h"
#include "config/internal/ConfigStorageInternal.h"

using namespace config_storage_internal;

namespace {

size_t encodePostedBaseConfig(Print& p, void* ctx) {
    const auto* cfg = reinterpret_cast<const BaseConfig*>(ctx);
    return config_internal::serializeBaseConfigToPrint(*cfg, p);
}

size_t encodePostedProgram(Print& p, void* ctx) {
    const auto* prog = reinterpret_cast<const Program*>(ctx);
    return program_json::emitProgramToPrint(*prog, p);
}

} // namespace

bool Config::applyPostedConfigJsonFile(const char* tmpPath) {
    File f = fileSystem.openRead(tmpPath);
    if (!f) {
        fileSystem.deleteFile(tmpPath);
        return false;
    }
    BaseConfig tmp{};
    const bool parsed = config_internal::parseBaseConfigStreamingFromFile(f, tmp);
    f.close();
    fileSystem.deleteFile(tmpPath);
    if (!parsed) {
        return false;
    }
    if (!validateCross(tmp)) {
        logger.log("[Config] Posted config failed cross-validation\n");
        return false;
    }

    prepareFlashWriteLogGcAndWdt();

    if (!fileSystem.writeJsonAtomicStream("/config.json", encodePostedBaseConfig, &tmp, Limits::CONFIG_JSON_SIZE)) {
        logger.log("[Config] Failed to write config.json\n");
        return false;
    }

    File cf = fileSystem.openRead("/config.json");
    if (cf) {
        configCRC = crc16ModbusStreamFile(cf);
        cf.close();
    }
    logger.log("[Config] Posted config written to /config.json, CRC=%04X\n", configCRC);
    return true;
}

bool Config::commitPostedProgramFile(const char* tmpPath, uint8_t* outId) {
    File f = fileSystem.openRead(tmpPath);
    if (!f) {
        fileSystem.deleteFile(tmpPath);
        return false;
    }
    Program prog{};
    const bool ok = program_json::parseProgramObjectFile(f, prog);
    f.close();
    fileSystem.deleteFile(tmpPath);
    if (!ok || prog.id == 0) {
        return false;
    }
    if (outId) *outId = prog.id;

    char path[BufferBytes::Fs::PROGRAM_PATH];
    snprintf(path, sizeof(path), "/programs/%u.json", prog.id);

    if (!ensureProgramsDir()) return false;

    prepareFlashWriteLogGcAndWdt();

    if (!fileSystem.writeJsonAtomicStream(path, encodePostedProgram, &prog, Limits::CONFIG_JSON_SIZE)) {
        logger.log("[Config] Failed to write program file\n");
        return false;
    }

    logger.log("[Config] Program %u file written\n", prog.id);
    return true;
}
