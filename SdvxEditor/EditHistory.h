#pragma once
#include <vector>
#include <cstddef>
#include <algorithm>
#include "ChartManager.h"

namespace SdvxEditor
{
    enum class CommandType
    {
        AddBT, RemoveBT,
        AddFX, RemoveFX,
        ModifyBT, ModifyFX,
        AddLaser, RemoveLaser, ModifyLaser
    };

    struct EditCommand
    {
        CommandType type;
        int          lane       = 0;
        kson::Pulse  pulse      = 0;

        // BT / FX note storage (before = undo state, after = redo state)
        kson::Interval noteBefore{};
        kson::Interval noteAfter{};

        // Laser section storage
        std::shared_ptr<kson::LaserSection> laserBefore{};
        std::shared_ptr<kson::LaserSection> laserAfter{};
    };

    class EditHistory
    {
    public:
        static constexpr std::size_t kMaxHistory = 200;

        void execute(EditCommand cmd, ChartManager& chart)
        {
            applyCommand(cmd, chart, true);
            if (cursor_ < history_.size())
                history_.erase(
                    history_.begin() + static_cast<std::ptrdiff_t>(cursor_),
                    history_.end());
            history_.push_back(cmd);
            if (history_.size() > kMaxHistory)
                history_.erase(history_.begin());
            else
                ++cursor_;
        }

        bool canUndo() const noexcept { return cursor_ > 0; }
        bool canRedo() const noexcept { return cursor_ < history_.size(); }

        void undo(ChartManager& chart)
        {
            if (!canUndo()) return;
            --cursor_;
            applyCommand(history_[cursor_], chart, false);
        }

        void redo(ChartManager& chart)
        {
            if (!canRedo()) return;
            applyCommand(history_[cursor_], chart, true);
            ++cursor_;
        }

        void clear() { history_.clear(); cursor_ = 0; }

    private:
        std::vector<EditCommand> history_;
        std::size_t cursor_{ 0 };

        static void applyCommand(const EditCommand& c, ChartManager& chart, bool forward)
        {
            switch (c.type)
            {
            case CommandType::AddBT:
            {
                auto& lane = chart.chart().note.bt[static_cast<size_t>(c.lane)];
                if (forward) lane.insert_or_assign(c.pulse, c.noteAfter);
                else         lane.erase(c.pulse);
                break;
            }
            case CommandType::RemoveBT:
            {
                auto& lane = chart.chart().note.bt[static_cast<size_t>(c.lane)];
                if (forward) lane.erase(c.pulse);
                else         lane.insert_or_assign(c.pulse, c.noteBefore);
                break;
            }
            case CommandType::AddFX:
            {
                auto& lane = chart.chart().note.fx[static_cast<size_t>(c.lane)];
                if (forward) lane.insert_or_assign(c.pulse, c.noteAfter);
                else         lane.erase(c.pulse);
                break;
            }
            case CommandType::RemoveFX:
            {
                auto& lane = chart.chart().note.fx[static_cast<size_t>(c.lane)];
                if (forward) lane.erase(c.pulse);
                else         lane.insert_or_assign(c.pulse, c.noteBefore);
                break;
            }
            case CommandType::ModifyBT:
            {
                auto& lane = chart.chart().note.bt[static_cast<size_t>(c.lane)];
                auto it = lane.find(c.pulse);
                if (it != lane.end())
                    it->second = forward ? c.noteAfter : c.noteBefore;
                break;
            }
            case CommandType::ModifyFX:
            {
                auto& lane = chart.chart().note.fx[static_cast<size_t>(c.lane)];
                auto it = lane.find(c.pulse);
                if (it != lane.end())
                    it->second = forward ? c.noteAfter : c.noteBefore;
                break;
            }
            case CommandType::AddLaser:
            {
                auto& lane = chart.chart().note.laser[static_cast<size_t>(c.lane)];
                if (forward && c.laserAfter)  lane.insert_or_assign(c.pulse, *c.laserAfter);
                else                          lane.erase(c.pulse);
                break;
            }
            case CommandType::RemoveLaser:
            {
                auto& lane = chart.chart().note.laser[static_cast<size_t>(c.lane)];
                if (forward)                   lane.erase(c.pulse);
                else if (c.laserBefore)        lane.insert_or_assign(c.pulse, *c.laserBefore);
                break;
            }
            case CommandType::ModifyLaser:
            {
                auto& lane = chart.chart().note.laser[static_cast<size_t>(c.lane)];
                auto it = lane.find(c.pulse);
                if (it != lane.end())
                {
                    if (forward  && c.laserAfter)  it->second = *c.laserAfter;
                    if (!forward && c.laserBefore) it->second = *c.laserBefore;
                }
                break;
            }
        }
    };

} // namespace SdvxEditor