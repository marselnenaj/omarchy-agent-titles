#include "barDeco.hpp"

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/desktop/state/WindowState.hpp>
#include <hyprland/src/desktop/state/LayerState.hpp>
#include <hyprland/src/desktop/state/ViewHitTester.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/desktop/view/LayerSurface.hpp>
#include <hyprland/src/helpers/MiscFunctions.hpp>
#include <hyprland/src/managers/SeatManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/config/shared/animation/AnimationTree.hpp>
#include <hyprland/src/config/shared/parserUtils/ParserUtils.hpp>
#include <hyprland/src/config/supplementary/executor/Executor.hpp>
#include <hyprland/src/config/shared/actions/ConfigActions.hpp>
#include <hyprland/src/animation/AnimationManager.hpp>
#include <hyprland/src/protocols/LayerShell.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/layout/LayoutManager.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/state/MonitorState.hpp>

#include "globals.hpp"
#include "BarPassElement.hpp"

#include <climits>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <hyprland/src/protocols/types/DataDevice.hpp>
#include <hyprland/src/managers/SessionLockManager.hpp>
#include <cerrno>
#include <glib.h>
#include <hyprland/src/managers/eventLoop/EventLoopManager.hpp>

using namespace Render::GL;

static CHyprColor configColor(Config::INTEGER color) {
    return CHyprColor{static_cast<uint64_t>(color)};
}

class NameClipboardSource final : public IDataSource {
    std::string m_text;
  public:
    explicit NameClipboardSource(std::string text) : m_text(std::move(text)) {}
    ~NameClipboardSource() override { m_events.destroy.emit(); }
    std::vector<std::string> mimes() override { return {"text/plain;charset=utf-8", "text/plain", "UTF8_STRING"}; }
    void send(const std::string&, Hyprutils::OS::CFileDescriptor fd) override {
        size_t offset = 0;
        while (offset < m_text.size()) {
            const auto count = write(fd.get(), m_text.data() + offset, m_text.size() - offset);
            if (count < 0 && errno == EINTR) continue;
            if (count <= 0) break;
            offset += count;
        }
    }
    void accepted(const std::string&) override {}
    void cancelled() override {}
    void error(uint32_t, const std::string&) override {}
    eDataSourceType type() override { return DATA_SOURCE_TYPE_WAYLAND; }
};

CAgentTitleDecoration::CAgentTitleDecoration(PHLWINDOW pWindow) : IHyprWindowDecoration(pWindow) {
    m_pWindow = pWindow;

    const auto PMONITOR         = pWindow->m_monitor.lock();
    if (PMONITOR) PMONITOR->m_scheduledRecalc = true;

    // button events
    m_pMouseButtonCallback = Event::bus()->m_events.input.mouse.button.listen([&](IPointer::SButtonEvent e, Event::SCallbackInfo& info) { onMouseButton(info, e); });
    Animation::mgr()->createAnimation(configColor(g_agentTitlesState->config.barColor->value()), m_cRealBarColor, Config::animationTree()->getAnimationPropertyConfig("border"),
                                      pWindow, AVARDAMAGE_NONE);
    m_cRealBarColor->setUpdateCallback([&](auto) { damageEntire(); });
}

CAgentTitleDecoration::~CAgentTitleDecoration() {
    closePaste();
    if (m_repeat) { g_pEventLoopManager->removeTimer(m_repeat); m_repeat.reset(); }
    if (m_compose) xkb_compose_state_unref(m_compose);
    if (g_agentTitlesState->editing == m_self) g_agentTitlesState->editing.reset();
    std::erase(g_agentTitlesState->bars, m_self);
}

SDecorationPositioningInfo CAgentTitleDecoration::getPositioningInfo() {
    const auto                 HEIGHT     = g_agentTitlesState->config.barHeight->value();
    const auto                 ENABLED    = g_agentTitlesState->config.enabled->value();
    const auto                 PRECEDENCE = g_agentTitlesState->config.barPrecedenceOverBorder->value();

    SDecorationPositioningInfo info;
    info.policy         = m_hidden ? DECORATION_POSITION_ABSOLUTE : DECORATION_POSITION_STICKY;
    info.edges          = DECORATION_EDGE_TOP;
    info.priority       = PRECEDENCE ? 10005 : 5000;
    info.reserved       = true;
    info.desiredExtents = {{0, static_cast<int>(m_hidden || !ENABLED ? 0 : HEIGHT)}, {0, 0}};
    return info;
}

