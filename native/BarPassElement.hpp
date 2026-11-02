#pragma once
#include <hyprland/src/render/pass/PassElement.hpp>

class CAgentTitleDecoration;

class CAgentTitlePassElement : public IPassElement {
  public:
    struct SBarData {
        CAgentTitleDecoration* deco = nullptr;
        float     a    = 1.F;
    };

    CAgentTitlePassElement(const SBarData& data_);
    virtual ~CAgentTitlePassElement() = default;

    virtual std::vector<UP<IPassElement>> draw() override;
    virtual bool                          needsLiveBlur() override;
    virtual bool                          needsPrecomputeBlur() override;
    virtual std::optional<CBox>           boundingBox() override;

    virtual const char*                   passName() override {
        return "CAgentTitlePassElement";
    }

    virtual ePassElementType type() override {
        return EK_CUSTOM;
    }

  private:
    SBarData data;
};