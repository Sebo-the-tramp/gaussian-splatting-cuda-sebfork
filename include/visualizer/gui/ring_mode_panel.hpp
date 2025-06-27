#pragma once

#include "visualizer/gui/gui_manager.hpp"
#include "visualizer/scene_renderer.hpp"
#include <memory>

namespace gs {

    class RingModePanel : public GUIPanel {
    public:
        RingModePanel(SceneRenderer* renderer, bool* use_ring_mode);

        void render() override;

    private:
        void renderModeControls();
        void renderRingSettings();
        void renderColorControls();

        SceneRenderer* renderer_;
        bool* use_ring_mode_;

        // Local state for UI
        int current_mode_ = 0; // 0 = centers, 1 = rings
    };

} // namespace gs