void CAgentTitleDecoration::onPositioningReply(const SDecorationPositioningReply& reply) {
    if (reply.assignedGeometry.size() != m_bAssignedBox.size())
        m_bWindowSizeChanged = true;

    m_bAssignedBox = reply.assignedGeometry;
}

std::string CAgentTitleDecoration::getDisplayName() {
    return "Agent Title";
}

bool CAgentTitleDecoration::inputIsValid() {
    if (m_hidden || !g_agentTitlesState->config.enabled->value())
        return false;

    if (g_pSeatManager->m_seatGrab && !g_pSeatManager->m_seatGrab->accepts(m_pWindow->wlSurface()->resource()))
        return false;

    const auto MOUSE    = g_pInputManager->getMouseCoordsInternal();
    auto       PMONITOR = Desktop::focusState()->monitor();

    if (!PMONITOR)
        return false;

    Desktop::CViewHitTester hitTester{*Desktop::viewState()};

    const auto              WINDOWATCURSOR = hitTester.windowAt(MOUSE, Desktop::View::RESERVED_EXTENTS | Desktop::View::INPUT_EXTENTS | Desktop::View::ALLOW_FLOATING);

    auto                    focusState = Desktop::focusState();
    auto                    window     = focusState->window();

    if (WINDOWATCURSOR != m_pWindow && m_pWindow != window)
        return false;

    PHLLS    foundSurface = nullptr;
    Vector2D surfaceCoords;

    // Check Top Layer
    hitTester.layerSurfaceAt(MOUSE, &PMONITOR->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP], &surfaceCoords, &foundSurface);
    if (foundSurface)
        return false;

    // Check Overlay Layer
    hitTester.layerSurfaceAt(MOUSE, &PMONITOR->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY], &surfaceCoords, &foundSurface);
    if (foundSurface)
        return false;

    return true;
}

void CAgentTitleDecoration::onMouseButton(Event::SCallbackInfo& info, IPointer::SButtonEvent e) {
    const auto coords = cursorRelativeToBar();
    const bool inside = !m_hidden && VECINRECT(coords, 0, 0, assignedBoxGlobal().w, assignedBoxGlobal().h) && inputIsValid();
    if (m_editing && !inside && e.state == WL_POINTER_BUTTON_STATE_PRESSED) finishEditing(true);
    if (g_pInputManager->getModsFromAllKBs() & HL_MODIFIER_META) return;
    if (inside && e.button == 272) {
        info.cancelled = true;
        if (e.state == WL_POINTER_BUTTON_STATE_PRESSED && !m_editing) beginEditing();
        return;
    }
    return;
}

void CAgentTitleDecoration::renderBarTitle(const Vector2D& bufferSize, const float scale) {
    const auto COLORVAL         = g_agentTitlesState->config.textColor->value();
    const auto SIZE             = g_agentTitlesState->config.barTextSize->value();
    const auto WEIGHT           = g_agentTitlesState->config.barTextWeight->value();
    const auto FONT             = g_agentTitlesState->config.barTextFont->value();
    const auto BARPADDING       = g_agentTitlesState->config.barPadding->value();

    const int  scaledSize        = std::round(SIZE * scale);
    const auto scaledBarPadding  = BARPADDING * scale;
    const int  paddingTotal      = scaledBarPadding * 2;
    const int  maxWidth          = std::clamp(static_cast<int>(bufferSize.x - paddingTotal), 0, INT_MAX);

    if (m_szLastTitle.empty() || maxWidth < 1) {
        m_pTextTex = nullptr;
        return;
    }

    const CHyprColor COLOR = m_bForcedTitleColor.value_or(configColor(COLORVAL));
    m_pTextTex             = g_pHyprRenderer->renderText(m_szLastTitle, COLOR, scaledSize, false, FONT, maxWidth, WEIGHT.m_value);
}

void CAgentTitleDecoration::draw(PHLMONITOR, const float& a) {
    const auto ENABLED = g_agentTitlesState->config.enabled->value();

    if (m_bLastEnabledState != ENABLED) {
        m_bLastEnabledState = ENABLED;
        g_pDecorationPositioner->repositionDeco(this);
    }

    if (m_hidden || !validMapped(m_pWindow) || !ENABLED)
        return;

    const auto PWINDOW = m_pWindow.lock();

    if (!PWINDOW->m_ruleApplicator->decorate().valueOrDefault())
        return;

    auto data = CAgentTitlePassElement::SBarData{this, a};
    g_pHyprRenderer->m_renderPass.add(makeUnique<CAgentTitlePassElement>(data));
}

