#pragma once
#include "ChartManager.h"
#include <filesystem>

namespace SdvxEditor {
    // Converts the internal kson chart to .ksh and writes it to path.
    // Returns true on success.
    bool exportKsh(const ChartManager& chart, const std::filesystem::path& path);
}