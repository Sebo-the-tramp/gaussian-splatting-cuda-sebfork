#pragma once

#include "core/dataset.hpp"
#include "core/trainer.hpp"
#include "visualizer/camera_frustum_renderer.hpp"
#include "visualizer/gui/render_settings_panel.hpp"
#include "visualizer/infinite_grid_renderer.hpp"
#include "visualizer/opengl_state_manager.hpp"
#include "visualizer/renderer.hpp"
#include "visualizer/rotation_gizmo.hpp"
#include "visualizer/shader_manager.hpp"
#include "visualizer/translation_gizmo.hpp"
#include "visualizer/view_cube_renderer.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <torch/torch.h>

namespace gs {

    // Encapsulates all scene rendering logic
    class SceneRenderer {
    public:
        enum class GizmoMode {
            NONE,
            ROTATION,
            TRANSLATION
        };

        enum class SplatRenderMode {
            CENTERS, // Normal filled splats
            RINGS    // Hollow ring splats
        };

        struct RenderSettings {
            bool show_grid;
            bool show_view_cube;
            bool show_cameras;
            bool show_image_overlay;
            InfiniteGridRenderer::GridPlane grid_plane;

            RenderSettings()
                : show_grid(true),
                  show_view_cube(true),
                  show_cameras(true),
                  show_image_overlay(false),
                  grid_plane(InfiniteGridRenderer::GridPlane::XZ) {}
        };

        struct SplatRenderConfig {
            SplatRenderMode mode = SplatRenderMode::CENTERS;
            float ring_size = 0.04f;      // Ring thickness (0.0 to 1.0)
            float selection_alpha = 1.0f; // Alpha for selected splats
            bool show_overlay = true;     // Show selection overlay
            glm::vec4 selected_color = glm::vec4(1.0f, 1.0f, 0.2f, 1.0f);
            glm::vec4 unselected_color = glm::vec4(0.5f, 0.5f, 0.5f, 0.3f);
            glm::vec4 locked_color = glm::vec4(0.8f, 0.2f, 0.2f, 0.8f);
        };

        SceneRenderer();
        ~SceneRenderer();

        // Initialize all renderers
        bool initialize(const std::string& shader_path);

        void renderSplats(const Viewport& viewport,
                          Trainer* trainer,
                          std::shared_ptr<RenderSettingsPanel::RenderingConfig> config,
                          std::mutex& splat_mutex);

        void renderSplatsWithRings(const Viewport& viewport,
                                   Trainer* trainer,
                                   const SplatRenderConfig& ring_config,
                                   std::mutex& splat_mutex);

        // Individual component renders
        void renderGrid(const Viewport& viewport, const RenderSettings& settings);
        void renderViewCube(const Viewport& viewport, bool show);
        void renderCameras(const Viewport& viewport, int highlight_index = -1);
        void renderImageOverlay(const Viewport& viewport,
                                const torch::Tensor& image,
                                float x, float y, float width, float height);
        void renderGizmo(const Viewport& viewport);

        // Scene management
        void updateSceneBounds(const glm::vec3& center, float radius);
        void setCameras(const std::vector<std::shared_ptr<Camera>>& cameras,
                        const std::vector<bool>& is_test_camera);

        // View cube interaction
        int hitTestViewCube(const Viewport& viewport, float screen_x, float screen_y);

        // Gizmo control
        void setGizmoMode(GizmoMode mode);
        GizmoMode getGizmoMode() const { return gizmo_mode_; }
        void setGizmoVisible(bool visible);
        bool isGizmoVisible() const;
        glm::mat4 getSceneTransform() const;
        RotationGizmo* getRotationGizmo() { return rotation_gizmo_.get(); }
        TranslationGizmo* getTranslationGizmo() { return translation_gizmo_.get(); }
        void updateGizmoPosition(const glm::vec3& position);

        // Ring mode controls
        void setRingMode(SplatRenderMode mode) { ring_config_.mode = mode; }
        SplatRenderMode getRingMode() const { return ring_config_.mode; }
        void setRingSize(float size) { ring_config_.ring_size = glm::clamp(size, 0.0f, 1.0f); }
        float getRingSize() const { return ring_config_.ring_size; }
        void setRingSelectionAlpha(float alpha) { ring_config_.selection_alpha = glm::clamp(alpha, 0.0f, 1.0f); }
        float getRingSelectionAlpha() const { return ring_config_.selection_alpha; }
        void setRingShowOverlay(bool show) { ring_config_.show_overlay = show; }
        bool getRingShowOverlay() const { return ring_config_.show_overlay; }

        void setRingSelectedColor(const glm::vec4& color) { ring_config_.selected_color = color; }
        void setRingUnselectedColor(const glm::vec4& color) { ring_config_.unselected_color = color; }
        void setRingLockedColor(const glm::vec4& color) { ring_config_.locked_color = color; }

        glm::vec4 getRingSelectedColor() const { return ring_config_.selected_color; }
        glm::vec4 getRingUnselectedColor() const { return ring_config_.unselected_color; }
        glm::vec4 getRingLockedColor() const { return ring_config_.locked_color; }

        SplatRenderConfig& getRingConfig() { return ring_config_; }
        const SplatRenderConfig& getRingConfig() const { return ring_config_; }

        // Getters for GUI interaction
        InfiniteGridRenderer* getGridRenderer() { return grid_renderer_.get(); }
        ViewCubeRenderer* getViewCubeRenderer() { return view_cube_renderer_.get(); }
        CameraFrustumRenderer* getCameraRenderer() { return camera_renderer_.get(); }
        ShaderManager* getShaderManager() { return shader_manager_.get(); }
        std::shared_ptr<ScreenQuadRenderer> getScreenRenderer() { return screen_renderer_; }

    private:
        // Ring mode rendering using post-processing approach

        // Renderers
        std::unique_ptr<ShaderManager> shader_manager_;
        std::unique_ptr<InfiniteGridRenderer> grid_renderer_;
        std::unique_ptr<ViewCubeRenderer> view_cube_renderer_;
        std::unique_ptr<CameraFrustumRenderer> camera_renderer_;
        std::unique_ptr<RotationGizmo> rotation_gizmo_;
        std::unique_ptr<TranslationGizmo> translation_gizmo_;
        GizmoMode gizmo_mode_ = GizmoMode::NONE;
        std::shared_ptr<ScreenQuadRenderer> screen_renderer_;

        // Ring mode rendering
        std::shared_ptr<Shader> ring_splat_shader_;
        SplatRenderConfig ring_config_;

        // Scene info
        glm::vec3 scene_center_{0.0f};
        float scene_radius_{1.0f};
        bool scene_bounds_valid_ = false;

        // View cube position
        float view_cube_margin_ = 20.0f;
        float view_cube_size_ = 120.0f;

        bool initialized_ = false;
    };

} // namespace gs