void CAgentTitleDecoration::renderPass(PHLMONITOR pMonitor, const float& a) {
    const auto  PWINDOW = m_pWindow.lock();

    static auto PENABLEBLURGLOBAL = CConfigValue<Config::BOOL>("decoration:blur:enabled");
    const auto  BARCOLOR          = g_agentTitlesState->config.barColor->value();
    const auto  HEIGHT            = g_agentTitlesState->config.barHeight->value();
    const auto  PRECEDENCE        = g_agentTitlesState->config.barPrecedenceOverBorder->value();
    const auto  ENABLETITLE       = g_agentTitlesState->config.barTitleEnabled->value();
    const auto  ENABLEBLUR        = g_agentTitlesState->config.barBlur->value();

    const CHyprColor DEST_COLOR = m_bForcedBarColor.value_or(configColor(BARCOLOR));
    if (DEST_COLOR != m_cRealBarColor->goal())
        *m_cRealBarColor = DEST_COLOR;

    CHyprColor color = m_cRealBarColor->value();

    color.a *= a;
    const bool SHOULDBLUR   = ENABLEBLUR && *PENABLEBLURGLOBAL && color.a < 1.F;

    if (HEIGHT < 1) {
        m_iLastHeight = HEIGHT;
        return;
    }

    const auto PWORKSPACE      = PWINDOW->m_workspace;
    const auto WORKSPACEOFFSET = PWORKSPACE && !PWINDOW->m_pinned ? PWORKSPACE->m_renderOffset->value() : Vector2D();

    const auto ROUNDING = PWINDOW->rounding() + (PRECEDENCE ? 0 : PWINDOW->getRealBorderSize());

    const auto scaledRounding = std::max(0, static_cast<int>(ROUNDING * pMonitor->m_scale - 2));

    m_seExtents = {{0, static_cast<int>(HEIGHT)}, {}};

    const auto DECOBOX = assignedBoxGlobal();

    const auto BARBUF = DECOBOX.size() * pMonitor->m_scale;

    CBox       titleBarBox = {DECOBOX.x - pMonitor->m_position.x, DECOBOX.y - pMonitor->m_position.y, DECOBOX.w,
                              DECOBOX.h + ROUNDING * 3 /* to fill the bottom cuz we can't disable rounding there */};

    titleBarBox.translate(PWINDOW->m_floatingOffset).scale(pMonitor->m_scale).round();

    if (titleBarBox.w < 1 || titleBarBox.h < 1)
        return;

    g_pHyprOpenGL->scissor(titleBarBox);

    if (ROUNDING) {
        // Inset the stencil until renderRect supports an alpha matte.
        CBox windowBox = {PWINDOW->position(Desktop::View::IGeometric::GEOMETRIC_CURRENT).x + PWINDOW->m_floatingOffset.x - pMonitor->m_position.x + 1,
                          PWINDOW->position(Desktop::View::IGeometric::GEOMETRIC_CURRENT).y + PWINDOW->m_floatingOffset.y - pMonitor->m_position.y + 1,
                          PWINDOW->size(Desktop::View::IGeometric::GEOMETRIC_CURRENT).x - 2, PWINDOW->size(Desktop::View::IGeometric::GEOMETRIC_CURRENT).y - 2};

        if (windowBox.w < 1 || windowBox.h < 1)
            return;

        glClearStencil(0);
        glClear(GL_STENCIL_BUFFER_BIT);

        g_pHyprOpenGL->setCapStatus(GL_STENCIL_TEST, true);

        glStencilFunc(GL_ALWAYS, 1, -1);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

        windowBox.translate(WORKSPACEOFFSET).scale(pMonitor->m_scale).round();
        g_pHyprOpenGL->renderRect(windowBox, CHyprColor(0, 0, 0, 0), {.round = scaledRounding, .roundingPower = m_pWindow->roundingPower()});
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

        glStencilFunc(GL_NOTEQUAL, 1, -1);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    }

    if (SHOULDBLUR)
        g_pHyprOpenGL->renderRect(titleBarBox, color, {.round = scaledRounding, .roundingPower = m_pWindow->roundingPower(), .blur = true, .blurA = a});
    else
        g_pHyprOpenGL->renderRect(titleBarBox, color, {.round = scaledRounding, .roundingPower = m_pWindow->roundingPower()});

    // render title
    const auto title = displayTitle();
    if (ENABLETITLE && (m_szLastTitle != title || m_bWindowSizeChanged || m_lastScale != pMonitor->m_scale || !m_pTextTex || m_pTextTex->m_texID == 0 || m_bTitleColorChanged)) {
        m_szLastTitle = title;
        m_lastScale = pMonitor->m_scale;
        renderBarTitle(BARBUF, pMonitor->m_scale);
    }

    if (ROUNDING) {
        // cleanup stencil
        glClearStencil(0);
        glClear(GL_STENCIL_BUFFER_BIT);
        g_pHyprOpenGL->setCapStatus(GL_STENCIL_TEST, false);
        glStencilMask(-1);
        glStencilFunc(GL_ALWAYS, 1, 0xFF);
    }

    CBox textBox = {titleBarBox.x, titleBarBox.y, BARBUF.x, BARBUF.y};
    if (ENABLETITLE && m_pTextTex) {
        const auto BARPADDING       = g_agentTitlesState->config.barPadding->value();
        const auto ALIGN            = g_agentTitlesState->config.barTextAlign->value();

        const auto scaledBorderSize  = PWINDOW->getRealBorderSize() * pMonitor->m_scale;
        const auto scaledBarPadding  = BARPADDING * pMonitor->m_scale;
        const auto xOffset           = ALIGN == "left" ? std::round(scaledBarPadding) :
                                                         std::round(((BARBUF.x - scaledBorderSize) / 2.0 - m_pTextTex->m_size.x / 2.0));
        const auto yOffset           = std::round((BARBUF.y - m_pTextTex->m_size.y) / 2.0);
        CBox       titleBox          = {textBox.x + xOffset, textBox.y + yOffset, m_pTextTex->m_size.x, m_pTextTex->m_size.y};

        CHyprOpenGLImpl::STextureRenderData options{};
        options.a = a;
        g_pHyprOpenGL->renderTexture(m_pTextTex, titleBox, options);
    }


    g_pHyprOpenGL->scissor(nullptr);


    m_bWindowSizeChanged = false;
    m_bTitleColorChanged = false;

    // dynamic updates change the extents
    if (m_iLastHeight != HEIGHT) {
        PWINDOW->layoutTarget()->recalc();
        m_iLastHeight = HEIGHT;
    }
}

