#pragma once

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/render/Texture.hpp>
#include <hyprland/src/config/values/types/BoolValue.hpp>
#include <hyprland/src/config/values/types/IntValue.hpp>
#include <hyprland/src/config/values/types/StringValue.hpp>
#include <hyprland/src/config/values/types/ColorValue.hpp>
#include <hyprland/src/config/values/types/FontWeightValue.hpp>
#include "NameStore.hpp"
#include <unordered_set>
#include <hyprland/src/protocols/types/DataDevice.hpp>

inline HANDLE AGENT_TITLES_HANDLE = nullptr;

class CAgentTitleDecoration;

struct SAgentTitlesGlobalState {
    AgentTitles::NameStore names;
    WP<CAgentTitleDecoration> editing;
    std::unordered_set<uint32_t> swallowedKeys;
    std::vector<CHyprSignalListener> listeners;
    SP<IDataSource> clipboard;
    std::vector<WP<CAgentTitleDecoration>> bars;
    uint32_t                  nobarRuleIdx      = 0;
    uint32_t                  barColorRuleIdx   = 0;
    uint32_t                  titleColorRuleIdx = 0;

    struct {
        SP<Config::Values::CColorValue>      barColor, textColor;
        SP<Config::Values::CIntValue>        barHeight;
        SP<Config::Values::CIntValue>        barTextSize;
        SP<Config::Values::CFontWeightValue> barTextWeight;
        SP<Config::Values::CIntValue>        barPadding;
        SP<Config::Values::CBoolValue>       barBlur, barTitleEnabled, barPartOfWindow, barPrecedenceOverBorder, enabled;
        SP<Config::Values::CStringValue>     barTextFont, barTextAlign;
    } config;
};

inline UP<SAgentTitlesGlobalState> g_agentTitlesState;
