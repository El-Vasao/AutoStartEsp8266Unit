#pragma once

#include <stdint.h>

namespace gsm_at {

bool parseCregStat(const char* line, int8_t& statOut);
bool parseCgattStat(const char* line, int8_t& statOut);
bool isIpv4Line(const char* s);

/// SAPBR line contains quoted IPv4 (e.g. +SAPBR: 1,1,"1.2.3.4").
bool sapbrLineHasQuotedIpv4(const char* line);

} // namespace gsm_at