eDecorationType CAgentTitleDecoration::getDecorationType() {
    return DECORATION_CUSTOM;
}

void CAgentTitleDecoration::updateWindow(PHLWINDOW) {
    damageEntire();
}

void CAgentTitleDecoration::onConfigReloaded() {
    if (m_editing && !g_agentTitlesState->config.enabled->value()) finishEditing(true);
    m_bTitleColorChanged = true;
    m_pTextTex           = nullptr;

    g_pDecorationPositioner->repositionDeco(this);
    damageEntire();
}

void CAgentTitleDecoration::damageEntire() {
    g_pHyprRenderer->damageBox(assignedBoxGlobal());
}

Vector2D CAgentTitleDecoration::cursorRelativeToBar() {
    return g_pInputManager->getMouseCoordsInternal() - assignedBoxGlobal().pos();
}

eDecorationLayer CAgentTitleDecoration::getDecorationLayer() {
    return DECORATION_LAYER_UNDER;
}

uint64_t CAgentTitleDecoration::getDecorationFlags() {
    return DECORATION_ALLOWS_MOUSE_INPUT | (g_agentTitlesState->config.barPartOfWindow->value() ? DECORATION_PART_OF_MAIN_WINDOW : 0);
}

CBox CAgentTitleDecoration::assignedBoxGlobal() {
    if (!validMapped(m_pWindow))
        return {};

    CBox box = m_bAssignedBox;
    box.translate(g_pDecorationPositioner->getEdgeDefinedPoint(DECORATION_EDGE_TOP, m_pWindow.lock()));

    const auto PWORKSPACE      = m_pWindow->m_workspace;
    const auto WORKSPACEOFFSET = PWORKSPACE && !m_pWindow->m_pinned ? PWORKSPACE->m_renderOffset->value() : Vector2D();

    return box.translate(WORKSPACEOFFSET);
}

