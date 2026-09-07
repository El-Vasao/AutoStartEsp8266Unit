#pragma once

#include <stddef.h>
#include <stdint.h>

class Core;
struct CorePrivate;

bool coreBoot_pinArrayHasZeroOrDuplicates(const uint16_t* ids, size_t n);
