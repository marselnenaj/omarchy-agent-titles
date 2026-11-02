#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/state/WindowState.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/state/MonitorState.hpp>
#include <hyprland/src/managers/SeatManager.hpp>
#include "barDeco.hpp"
#include "globals.hpp"

APICALL EXPORT std::string PLUGIN_API_VERSION() { return HYPRLAND_API_VERSION; }

static void addWindow(PHLWINDOW window) {
    if (!window || !window->m_isMapped || !window->m_monitor || !AgentTitles::terminal(AgentTitles::lower(window->m_class))) return;
    if (std::ranges::any_of(window->m_windowDecorations, [](const auto& decoration) { return decoration->getDisplayName() == "Agent Title"; })) return;
    auto bar = makeUnique<CAgentTitleDecoration>(window);
    g_agentTitlesState->bars.emplace_back(bar);
    bar->m_self = bar;
    HyprlandAPI::addWindowDecoration(AGENT_TITLES_HANDLE, window, std::move(bar));
    g_agentTitlesState->bars.back()->updateAgent();
}

static void updateWindow(PHLWINDOW window) {
    addWindow(window);
    for (const auto& bar : g_agentTitlesState->bars)
        if (bar && bar->getOwner() == window) { bar->updateAgent(); break; }
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    AGENT_TITLES_HANDLE = handle;
    const std::string runtimeHash = __hyprland_api_get_hash();
    const std::string headerHash = __hyprland_api_get_client_hash();
    if (runtimeHash != headerHash)
        throw std::runtime_error("Agent Window Titles: incompatible headers (" + headerHash + ") for compositor (" + runtimeHash + "); rebuild required.");
    g_agentTitlesState = makeUnique<SAgentTitlesGlobalState>();
    g_agentTitlesState->nobarRuleIdx = Desktop::Rule::windowEffects()->registerEffect("agenttitles:no_bar");
    g_agentTitlesState->barColorRuleIdx = Desktop::Rule::windowEffects()->registerEffect("agenttitles:bar_color");
    g_agentTitlesState->titleColorRuleIdx = Desktop::Rule::windowEffects()->registerEffect("agenttitles:title_color");
    auto& c = g_agentTitlesState->config;
    c.barColor = makeShared<Config::Values::CColorValue>("plugin:agenttitles:bar_color", "Title background", 0xff171717);
    c.textColor = makeShared<Config::Values::CColorValue>("plugin:agenttitles:col.text", "Title foreground", 0xffeeeeee);
    c.barHeight = makeShared<Config::Values::CIntValue>("plugin:agenttitles:bar_height", "Title height", 24, Config::Values::SIntValueOptions{.min = 18, .max = 64});
    c.barTextSize = makeShared<Config::Values::CIntValue>("plugin:agenttitles:bar_text_size", "Title font size", 14, Config::Values::SIntValueOptions{.min = 10, .max = 32});
    c.barTextWeight = makeShared<Config::Values::CFontWeightValue>("plugin:agenttitles:bar_text_weight", "Title font weight", 600);
    c.barTitleEnabled = makeShared<Config::Values::CBoolValue>("plugin:agenttitles:bar_title_enabled", "Show title text", true);
    c.barBlur = makeShared<Config::Values::CBoolValue>("plugin:agenttitles:bar_blur", "Blur title background", false);
    c.barTextFont = makeShared<Config::Values::CStringValue>("plugin:agenttitles:bar_text_font", "Title font", "JetBrainsMono Nerd Font");
    c.barTextAlign = makeShared<Config::Values::CStringValue>("plugin:agenttitles:bar_text_align", "Title alignment", "center");
    c.barPartOfWindow = makeShared<Config::Values::CBoolValue>("plugin:agenttitles:bar_part_of_window", "Include title in window decoration", true);
    c.barPrecedenceOverBorder = makeShared<Config::Values::CBoolValue>("plugin:agenttitles:bar_precedence_over_border", "Title outside border", false);
    c.barPadding = makeShared<Config::Values::CIntValue>("plugin:agenttitles:bar_padding", "Title horizontal padding", 10);
    c.enabled = makeShared<Config::Values::CBoolValue>("plugin:agenttitles:enabled", "Enable agent titles", true);
    for (const auto& value : std::vector<SP<Config::Values::IValue>>{
        c.barColor, c.textColor, c.barHeight, c.barTextSize, c.barTextWeight,
        c.barTitleEnabled, c.barBlur, c.barTextFont, c.barTextAlign, c.barPartOfWindow,
        c.barPrecedenceOverBorder, c.barPadding,
        c.enabled}) HyprlandAPI::addConfigValueV2(AGENT_TITLES_HANDLE, value);

    auto& events = Event::bus()->m_events;
    auto& listeners = g_agentTitlesState->listeners;
    listeners.emplace_back(events.window.open.listen([](PHLWINDOW window) { updateWindow(window); }));
    listeners.emplace_back(events.window.title.listen([](PHLWINDOW window) { updateWindow(window); }));
    listeners.emplace_back(events.window.class_.listen([](PHLWINDOW window) { updateWindow(window); }));
    listeners.emplace_back(events.window.updateRules.listen([](PHLWINDOW window) {
        updateWindow(window);
        for (const auto& bar : g_agentTitlesState->bars)
            if (bar && bar->getOwner() == window) { bar->updateRules(); bar->damageEntire(); break; }
    }));
    listeners.emplace_back(events.window.active.listen([](PHLWINDOW window, Desktop::eFocusReason) {
        if (g_agentTitlesState->editing && g_agentTitlesState->editing->getOwner() != window) g_agentTitlesState->editing->finishEditing(true);
    }));
    listeners.emplace_back(events.input.keyboard.key.listen([](IKeyboard::SKeyEvent event, Event::SCallbackInfo& info) {
        auto& swallowed = g_agentTitlesState->swallowedKeys;
        if (event.state == WL_KEYBOARD_KEY_STATE_RELEASED && swallowed.erase(event.keycode)) {
            if (g_agentTitlesState->editing) g_agentTitlesState->editing->releaseKey(event.keycode);
            info.cancelled = true; return;
        }
        if (g_agentTitlesState->editing) g_agentTitlesState->editing->onKey(event, info);
    }));
    listeners.emplace_back(events.config.reloaded.listen([] {
        g_agentTitlesState->names.reload();
        for (const auto& bar : g_agentTitlesState->bars) if (bar) { bar->updateAgent(); bar->updateRules(); bar->onConfigReloaded(); }
    }));
    for (const auto& window : Desktop::windowState()->windows())
        if (window->m_isMapped) addWindow(window);
    HyprlandAPI::reloadConfig();
    return {"agent-titles", "Native editable titles for coding-agent windows", "marselnenaj", "2.0.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    g_agentTitlesState->listeners.clear();
    if (g_agentTitlesState->editing) g_agentTitlesState->editing->finishEditing(false);
    if (g_agentTitlesState->clipboard && g_pSeatManager->m_selection.currentSelection == g_agentTitlesState->clipboard)
        g_pSeatManager->setCurrentSelection(nullptr);
    g_agentTitlesState->clipboard.reset();
    g_pHyprRenderer->m_renderPass.removeAllOfType("CAgentTitlePassElement");
    while (!g_agentTitlesState->bars.empty()) {
        const auto bar = g_agentTitlesState->bars.back();
        if (bar) HyprlandAPI::removeWindowDecoration(AGENT_TITLES_HANDLE, bar.get());
        else g_agentTitlesState->bars.pop_back();
    }
    Desktop::Rule::windowEffects()->unregisterEffect(g_agentTitlesState->barColorRuleIdx);
    Desktop::Rule::windowEffects()->unregisterEffect(g_agentTitlesState->titleColorRuleIdx);
    Desktop::Rule::windowEffects()->unregisterEffect(g_agentTitlesState->nobarRuleIdx);
    for (const auto& monitor : State::monitorState()->monitors()) monitor->m_scheduledRecalc = true;
    g_agentTitlesState.reset();
}