PHLWINDOW CAgentTitleDecoration::getOwner() {
    return m_pWindow.lock();
}

void CAgentTitleDecoration::updateRules() {
    const auto PWINDOW              = m_pWindow.lock();
    auto       prevHidden           = m_hidden;
    auto       prevForcedTitleColor = m_bForcedTitleColor;

    m_bForcedBarColor   = std::nullopt;
    m_bForcedTitleColor = std::nullopt;
    m_hidden            = !m_agent.has_value();

    if (PWINDOW->m_ruleApplicator->m_otherProps.props.contains(g_agentTitlesState->nobarRuleIdx))
        m_hidden = m_hidden || truthy(PWINDOW->m_ruleApplicator->m_otherProps.props.at(g_agentTitlesState->nobarRuleIdx)->effect);
    if (PWINDOW->m_ruleApplicator->m_otherProps.props.contains(g_agentTitlesState->barColorRuleIdx))
        m_bForcedBarColor = CHyprColor(Config::ParserUtils::parseColor(PWINDOW->m_ruleApplicator->m_otherProps.props.at(g_agentTitlesState->barColorRuleIdx)->effect).value_or(0));
    if (PWINDOW->m_ruleApplicator->m_otherProps.props.contains(g_agentTitlesState->titleColorRuleIdx))
        m_bForcedTitleColor = CHyprColor(Config::ParserUtils::parseColor(PWINDOW->m_ruleApplicator->m_otherProps.props.at(g_agentTitlesState->titleColorRuleIdx)->effect).value_or(0));

    if (prevHidden != m_hidden)
        g_pDecorationPositioner->repositionDeco(this);
    if (prevForcedTitleColor != m_bForcedTitleColor)
        m_bTitleColorChanged = true;
}

void CAgentTitleDecoration::updateAgent() {
    const auto window = getOwner();
    if (!window) return;
    auto detected = AgentTitles::detect(window->m_class, window->m_title, m_agent ? m_agent->provider : "");
    const auto custom = g_agentTitlesState->names.get(std::format("{:x}", window->m_stableID));
    if (detected == m_agent && custom == m_customName) return;
    const bool wasHidden = m_hidden;
    m_agent = std::move(detected);
    m_nameKey = std::format("{:x}", window->m_stableID);
    m_customName = custom;
    if (!m_agent && m_editing) finishEditing(true);
    updateRules();
    m_pTextTex.reset();
    if (wasHidden != m_hidden) window->updateWindowDecos();
    damageEntire();
}

std::string CAgentTitleDecoration::displayTitle() const {
    if (m_editing) {
        auto text = m_editor.text;
        text.insert(m_editor.cursor, "│");
        return text;
    }
    if (!m_agent) return {};
    return m_agent->provider + "  " + (m_customName.empty() ? m_agent->text : m_customName);
}

void CAgentTitleDecoration::beginEditing() {
    const auto window = getOwner();
    if (!window || !m_agent || g_pSessionLockManager->isSessionLocked()) return;
    if (g_agentTitlesState->editing && g_agentTitlesState->editing != m_self)
        if (!g_agentTitlesState->editing->finishEditing(true)) return;
    Desktop::focusState()->fullWindowFocus(window, Desktop::FOCUS_REASON_CLICK);
    m_editor.begin(m_customName.empty() ? m_agent->text : m_customName);
    m_editing = true;
    g_agentTitlesState->editing = m_self;
    if (!m_compose) {
        auto context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
        const char* locale = std::getenv("LC_ALL");
        if (!locale || !*locale) locale = std::getenv("LC_CTYPE");
        if (!locale || !*locale) locale = std::getenv("LANG");
        if (context) {
            auto table = xkb_compose_table_new_from_locale(context, locale && *locale ? locale : "C.UTF-8", XKB_COMPOSE_COMPILE_NO_FLAGS);
            if (table) { m_compose = xkb_compose_state_new(table, XKB_COMPOSE_STATE_NO_FLAGS); xkb_compose_table_unref(table); }
            xkb_context_unref(context);
        }
    } else xkb_compose_state_reset(m_compose);
    m_repeat = makeShared<CEventLoopTimer>(std::nullopt, [this](SP<CEventLoopTimer> timer, void*) {
        if (!m_editing || !m_repeatKey) return;
        Event::SCallbackInfo info;
        m_repeating = true;
        onKey(*m_repeatKey, info);
        m_repeating = false;
        if (m_editing && m_repeatKey) {
            auto keyboard = g_pSeatManager->m_keyboard.lock();
            const auto rate = keyboard ? std::clamp(keyboard->m_repeatRate, 1, 100) : 25;
            timer->updateTimeout(std::chrono::milliseconds(1000 / rate));
        }
    }, nullptr);
    g_pEventLoopManager->addTimer(m_repeat);
    m_bForcedTitleColor = CHyprColor(0xffb4e66e);
    m_bTitleColorChanged = true;
    damageEntire();
}

