#pragma once

#define WLR_USE_UNSTABLE

#include <hyprland/src/render/decorations/IHyprWindowDecoration.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/gl/GLTexture.hpp>
#include <hyprland/src/devices/IPointer.hpp>
#include <hyprland/src/devices/ITouch.hpp>
#include <hyprland/src/desktop/rule/windowRule/WindowRule.hpp>
#include <hyprland/src/helpers/AnimatedVariable.hpp>
#include <hyprland/src/helpers/time/Time.hpp>
#include <hyprland/src/helpers/signal/Signal.hpp>
#include <hyprland/src/managers/eventLoop/EventLoopTimer.hpp>
#include <xkbcommon/xkbcommon-compose.h>
#include "globals.hpp"

#include <hyprland/src/managers/input/InputManager.hpp>

namespace Event {
    struct SCallbackInfo;
}

class CAgentTitleDecoration : public IHyprWindowDecoration {
  public:
    CAgentTitleDecoration(PHLWINDOW);
    virtual ~CAgentTitleDecoration();

    virtual SDecorationPositioningInfo getPositioningInfo();

    virtual void                       onPositioningReply(const SDecorationPositioningReply& reply);

    virtual void                       draw(PHLMONITOR, float const& a);

    virtual eDecorationType            getDecorationType();

    virtual void                       updateWindow(PHLWINDOW);

    virtual void                       damageEntire();

    virtual eDecorationLayer           getDecorationLayer();

    virtual uint64_t                   getDecorationFlags();


    virtual std::string                getDisplayName();

    PHLWINDOW                          getOwner();

    void                               updateRules();
    void                               onConfigReloaded();
    void                               updateAgent();
    void                               beginEditing();
    bool                               finishEditing(bool save);
    void                               onKey(IKeyboard::SKeyEvent event, Event::SCallbackInfo& info);
    void                               releaseKey(uint32_t keycode);
    std::string                        displayTitle() const;

    WP<CAgentTitleDecoration>                       m_self;

  private:
    SBoxExtents                m_seExtents;

    PHLWINDOWREF               m_pWindow;

    CBox                       m_bAssignedBox;

    SP<Render::ITexture>       m_pTextTex;

    bool                       m_bWindowSizeChanged = false;
    bool                       m_hidden             = true;
    bool                       m_bTitleColorChanged = false;
    bool                       m_bLastEnabledState  = false;
    std::optional<CHyprColor>  m_bForcedBarColor;
    std::optional<CHyprColor>  m_bForcedTitleColor;


    PHLANIMVAR<CHyprColor>     m_cRealBarColor;

    Vector2D                   cursorRelativeToBar();

    void                       renderPass(PHLMONITOR, float const& a);
    void                       renderBarTitle(const Vector2D& bufferSize, const float scale);
    bool inputIsValid();
    void onMouseButton(Event::SCallbackInfo& info, IPointer::SButtonEvent e);
    CBox assignedBoxGlobal();

    CHyprSignalListener m_pMouseButtonCallback;
    std::string         m_szLastTitle;
    std::optional<AgentTitles::Title> m_agent;
    AgentTitles::Editor m_editor;
    bool m_editing = false;
    std::string m_nameKey;
    std::string m_customName;
    void paste();
    void closePaste();
    int m_pasteFd = -1;
    wl_event_source* m_pasteSource = nullptr;
    std::string m_pasteBuffer;
    xkb_compose_state* m_compose = nullptr;
    SP<CEventLoopTimer> m_repeat;
    std::optional<IKeyboard::SKeyEvent> m_repeatKey;
    bool m_repeating = false;
    float m_lastScale = 0;

    // for dynamic updates
    int    m_iLastHeight = 0;


    friend class CAgentTitlePassElement;
};
