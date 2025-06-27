#include "visualizer/gui/ring_mode_panel.hpp"
#include <imgui.h>

namespace gs {

    RingModePanel::RingModePanel(SceneRenderer* renderer, bool* use_ring_mode)
        : GUIPanel("Ring Mode"),
          renderer_(renderer),
          use_ring_mode_(use_ring_mode) {

        if (renderer_) {
            current_mode_ = static_cast<int>(renderer_->getRingMode());
        }
    }

    void RingModePanel::render() {
        ImGui::Begin(title_.c_str(), &visible_, window_flags_);
        ImGui::SetWindowSize(ImVec2(280, 0));

        window_active_ = ImGui::IsWindowHovered();

        if (!renderer_) {
            ImGui::Text("No scene renderer available");
            ImGui::End();
            return;
        }

        renderModeControls();
        ImGui::Separator();
        renderRingSettings();
        ImGui::Separator();
        renderColorControls();

        ImGui::End();
    }

    void RingModePanel::renderModeControls() {
        ImGui::Text("Splat Display Mode");
        ImGui::Separator();

        // Main toggle
        if (ImGui::Checkbox("Enable Ring Mode", use_ring_mode_)) {
            std::cout << "Ring mode " << (*use_ring_mode_ ? "enabled" : "disabled") << " via GUI" << std::endl;
        }

        ImGui::BeginDisabled(!*use_ring_mode_);

        // Ring mode selector
        const char* modes[] = {"Centers", "Rings"};
        if (ImGui::Combo("Ring Style", &current_mode_, modes, 2)) {
            renderer_->setRingMode(static_cast<SceneRenderer::SplatRenderMode>(current_mode_));
        }

        ImGui::EndDisabled();

        if (!*use_ring_mode_) {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                               "Enable ring mode to access settings");
        }

        ImGui::Spacing();
        ImGui::Text("Description:");
        if (*use_ring_mode_) {
            if (current_mode_ == 0) {
                ImGui::BulletText("Centers: Traditional filled splats");
                ImGui::BulletText("Good for normal viewing");
            } else {
                ImGui::BulletText("Rings: Hollow ring splats");
                ImGui::BulletText("Better for selection visibility");
                ImGui::BulletText("Allows seeing through splats");
            }
        } else {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Enable to see mode descriptions");
        }
    }

    void RingModePanel::renderRingSettings() {
        ImGui::Text("Ring Settings");
        ImGui::Separator();

        bool rings_enabled = *use_ring_mode_ && (current_mode_ == 1);

        ImGui::BeginDisabled(!rings_enabled);

        float ring_size = renderer_->getRingSize();
        if (ImGui::SliderFloat("Ring Thickness", &ring_size, 0.01f, 0.5f, "%.3f")) {
            renderer_->setRingSize(ring_size);
        }

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Controls how thick the ring outline is\n"
                              "0.01 = very thin ring\n"
                              "0.5 = thick ring");
        }

        float selection_alpha = renderer_->getRingSelectionAlpha();
        if (ImGui::SliderFloat("Selection Alpha", &selection_alpha, 0.0f, 1.0f, "%.2f")) {
            renderer_->setRingSelectionAlpha(selection_alpha);
        }

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Alpha multiplier for selected splats\n"
                              "0.0 = fully transparent\n"
                              "1.0 = fully opaque");
        }

        bool show_overlay = renderer_->getRingShowOverlay();
        if (ImGui::Checkbox("Show Selection Overlay", &show_overlay)) {
            renderer_->setRingShowOverlay(show_overlay);
        }

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Show different colors for selected/unselected splats");
        }

        ImGui::EndDisabled();

        if (!rings_enabled) {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                               "Ring settings only apply in Ring mode");
        }
    }

    void RingModePanel::renderColorControls() {
        ImGui::Text("Selection Colors");
        ImGui::Separator();

        bool enabled = *use_ring_mode_;
        ImGui::BeginDisabled(!enabled);

        glm::vec4 selected = renderer_->getRingSelectedColor();
        float selected_arr[4] = {selected.r, selected.g, selected.b, selected.a};
        if (ImGui::ColorEdit4("Selected", selected_arr, ImGuiColorEditFlags_AlphaBar)) {
            renderer_->setRingSelectedColor(glm::vec4(selected_arr[0], selected_arr[1], selected_arr[2], selected_arr[3]));
        }

        glm::vec4 unselected = renderer_->getRingUnselectedColor();
        float unselected_arr[4] = {unselected.r, unselected.g, unselected.b, unselected.a};
        if (ImGui::ColorEdit4("Unselected", unselected_arr, ImGuiColorEditFlags_AlphaBar)) {
            renderer_->setRingUnselectedColor(glm::vec4(unselected_arr[0], unselected_arr[1], unselected_arr[2], unselected_arr[3]));
        }

        glm::vec4 locked = renderer_->getRingLockedColor();
        float locked_arr[4] = {locked.r, locked.g, locked.b, locked.a};
        if (ImGui::ColorEdit4("Locked", locked_arr, ImGuiColorEditFlags_AlphaBar)) {
            renderer_->setRingLockedColor(glm::vec4(locked_arr[0], locked_arr[1], locked_arr[2], locked_arr[3]));
        }

        if (ImGui::Button("Reset to Defaults", ImVec2(-1, 0))) {
            // Reset to default colors
            renderer_->setRingSelectedColor(glm::vec4(1.0f, 1.0f, 0.2f, 1.0f));
            renderer_->setRingUnselectedColor(glm::vec4(0.5f, 0.5f, 0.5f, 0.3f));
            renderer_->setRingLockedColor(glm::vec4(0.8f, 0.2f, 0.2f, 0.8f));
        }

        ImGui::EndDisabled();

        if (!enabled) {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                               "Color controls only apply in ring mode");
        }
    }

} // namespace gs