bool CAgentTitleDecoration::finishEditing(bool save) {
    if (!m_editing) return true;
    if (save && !g_agentTitlesState->names.set(m_nameKey, m_editor.text)) {
        HyprlandAPI::addNotification(AGENT_TITLES_HANDLE, "Agent title could not be saved; press Escape to cancel.", CHyprColor(0xffff5555), 3000);
        return false;
    }
    closePaste();
    if (m_repeat) { g_pEventLoopManager->removeTimer(m_repeat); m_repeat.reset(); }
    m_repeatKey.reset();
    m_editing = false;
    g_agentTitlesState->editing.reset();
    m_customName = g_agentTitlesState->names.get(m_nameKey);
    updateRules();
    m_pTextTex.reset();
    damageEntire();
    return true;
}

void CAgentTitleDecoration::onKey(IKeyboard::SKeyEvent event, Event::SCallbackInfo& info) {
    if (!m_editing || event.state != WL_KEYBOARD_KEY_STATE_PRESSED) return;
    if (g_pSessionLockManager->isSessionLocked() || Desktop::focusState()->window() != getOwner()) { finishEditing(false); return; }
    auto keyboard = g_pSeatManager->m_keyboard.lock();
    if (!keyboard || !keyboard->m_xkbState) return;
    const auto mods = keyboard->getModifiers();
    if (mods & (HL_MODIFIER_META | HL_MODIFIER_ALT)) { finishEditing(true); return; }
    const auto symbol = xkb_state_key_get_one_sym(keyboard->m_xkbState, event.keycode + 8);
    const bool ctrl = mods & HL_MODIFIER_CTRL;
    const bool shift = mods & HL_MODIFIER_SHIFT;
    // Modifiers keep their normal state lifecycle; editing actions and text
    // are consumed before keybindings or terminal clients receive them.
    if (symbol == XKB_KEY_Shift_L || symbol == XKB_KEY_Shift_R || symbol == XKB_KEY_Control_L || symbol == XKB_KEY_Control_R ||
        symbol == XKB_KEY_Alt_L || symbol == XKB_KEY_Alt_R || symbol == XKB_KEY_ISO_Level3_Shift || symbol == XKB_KEY_Caps_Lock) return;
    info.cancelled = true;
    g_agentTitlesState->swallowedKeys.insert(event.keycode);
    if (symbol == XKB_KEY_Escape) { finishEditing(false); return; }
    if (symbol == XKB_KEY_Return || symbol == XKB_KEY_KP_Enter || symbol == XKB_KEY_Tab) { finishEditing(true); return; }
    if (ctrl && (symbol == XKB_KEY_a || symbol == XKB_KEY_A)) { m_editor.anchor = 0; m_editor.cursor = m_editor.text.size(); }
    else if (ctrl && (symbol == XKB_KEY_c || symbol == XKB_KEY_C || symbol == XKB_KEY_x || symbol == XKB_KEY_X)) {
        if (m_editor.selected()) {
            const auto begin = std::min(m_editor.cursor, m_editor.anchor);
            g_agentTitlesState->clipboard = makeShared<NameClipboardSource>(m_editor.text.substr(begin, std::max(m_editor.cursor, m_editor.anchor) - begin));
            g_pSeatManager->setCurrentSelection(g_agentTitlesState->clipboard);
            if (symbol == XKB_KEY_x || symbol == XKB_KEY_X) m_editor.eraseSelection();
        }
    }
    else if (ctrl && (symbol == XKB_KEY_v || symbol == XKB_KEY_V)) paste();
    else if (symbol == XKB_KEY_BackSpace) m_editor.backspace();
    else if (symbol == XKB_KEY_Delete) m_editor.deleteForward();
    else if (symbol == XKB_KEY_Home) m_editor.move(0, shift);
    else if (symbol == XKB_KEY_End) m_editor.move(m_editor.text.size(), shift);
    else if (symbol == XKB_KEY_Left) m_editor.left(shift);
    else if (symbol == XKB_KEY_Right) m_editor.right(shift);
    else if (!ctrl) {
        char text[128] = {};
        auto count = 0;
        bool direct = true;
        if (m_compose) {
            xkb_compose_state_feed(m_compose, symbol);
            const auto status = xkb_compose_state_get_status(m_compose);
            if (status == XKB_COMPOSE_COMPOSING) direct = false;
            else if (status == XKB_COMPOSE_COMPOSED) { count = xkb_compose_state_get_utf8(m_compose, text, sizeof(text)); xkb_compose_state_reset(m_compose); direct = false; }
            else if (status == XKB_COMPOSE_CANCELLED) { xkb_compose_state_reset(m_compose); direct = false; }
        }
        if (direct) count = xkb_state_key_get_utf8(keyboard->m_xkbState, event.keycode + 8, text, sizeof(text));
        if (count > 0 && count < static_cast<int>(sizeof(text)) && static_cast<unsigned char>(text[0]) >= 32)
            m_editor.insert(std::string_view(text, count));
    }
    if (!m_repeating && !ctrl && m_repeat && keyboard->m_repeatRate > 0 &&
        xkb_keymap_key_repeats(keyboard->m_xkbKeymap, event.keycode + 8)) {
        m_repeatKey = event;
        m_repeat->updateTimeout(std::chrono::milliseconds(std::clamp(keyboard->m_repeatDelay, 100, 2000)));
    }
    m_pTextTex.reset();
    damageEntire();
}

