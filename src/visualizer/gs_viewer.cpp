#include "visualizer/gs_viewer.hpp"
#include <imgui.h>
#include <iostream>
#include <thread>

namespace gs {

    // Static member for callbacks
    static GSViewer* g_current_viewer = nullptr;

    // Static callback functions
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
        // Let ImGui handle it first
        if (ImGui::GetIO().WantCaptureMouse) {
            return;
        }

        if (g_current_viewer && g_current_viewer->getInputHandler()) {
            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);
            g_current_viewer->getInputHandler()->handleMouseButton(button, action, xpos, ypos);
        }
    }

    static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
        // Let ImGui handle it first
        if (ImGui::GetIO().WantCaptureMouse) {
            return;
        }

        if (g_current_viewer && g_current_viewer->getInputHandler()) {
            g_current_viewer->getInputHandler()->handleMouseMove(xpos, ypos);
        }
    }

    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
        // Let ImGui handle it first
        if (ImGui::GetIO().WantCaptureMouse) {
            return;
        }

        if (g_current_viewer && g_current_viewer->getInputHandler()) {
            g_current_viewer->getInputHandler()->handleScroll(yoffset);
        }
    }

    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        // Let ImGui handle it first
        if (ImGui::GetIO().WantCaptureKeyboard) {
            return;
        }

        if (g_current_viewer && g_current_viewer->getInputHandler()) {
            g_current_viewer->getInputHandler()->handleKey(key, scancode, action, mods);
        }
    }

    GSViewer::GSViewer(const std::string& title, int width, int height)
        : title_(title),
          viewport_(width, height) {

        shader_path_ = std::string(PROJECT_ROOT_PATH) + "/include/visualizer/shaders/";
        last_frame_time_ = std::chrono::steady_clock::now();

        render_config_ = std::make_shared<RenderingConfig>();
        training_info_ = std::make_shared<TrainingInfo>();
        notifier_ = std::make_shared<Notifier>();

        setTargetFPS(30);

        // Default render settings
        render_settings_.show_grid = true;
        render_settings_.show_view_cube = true;
        render_settings_.show_cameras = true;
        render_settings_.grid_plane = InfiniteGridRenderer::GridPlane::XZ;

        std::cout << "GSViewer constructed" << std::endl;
    }

    GSViewer::~GSViewer() {
        if (g_current_viewer == this) {
            g_current_viewer = nullptr;
        }

        // If trainer is still running, request it to stop
        if (trainer_ && trainer_->is_running()) {
            std::cout << "Viewer closing - stopping training..." << std::endl;
            trainer_->request_stop();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        std::cout << "GSViewer destroyed." << std::endl;
        shutdownWindow();
    }

    bool GSViewer::initializeWindow() {
        if (!glfwInit()) {
            std::cerr << "Failed to initialize GLFW!" << std::endl;
            return false;
        }

        glfwWindowHint(GLFW_SAMPLES, 8);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_FALSE);
        glfwWindowHint(GLFW_DEPTH_BITS, 24);

        window_ = glfwCreateWindow(
            viewport_.windowSize.x,
            viewport_.windowSize.y,
            title_.c_str(), NULL, NULL);

        if (window_ == NULL) {
            std::cerr << "Failed to create GLFW window!" << std::endl;
            glfwTerminate();
            return false;
        }

        glfwMakeContextCurrent(window_);

        // Set static viewer reference
        g_current_viewer = this;

        // Set GLFW callbacks - these will check ImGui first
        glfwSetMouseButtonCallback(window_, mouseButtonCallback);
        glfwSetCursorPosCallback(window_, cursorPosCallback);
        glfwSetScrollCallback(window_, scrollCallback);
        glfwSetKeyCallback(window_, keyCallback);

        std::cout << "GLFW callbacks registered successfully" << std::endl;

        return true;
    }

    bool GSViewer::initializeOpenGL() {
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
            std::cerr << "GLAD init failed" << std::endl;
            return false;
        }

        glfwSwapInterval(1); // Enable vsync

        // Set default OpenGL state
        glEnable(GL_LINE_SMOOTH);
        glDepthFunc(GL_LEQUAL);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBlendEquation(GL_FUNC_ADD);
        glEnable(GL_PROGRAM_POINT_SIZE);

        return true;
    }

    bool GSViewer::initializeComponents() {
        // Initialize scene renderer
        scene_renderer_ = std::make_unique<SceneRenderer>();
        if (!scene_renderer_->initialize(shader_path_)) {
            std::cerr << "Failed to initialize scene renderer" << std::endl;
            return false;
        }

        // Initialize GUI manager
        gui_manager_ = std::make_unique<GUIManager>();
        if (!gui_manager_->init(window_)) {
            std::cerr << "Failed to initialize GUI manager" << std::endl;
            return false;
        }

        // Initialize input handler
        input_handler_ = std::make_unique<InputHandler>(window_, &viewport_);

        // Set view cube hit test
        input_handler_->setViewCubeHitTest([this](double x, double y) {
            return scene_renderer_->hitTestViewCube(viewport_, x, y);
        });

        return true;
    }

    void GSViewer::shutdownWindow() {
        if (gui_manager_) {
            gui_manager_->shutdown();
        }

        if (window_) {
            glfwDestroyWindow(window_);
            window_ = nullptr;
        }

        glfwTerminate();
    }

    void GSViewer::run() {
        if (!initializeWindow()) {
            std::cerr << "Failed to initialize window" << std::endl;
            return;
        }

        if (!initializeOpenGL()) {
            std::cerr << "Failed to initialize OpenGL" << std::endl;
            shutdownWindow();
            return;
        }

        if (!initializeComponents()) {
            std::cerr << "Failed to initialize components" << std::endl;
            shutdownWindow();
            return;
        }

        // Call derived class initialization
        onInitialize();

        // Setup GUI after initialization
        setupGUI();

        initialized_ = true;

        // Main loop
        while (!glfwWindowShouldClose(window_)) {
            limitFrameRate();
            updateWindowSize();

            // Update viewport for smooth camera transitions
            viewport_.update();

            // Clear screen
            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // Call derived class drawing
            onDraw();

            // Render GUI
            gui_manager_->beginFrame();
            gui_manager_->render();
            gui_manager_->endFrame();

            glfwSwapBuffers(window_);
            glfwPollEvents();
        }

        onClose();
    }

    void GSViewer::setTargetFPS(int fps) {
        target_fps_ = fps;
    }

    void GSViewer::limitFrameRate() {
        auto now = std::chrono::steady_clock::now();
        auto frame_duration = std::chrono::duration_cast<std::chrono::microseconds>(now - last_frame_time_);

        const auto target_duration = std::chrono::microseconds(1000000 / target_fps_);

        if (frame_duration < target_duration) {
            std::this_thread::sleep_for(target_duration - frame_duration);
        }

        last_frame_time_ = std::chrono::steady_clock::now();
    }

    void GSViewer::updateWindowSize() {
        int winW, winH, fbW, fbH;
        glfwGetWindowSize(window_, &winW, &winH);
        glfwGetFramebufferSize(window_, &fbW, &fbH);

        if (viewport_.windowSize.x != winW || viewport_.windowSize.y != winH) {
            viewport_.windowSize = glm::ivec2(winW, winH);
            viewport_.frameBufferSize = glm::ivec2(fbW, fbH);
            glViewport(0, 0, fbW, fbH);

            onResize(winW, winH);
        }
    }

    void GSViewer::setTrainer(Trainer* trainer) {
        trainer_ = trainer;

        // Setup panels if GUI manager exists
        if (getGUIManager()) {
            setupPanels();
        }
    }

    void GSViewer::setDataset(std::shared_ptr<CameraDataset> dataset) {
        dataset_ = dataset;

        // Pass cameras to renderer
        if (dataset_ && getSceneRenderer()) {
            std::vector<bool> is_test_camera(dataset_->get_cameras().size());
            for (size_t i = 0; i < dataset_->get_cameras().size(); ++i) {
                is_test_camera[i] = (i % 8) == 0; // Default test_every=8
            }
            getSceneRenderer()->setCameras(dataset_->get_cameras(), is_test_camera);
        }

        // Setup panels if GUI manager exists
        if (getGUIManager()) {
            setupPanels();
        }
    }

    void GSViewer::onInitialize() {
        std::cout << "GSViewer::onInitialize()" << std::endl;

        // Setup additional key bindings
        setupAdditionalKeyBindings();

        // Connect gizmo hit test and interaction
        if (getSceneRenderer()) {
            // Set gizmo hit test for both rotation and translation
            getInputHandler()->setGizmoHitTest([this](double x, double y) -> int {
                auto renderer = getSceneRenderer();
                if (!renderer) {
                    return -1;
                }

                // Check rotation gizmo
                if (renderer->getGizmoMode() == SceneRenderer::GizmoMode::ROTATION) {
                    auto gizmo = renderer->getRotationGizmo();
                    if (gizmo && gizmo->isVisible()) {
                        auto axis = gizmo->hitTest(getViewport(), x, y);
                        if (axis != RotationGizmo::Axis::NONE) {
                            std::cout << "Rotation gizmo hit! Starting rotation on axis " << static_cast<int>(axis) << std::endl;
                            gizmo->startRotation(axis, x, y, getViewport());
                            return static_cast<int>(axis);
                        }
                    }
                }
                // Check translation gizmo
                else if (renderer->getGizmoMode() == SceneRenderer::GizmoMode::TRANSLATION) {
                    auto gizmo = renderer->getTranslationGizmo();
                    if (gizmo && gizmo->isVisible()) {
                        auto axis = gizmo->hitTest(getViewport(), x, y);
                        if (axis != TranslationGizmo::Axis::NONE) {
                            std::cout << "Translation gizmo hit! Starting translation on axis " << static_cast<int>(axis) << std::endl;
                            gizmo->startTranslation(axis, x, y, getViewport());
                            return static_cast<int>(axis);
                        }
                    }
                }

                return -1;
            });

            // Update the existing mouse move callback to handle both gizmos
            getInputHandler()->setMouseMoveCallback([this](double x, double y, double dx, double dy) {
                auto renderer = getSceneRenderer();
                if (!renderer)
                    return;

                // Handle rotation gizmo
                if (getInputHandler()->isGizmoDragging() &&
                    renderer->getGizmoMode() == SceneRenderer::GizmoMode::ROTATION) {
                    auto gizmo = renderer->getRotationGizmo();
                    if (gizmo && gizmo->isRotating()) {
                        gizmo->updateRotation(x, y, getViewport());
                        return;
                    }
                }
                // Handle translation gizmo
                else if (getInputHandler()->isGizmoDragging() &&
                         renderer->getGizmoMode() == SceneRenderer::GizmoMode::TRANSLATION) {
                    auto gizmo = renderer->getTranslationGizmo();
                    if (gizmo && gizmo->isTranslating()) {
                        gizmo->updateTranslation(x, y, getViewport());

                        // Update gizmo position as it moves
                        glm::vec3 new_pos = gizmo->getPosition();
                        renderer->updateGizmoPosition(new_pos);
                        return;
                    }
                }

                // Let the default handler process camera movement
            });

            // Make sure gizmo operations end properly
            getInputHandler()->addMouseButtonCallback(GLFW_MOUSE_BUTTON_LEFT,
                                                      [this](int button, int action, double x, double y) -> bool {
                                                          if (action == GLFW_RELEASE) {
                                                              auto renderer = getSceneRenderer();
                                                              if (renderer) {
                                                                  // End rotation
                                                                  if (renderer->getGizmoMode() == SceneRenderer::GizmoMode::ROTATION) {
                                                                      auto gizmo = renderer->getRotationGizmo();
                                                                      if (gizmo && gizmo->isRotating()) {
                                                                          std::cout << "Ending rotation" << std::endl;
                                                                          gizmo->endRotation();
                                                                      }
                                                                  }
                                                                  // End translation
                                                                  else if (renderer->getGizmoMode() == SceneRenderer::GizmoMode::TRANSLATION) {
                                                                      auto gizmo = renderer->getTranslationGizmo();
                                                                      if (gizmo && gizmo->isTranslating()) {
                                                                          std::cout << "Ending translation" << std::endl;
                                                                          gizmo->endTranslation();

                                                                          // Update final position
                                                                          glm::vec3 final_pos = gizmo->getPosition();
                                                                          renderer->updateGizmoPosition(final_pos);
                                                                      }
                                                                  }
                                                              }
                                                          }
                                                          return false; // Let other handlers process too
                                                      });
        }

        // Update visualization panel to include scene renderer
        if (viz_panel_) {
            viz_panel_->setSceneRenderer(getSceneRenderer());
        }
    }

    void GSViewer::onClose() {
        std::cout << "GSViewer::onClose()" << std::endl;
    }

    void GSViewer::setupGUI() {
        if (hasTrainer() || hasDataset()) {
            setupPanels();
        }
    }

    void GSViewer::setupPanels() {
        std::cout << "GSViewer::setupPanels() called" << std::endl;

        auto gui = getGUIManager();
        if (!gui)
            return;

        // Clear existing panels
        while (gui->getPanelCount() > 0) {
            gui->removePanel("Training Control");
            gui->removePanel("Rendering Settings");
            gui->removePanel("Camera Controls");
            gui->removePanel("Visualization Settings");
            gui->removePanel("Dataset Viewer");
            gui->removePanel("Ring Mode");
        }

        try {
            // Create panels based on what's available
            if (trainer_) {
                std::cout << "Creating training panels..." << std::endl;
                training_panel_ = std::make_shared<TrainingControlPanel>(trainer_, training_info_);
                render_panel_ = std::make_shared<RenderSettingsPanel>(render_config_);
                gui->addPanel(training_panel_);
                gui->addPanel(render_panel_);
            }

            // Always add camera and visualization panels
            std::cout << "Creating camera and visualization panels..." << std::endl;
            camera_panel_ = std::make_shared<CameraControlPanel>(&getViewport());
            viz_panel_ = std::make_shared<VisualizationPanel>(
                getSceneRenderer()->getGridRenderer(),
                getSceneRenderer()->getViewCubeRenderer(),
                &render_settings_.show_grid,
                &render_settings_.show_view_cube);

            // Connect scene renderer to visualization panel for gizmo control
            viz_panel_->setSceneRenderer(getSceneRenderer());

            gui->addPanel(camera_panel_);
            gui->addPanel(viz_panel_);

            // Add ring mode panel
            std::cout << "Creating ring mode panel..." << std::endl;
            ring_panel_ = std::make_shared<RingModePanel>(getSceneRenderer(), &use_ring_mode_);
            gui->addPanel(ring_panel_);

            // Add dataset viewer if dataset is available
            if (dataset_ && getSceneRenderer()->getCameraRenderer()) {
                std::cout << "Creating dataset viewer panel..." << std::endl;
                dataset_panel_ = std::make_shared<DatasetViewerPanel>(
                    dataset_,
                    getSceneRenderer()->getCameraRenderer(),
                    &getViewport());
                gui->addPanel(dataset_panel_);
            }

            std::cout << "GUI panels setup complete" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Exception during GUI panel setup: " << e.what() << std::endl;
        }
    }

    void GSViewer::setupAdditionalKeyBindings() {
        auto input = getInputHandler();
        if (!input)
            return;

        // Toggle grid
        input->addKeyBinding(
            GLFW_KEY_G, [this]() {
                render_settings_.show_grid = !render_settings_.show_grid;
            },
            "Toggle grid");

        // Toggle cameras
        input->addKeyBinding(
            GLFW_KEY_C, [this]() {
                render_settings_.show_cameras = !render_settings_.show_cameras;
            },
            "Toggle camera frustums");

        // Toggle ring mode
        input->addKeyBinding(
            GLFW_KEY_Q, [this]() {
                toggleRingMode();
            },
            "Toggle ring mode");

        // Toggle rotation gizmo
        input->addKeyBinding(
            GLFW_KEY_R, [this]() {
                if (getSceneRenderer()) {
                    auto current_mode = getSceneRenderer()->getGizmoMode();
                    if (current_mode == SceneRenderer::GizmoMode::ROTATION) {
                        getSceneRenderer()->setGizmoMode(SceneRenderer::GizmoMode::NONE);
                        std::cout << "Gizmos disabled" << std::endl;
                    } else {
                        getSceneRenderer()->setGizmoMode(SceneRenderer::GizmoMode::ROTATION);
                        std::cout << "Rotation gizmo enabled" << std::endl;
                    }
                }
            },
            "Toggle rotation gizmo");

        // Toggle translation gizmo
        input->addKeyBinding(
            GLFW_KEY_T, [this]() {
                if (getSceneRenderer()) {
                    auto current_mode = getSceneRenderer()->getGizmoMode();
                    if (current_mode == SceneRenderer::GizmoMode::TRANSLATION) {
                        getSceneRenderer()->setGizmoMode(SceneRenderer::GizmoMode::NONE);
                        std::cout << "Gizmos disabled" << std::endl;
                    } else {
                        getSceneRenderer()->setGizmoMode(SceneRenderer::GizmoMode::TRANSLATION);
                        std::cout << "Translation gizmo enabled" << std::endl;
                    }
                }
            },
            "Toggle translation gizmo");

        // Navigation keys for dataset
        input->addKeyBinding(
            GLFW_KEY_LEFT, [this]() {
                if (dataset_panel_) {
                    dataset_panel_->previousCamera();
                }
            },
            "Previous camera");

        input->addKeyBinding(
            GLFW_KEY_RIGHT, [this]() {
                if (dataset_panel_) {
                    dataset_panel_->nextCamera();
                }
            },
            "Next camera");

        input->addKeyBinding(
            GLFW_KEY_ESCAPE, [this]() {
                if (dataset_panel_ && dataset_panel_->shouldShowImageOverlay()) {
                    // Toggle overlay off
                    render_settings_.show_image_overlay = false;
                }
            },
            "Close image overlay");

        // Add help key
        input->addKeyBinding(
            GLFW_KEY_SLASH, [this]() {
                show_help_ = !show_help_;
            },
            "Toggle help", GLFW_MOD_SHIFT); // Shift+/ = ?
    }

    void GSViewer::updateSceneBounds() {
        if (!trainer_) {
            return;
        }

        if (trainer_->get_strategy().get_model().size() > 0) {
            auto& model = trainer_->get_strategy().get_model();
            auto means = model.get_means();

            if (means.size(0) > 0) {
                // Move to CPU for calculations
                auto means_cpu = means.to(torch::kCPU);

                // Calculate bounding box
                auto min_vals = std::get<0>(means_cpu.min(0));
                auto max_vals = std::get<0>(means_cpu.max(0));

                glm::vec3 min_point(
                    min_vals[0].item<float>(),
                    min_vals[1].item<float>(),
                    min_vals[2].item<float>());

                glm::vec3 max_point(
                    max_vals[0].item<float>(),
                    max_vals[1].item<float>(),
                    max_vals[2].item<float>());

                // Calculate median center (more robust than mean for point clouds)
                auto median_x = means_cpu.select(1, 0).median();
                auto median_y = means_cpu.select(1, 1).median();
                auto median_z = means_cpu.select(1, 2).median();

                glm::vec3 median_center(
                    median_x.item<float>(),
                    median_y.item<float>(),
                    median_z.item<float>());

                // Use median for center, but calculate radius from bounding box
                scene_center_ = median_center;
                scene_radius_ = glm::length(max_point - min_point) * 0.5f;

                // Clamp radius to reasonable range
                scene_radius_ = std::clamp(scene_radius_, 0.1f, 100.0f);

                scene_bounds_valid_ = true;

                // Update components with current scene bounds
                getViewport().camera.sceneRadius = scene_radius_;
                getViewport().camera.minZoom = scene_radius_ * 0.01f;
                getViewport().camera.maxZoom = scene_radius_ * 100.0f;

                // Update scene renderer with bounds
                if (getSceneRenderer()) {
                    getSceneRenderer()->updateSceneBounds(scene_center_, scene_radius_);

                    // Update gizmo positions - they should follow the scene center
                    getSceneRenderer()->updateGizmoPosition(scene_center_);

                    // Reset camera frustum transform to identity initially
                    // This ensures cameras and point cloud start in sync
                    if (getSceneRenderer()->getCameraRenderer()) {
                        getSceneRenderer()->getCameraRenderer()->setSceneTransform(glm::mat4(1.0f));
                    }
                }

                if (camera_panel_) {
                    camera_panel_->setSceneBounds(scene_center_, scene_radius_);
                }

                // Only print on first initialization
                if (!scene_bounds_initialized_) {
                    std::cout << "Scene bounds - Median Center: ("
                              << scene_center_.x << ", "
                              << scene_center_.y << ", "
                              << scene_center_.z << "), Radius: "
                              << scene_radius_ << std::endl;

                    std::cout << "Bounding box: ("
                              << min_point.x << ", " << min_point.y << ", " << min_point.z
                              << ") to ("
                              << max_point.x << ", " << max_point.y << ", " << max_point.z
                              << ")" << std::endl;

                    // Also update the camera to look at the scene
                    // Set a reasonable initial view position
                    float initial_distance = scene_radius_ * 3.0f; // View from 3x radius away
                    getViewport().target = scene_center_;
                    getViewport().distance = initial_distance;

                    std::cout << "Camera target set to scene center, distance: " << initial_distance << std::endl;

                    scene_bounds_initialized_ = true;
                }
            }
        }
    }

    void GSViewer::handleTrainingStart() {
        if (trainer_ && training_panel_ && training_panel_->shouldStartTraining() && notifier_) {
            std::lock_guard<std::mutex> lock(notifier_->mtx);
            notifier_->ready = true;
            notifier_->cv.notify_one();
            training_panel_->resetStartTrigger();
        }
    }

    void GSViewer::onDraw() {
        // Update scene bounds if needed
        updateSceneBounds();

        auto renderer = getSceneRenderer();

        // 1. Draw grid
        renderer->renderGrid(getViewport(), render_settings_);

        // 2. Draw camera frustums
        if (render_settings_.show_cameras && dataset_panel_) {
            int highlight_idx = dataset_panel_->getCurrentCameraIndex();
            renderer->renderCameras(getViewport(), highlight_idx);
        }

        // 3. Draw splats - THIS IS WHERE RING MODE IS USED
        if (trainer_) {
            glClear(GL_DEPTH_BUFFER_BIT);

            if (use_ring_mode_) {
                // Use ring rendering
                renderer->renderSplatsWithRings(
                    getViewport(),
                    trainer_,
                    renderer->getRingConfig(),
                    splat_mutex_);
            } else {
                // Use original rendering
                renderer->renderSplats(
                    getViewport(),
                    trainer_,
                    render_config_,
                    splat_mutex_);
            }
        }

        // 4. Draw rotation gizmo
        renderer->renderGizmo(getViewport());

        // 5. Draw view cube
        renderer->renderViewCube(getViewport(), render_settings_.show_view_cube);

        // 6. Draw image overlay if enabled
        if (render_settings_.show_image_overlay && dataset_panel_) {
            auto image = dataset_panel_->getCurrentImage();
            if (image.defined()) {
                float overlay_width = 400.0f;
                float overlay_height = overlay_width * image.size(1) / image.size(2);
                float margin = 20.0f;
                float x = getViewport().windowSize.x - overlay_width - margin;
                float y = margin;

                renderer->renderImageOverlay(getViewport(), image, x, y, overlay_width, overlay_height);
            }
        }

        // 7. Draw help overlay if enabled
        if (show_help_) {
            drawHelpOverlay();
        }

        // Handle training start trigger
        handleTrainingStart();
    }

    void GSViewer::drawHelpOverlay() {
        ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(400, 500), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.9f);

        ImGui::Begin("Keyboard Shortcuts", &show_help_,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

        ImGui::Text("Navigation:");
        ImGui::Separator();
        ImGui::BulletText("Left Mouse: Orbit camera / Rotate gizmo");
        ImGui::BulletText("Right Mouse: Pan camera");
        ImGui::BulletText("Scroll: Zoom in/out");
        ImGui::BulletText("F: Focus on world origin");
        ImGui::BulletText("H: Home view (look down at origin)");

        ImGui::Spacing();
        ImGui::Text("Display:");
        ImGui::Separator();
        ImGui::BulletText("G: Toggle grid");
        ImGui::BulletText("C: Toggle camera frustums");
        ImGui::BulletText("Q: Toggle ring mode");
        ImGui::BulletText("R: Toggle rotation gizmo");
        ImGui::BulletText("T: Toggle translation gizmo");
        ImGui::BulletText("Left/Right Arrow: Navigate cameras");
        ImGui::BulletText("ESC: Close image overlay");
        ImGui::BulletText("?: Toggle this help");

        // Show ring mode status
        if (use_ring_mode_) {
            ImGui::Spacing();
            ImGui::Text("Ring Mode Active:");
            ImGui::Separator();
            auto mode = getSceneRenderer()->getRingMode();
            ImGui::BulletText("Mode: %s",
                              mode == SceneRenderer::SplatRenderMode::RINGS ? "Rings" : "Centers");
            ImGui::BulletText("Ring Size: %.3f", getSceneRenderer()->getRingSize());
            ImGui::BulletText("Use Ring Mode panel for more settings");
        }

        if (getSceneRenderer()) {
            auto mode = getSceneRenderer()->getGizmoMode();
            if (mode == SceneRenderer::GizmoMode::ROTATION) {
                ImGui::Spacing();
                ImGui::Text("Rotation Gizmo:");
                ImGui::Separator();
                ImGui::BulletText("Red ring: Rotate around X axis");
                ImGui::BulletText("Green ring: Rotate around Y axis");
                ImGui::BulletText("Blue ring: Rotate around Z axis");
            } else if (mode == SceneRenderer::GizmoMode::TRANSLATION) {
                ImGui::Spacing();
                ImGui::Text("Translation Gizmo:");
                ImGui::Separator();
                ImGui::BulletText("Red arrow: Move along X axis");
                ImGui::BulletText("Green arrow: Move along Y axis");
                ImGui::BulletText("Blue arrow: Move along Z axis");
                ImGui::BulletText("Yellow square: Move in XY plane");
                ImGui::BulletText("Magenta square: Move in XZ plane");
                ImGui::BulletText("Cyan square: Move in YZ plane");
                ImGui::BulletText("Center sphere: Free movement");
            }
        }

        ImGui::Spacing();
        ImGui::Text("Press '?' to close this help");

        ImGui::End();
    }

} // namespace gs
