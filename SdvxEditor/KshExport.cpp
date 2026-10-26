#include "KshExport.h"
#include <kson/IO/KshIO.hpp>

namespace SdvxEditor {

bool exportKsh(const ChartManager& mgr, const std::filesystem::path& path)
{
    const kson::ErrorType err = kson::SaveKshChartData(path.string(), mgr.chart());
    return err == kson::ErrorType::None;
}

} // namespace SdvxEditor