void CAgentTitleDecoration::releaseKey(uint32_t keycode) {
    if (m_repeatKey && m_repeatKey->keycode == keycode) {
        m_repeatKey.reset();
        if (m_repeat) m_repeat->updateTimeout(std::nullopt);
    }
}

void CAgentTitleDecoration::closePaste() {
    if (m_pasteSource) { wl_event_source_remove(m_pasteSource); m_pasteSource = nullptr; }
    if (m_pasteFd >= 0) { close(m_pasteFd); m_pasteFd = -1; }
    m_pasteBuffer.clear();
}

void CAgentTitleDecoration::paste() {
    closePaste();
    auto source = g_pSeatManager->m_selection.currentSelection.lock();
    if (!source) return;
    const auto mimes = source->mimes();
    std::string mime;
    for (const auto candidate : {"text/plain;charset=utf-8", "text/plain", "UTF8_STRING"})
        if (std::ranges::find(mimes, candidate) != mimes.end()) { mime = candidate; break; }
    if (mime.empty()) return;
    int fds[2];
    if (pipe2(fds, O_CLOEXEC | O_NONBLOCK) != 0) return;
    m_pasteFd = fds[0];
    m_pasteSource = wl_event_loop_add_fd(wl_display_get_event_loop(g_pCompositor->m_wlDisplay), m_pasteFd,
        WL_EVENT_READABLE | WL_EVENT_HANGUP | WL_EVENT_ERROR, [](int fd, uint32_t mask, void* data) {
            auto self = static_cast<CAgentTitleDecoration*>(data);
            char buffer[1024];
            bool done = mask & (WL_EVENT_HANGUP | WL_EVENT_ERROR);
            for (;;) {
                const auto count = read(fd, buffer, sizeof(buffer));
                if (count == 0) { done = true; break; }
                if (count < 0) { if (errno != EAGAIN && errno != EINTR) done = true; break; }
                self->m_pasteBuffer.append(buffer, count);
                if (self->m_pasteBuffer.size() > 4096) { done = true; break; }
            }
            if (done) {
                if (self->m_editing && self->m_pasteBuffer.size() <= 4096) {
                    auto valid = g_utf8_make_valid(self->m_pasteBuffer.data(), self->m_pasteBuffer.size());
                    self->m_editor.insert(AgentTitles::sanitize(valid));
                    g_free(valid);
                    self->m_pTextTex.reset();
                    self->damageEntire();
                }
                self->closePaste();
            }
            return 0;
        }, this);
    if (!m_pasteSource) { close(fds[1]); closePaste(); return; }
    source->send(mime, Hyprutils::OS::CFileDescriptor(fds[1]));
}
