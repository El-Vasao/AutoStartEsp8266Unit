#include "core/internal/CoreBootHelpers.h"

#include <stddef.h>

bool coreBoot_pinArrayHasZeroOrDuplicates(const uint16_t* ids, size_t n) {
    if (!ids || n == 0) return true;
    for (size_t i = 0; i < n; i++) {
        if (ids[i] == 0) return true;
        for (size_t j = i + 1; j < n; j++) {
            if (ids[i] == ids[j]) return true;
        }
    }
    return false;
}

