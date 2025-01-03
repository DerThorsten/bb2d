//  ------------------------------------------------------------------
//  this is a heavy modification of the original sample code:
//  * the motivation is to use the high performance sample code but
//    with the python bindings
//  ------------------------------------------------------------------

// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#define _CRT_SECURE_NO_WARNINGS
#define _CRTDBG_MAP_ALLOC
#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS 1

#include <py_sample_base.hpp>
#include <sample.h>
#include <settings.h>

#include "box2d/base.h"
#include "box2d/box2d.h"
#include "box2d/math_functions.h"
#include "draw.h"

// clang-format off
#include <glad/glad.h>
#include <GLFW/glfw3.h>
// clang-format on

#include <iostream>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <nanobind/nanobind.h>
#include <stdio.h>
#include <stdlib.h>

#include "opengl_frontend.hpp"

GLFWwindow* g_mainWindow = nullptr;
static pyb2d::Sample* s_sample = nullptr;
static pyb2d::Settings s_settings;
static bool s_rightMouseDown = false;
static b2Vec2 s_clickPointWS = b2Vec2_zero;
static float s_windowScale = 1.0f;
static float s_framebufferScale = 1.0f;

namespace pyb2d
{

    inline bool IsPowerOfTwo(int32_t x)
    {
        return (x != 0) && ((x & (x - 1)) == 0);
    }

    void* AllocFcn(uint32_t size, int32_t alignment)
    {
        // Allocation must be a multiple of alignment or risk a seg fault
        // https://en.cppreference.com/w/c/memory/aligned_alloc
        assert(IsPowerOfTwo(alignment));
        size_t sizeAligned = ((size - 1) | (alignment - 1)) + 1;
        assert((sizeAligned & (alignment - 1)) == 0);

#if defined(_WIN64) || defined(_WIN32)
        void* ptr = _aligned_malloc(sizeAligned, alignment);
#else
        void* ptr = aligned_alloc(alignment, sizeAligned);
#endif
        assert(ptr != nullptr);
        return ptr;
    }

    void FreeFcn(void* mem)
    {
#if defined(_WIN64) || defined(_WIN32)
        _aligned_free(mem);
#else
        free(mem);
#endif
    }

    static void RestartSample()
    {
        // g_sampleEntries[s_settings.sampleIndex].py_instance.dec_ref();
        // s_sample = nullptr;
        // s_settings.restart = true;
        // s_sample = g_sampleEntries[s_settings.sampleIndex].createFcn( s_settings );
        // s_settings.restart = false;
    }

    static void ResizeWindowCallback(GLFWwindow*, int width, int height)
    {
        g_camera.m_width = int(width / s_windowScale);
        g_camera.m_height = int(height / s_windowScale);
        s_settings.windowWidth = int(width / s_windowScale);
        s_settings.windowHeight = int(height / s_windowScale);
    }

    static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
        if (ImGui::GetIO().WantCaptureKeyboard)
        {
            return;
        }

        if (action == GLFW_PRESS)
        {
            switch (key)
            {
                case GLFW_KEY_ESCAPE:
                    // Quit
                    glfwSetWindowShouldClose(g_mainWindow, GL_TRUE);
                    break;

                case GLFW_KEY_LEFT:
                    // Pan left
                    if (mods == GLFW_MOD_CONTROL)
                    {
                        b2Vec2 newOrigin = {2.0f, 0.0f};
                        s_sample->ShiftOrigin(newOrigin);
                    }
                    else
                    {
                        g_camera.m_center.x -= 0.5f;
                    }
                    break;

                case GLFW_KEY_RIGHT:
                    // Pan right
                    if (mods == GLFW_MOD_CONTROL)
                    {
                        b2Vec2 newOrigin = {-2.0f, 0.0f};
                        s_sample->ShiftOrigin(newOrigin);
                    }
                    else
                    {
                        g_camera.m_center.x += 0.5f;
                    }
                    break;

                case GLFW_KEY_DOWN:
                    // Pan down
                    if (mods == GLFW_MOD_CONTROL)
                    {
                        b2Vec2 newOrigin = {0.0f, 2.0f};
                        s_sample->ShiftOrigin(newOrigin);
                    }
                    else
                    {
                        g_camera.m_center.y -= 0.5f;
                    }
                    break;

                case GLFW_KEY_UP:
                    // Pan up
                    if (mods == GLFW_MOD_CONTROL)
                    {
                        b2Vec2 newOrigin = {0.0f, -2.0f};
                        s_sample->ShiftOrigin(newOrigin);
                    }
                    else
                    {
                        g_camera.m_center.y += 0.5f;
                    }
                    break;

                case GLFW_KEY_HOME:
                    g_camera.ResetView();
                    break;

                case GLFW_KEY_R:
                    RestartSample();
                    break;

                case GLFW_KEY_O:
                    s_settings.singleStep = true;
                    break;

                case GLFW_KEY_P:
                    s_settings.pause = !s_settings.pause;
                    break;

                case GLFW_KEY_TAB:
                    g_draw.m_showUI = !g_draw.m_showUI;

                default:
                    if (s_sample)
                    {
                        s_sample->Keyboard(key);
                    }
            }
        }
    }

    static void CharCallback(GLFWwindow* window, unsigned int c)
    {
        ImGui_ImplGlfw_CharCallback(window, c);
    }

    static void MouseButtonCallback(GLFWwindow* window, int32_t button, int32_t action, int32_t mods)
    {
        ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);

        if (ImGui::GetIO().WantCaptureMouse)
        {
            return;
        }

        double xd, yd;
        glfwGetCursorPos(g_mainWindow, &xd, &yd);
        b2Vec2 ps = {float(xd) / s_windowScale, float(yd) / s_windowScale};

        // Use the mouse to move things around.
        if (button == GLFW_MOUSE_BUTTON_1)
        {
            b2Vec2 pw = g_camera.ConvertScreenToWorld(ps);
            if (action == GLFW_PRESS)
            {
                s_sample->MouseDown(pw, button, mods);
            }

            if (action == GLFW_RELEASE)
            {
                s_sample->MouseUp(pw, button);
            }
        }
        else if (button == GLFW_MOUSE_BUTTON_2)
        {
            if (action == GLFW_PRESS)
            {
                s_clickPointWS = g_camera.ConvertScreenToWorld(ps);
                s_rightMouseDown = true;
            }

            if (action == GLFW_RELEASE)
            {
                s_rightMouseDown = false;
            }
        }
    }

    static void MouseMotionCallback(GLFWwindow* window, double xd, double yd)
    {
        b2Vec2 ps = {float(xd) / s_windowScale, float(yd) / s_windowScale};

        ImGui_ImplGlfw_CursorPosCallback(window, ps.x, ps.y);

        b2Vec2 pw = g_camera.ConvertScreenToWorld(ps);
        s_sample->MouseMove(pw);

        if (s_rightMouseDown)
        {
            b2Vec2 diff = b2Sub(pw, s_clickPointWS);
            g_camera.m_center.x -= diff.x;
            g_camera.m_center.y -= diff.y;
            s_clickPointWS = g_camera.ConvertScreenToWorld(ps);
        }
    }

    static void ScrollCallback(GLFWwindow* window, double dx, double dy)
    {
        ImGui_ImplGlfw_ScrollCallback(window, dx, dy);
        if (ImGui::GetIO().WantCaptureMouse)
        {
            return;
        }

        if (dy > 0)
        {
            g_camera.m_zoom /= 1.1f;
        }
        else
        {
            g_camera.m_zoom *= 1.1f;
        }
    }

    static void UpdateUI()
    {
        int maxWorkers = enki::GetNumHardwareThreads();

        float menuWidth = 180.0f;
        if (g_draw.m_showUI)
        {
            ImGui::SetNextWindowPos({g_camera.m_width - menuWidth - 10.0f, 10.0f});
            ImGui::SetNextWindowSize({menuWidth, g_camera.m_height - 20.0f});

            ImGui::Begin(
                "Tools",
                &g_draw.m_showUI,
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse
            );

            if (ImGui::BeginTabBar("ControlTabs", ImGuiTabBarFlags_None))
            {
                if (ImGui::BeginTabItem("Controls"))
                {
                    ImGui::PushItemWidth(100.0f);
                    ImGui::SliderInt("Sub-steps", &s_settings.subStepCount, 1, 50);
                    ImGui::SliderFloat("Hertz", &s_settings.hertz, 5.0f, 120.0f, "%.0f hz");

                    if (ImGui::SliderInt("Workers", &s_settings.workerCount, 1, maxWorkers))
                    {
                        s_settings.workerCount = b2ClampInt(s_settings.workerCount, 1, maxWorkers);
                        RestartSample();
                    }
                    ImGui::PopItemWidth();

                    ImGui::Separator();

                    ImGui::Checkbox("Sleep", &s_settings.enableSleep);
                    ImGui::Checkbox("Warm Starting", &s_settings.enableWarmStarting);
                    ImGui::Checkbox("Continuous", &s_settings.enableContinuous);

                    ImGui::Separator();

                    ImGui::Checkbox("Shapes", &s_settings.drawShapes);
                    ImGui::Checkbox("Joints", &s_settings.drawJoints);
                    ImGui::Checkbox("Joint Extras", &s_settings.drawJointExtras);
                    ImGui::Checkbox("AABBs", &s_settings.drawAABBs);
                    ImGui::Checkbox("Contact Points", &s_settings.drawContactPoints);
                    ImGui::Checkbox("Contact Normals", &s_settings.drawContactNormals);
                    ImGui::Checkbox("Contact Impulses", &s_settings.drawContactImpulses);
                    ImGui::Checkbox("Friction Impulses", &s_settings.drawFrictionImpulses);
                    ImGui::Checkbox("Center of Masses", &s_settings.drawMass);
                    ImGui::Checkbox("Graph Colors", &s_settings.drawGraphColors);

                    ImVec2 button_sz = ImVec2(-1, 0);
                    if (ImGui::Button("Pause (P)", button_sz))
                    {
                        s_settings.pause = !s_settings.pause;
                    }

                    if (ImGui::Button("Single Step (O)", button_sz))
                    {
                        s_settings.singleStep = !s_settings.singleStep;
                    }

                    if (ImGui::Button("Restart (R)", button_sz))
                    {
                        RestartSample();
                    }

                    if (ImGui::Button("Quit", button_sz))
                    {
                        glfwSetWindowShouldClose(g_mainWindow, GL_TRUE);
                    }

                    ImGui::EndTabItem();
                }

                ImGuiTreeNodeFlags leafNodeFlags = ImGuiTreeNodeFlags_OpenOnArrow
                                                   | ImGuiTreeNodeFlags_OpenOnDoubleClick;
                leafNodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

                ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow
                                               | ImGuiTreeNodeFlags_OpenOnDoubleClick;

                ImGui::EndTabBar();
            }

            ImGui::End();

            s_sample->UpdateUI();
        }
    }

    int start_everything(const char* data_dir, nanobind::object sample_cls)
    {
        s_settings.data_dir = data_dir;

        nanobind::object py_instance;

        // // Install memory hooks
        b2SetAllocator(AllocFcn, FreeFcn);

        char buffer[128];

        s_settings.workerCount = b2MinInt(8, (int) enki::GetNumHardwareThreads() / 2);

        g_camera.m_width = s_settings.windowWidth;
        g_camera.m_height = s_settings.windowHeight;

        OpenGlFrontend ui(s_settings);
        auto ui_address = &ui;

        g_mainWindow = ui.main_window();

        glfwSetWindowSizeCallback(g_mainWindow, ResizeWindowCallback);
        glfwSetKeyCallback(g_mainWindow, KeyCallback);
        glfwSetCharCallback(g_mainWindow, CharCallback);
        glfwSetMouseButtonCallback(g_mainWindow, MouseButtonCallback);
        glfwSetCursorPosCallback(g_mainWindow, MouseMotionCallback);
        glfwSetScrollCallback(g_mainWindow, ScrollCallback);

        g_draw.Create(data_dir);

        glClearColor(0.2f, 0.2f, 0.2f, 1.0f);

        float frameTime = 0.0;

        int32_t frame = 0;

        g_camera.m_zoom = 20.0f;

        while (!glfwWindowShouldClose(g_mainWindow))
        {
            double time1 = glfwGetTime();

            if (glfwGetKey(g_mainWindow, GLFW_KEY_Z) == GLFW_PRESS)
            {
                // Zoom out
                g_camera.m_zoom = b2MinFloat(1.005f * g_camera.m_zoom, 100.0f);
            }
            else if (glfwGetKey(g_mainWindow, GLFW_KEY_X) == GLFW_PRESS)
            {
                // Zoom in
                g_camera.m_zoom = b2MaxFloat(0.995f * g_camera.m_zoom, 0.5f);
            }

            glfwGetWindowSize(g_mainWindow, &g_camera.m_width, &g_camera.m_height);
            g_camera.m_width = int(g_camera.m_width / s_windowScale);
            g_camera.m_height = int(g_camera.m_height / s_windowScale);

            int bufferWidth, bufferHeight;
            glfwGetFramebufferSize(g_mainWindow, &bufferWidth, &bufferHeight);
            glViewport(0, 0, bufferWidth, bufferHeight);

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            g_draw.DrawBackground();

            double cursorPosX = 0, cursorPosY = 0;
            glfwGetCursorPos(g_mainWindow, &cursorPosX, &cursorPosY);
            ImGui_ImplGlfw_CursorPosCallback(g_mainWindow, cursorPosX / s_windowScale, cursorPosY / s_windowScale);
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui_ImplGlfw_CursorPosCallback(g_mainWindow, cursorPosX / s_windowScale, cursorPosY / s_windowScale);

            ImGuiIO& io = ImGui::GetIO();
            io.DisplaySize.x = float(g_camera.m_width);
            io.DisplaySize.y = float(g_camera.m_height);
            io.DisplayFramebufferScale.x = bufferWidth / float(g_camera.m_width);
            io.DisplayFramebufferScale.y = bufferHeight / float(g_camera.m_height);

            ImGui::NewFrame();

            ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
            ImGui::SetNextWindowSize(ImVec2(float(g_camera.m_width), float(g_camera.m_height)));
            ImGui::SetNextWindowBgAlpha(0.0f);
            ImGui::Begin(
                "Overlay",
                nullptr,
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize
                    | ImGuiWindowFlags_NoScrollbar
            );
            ImGui::End();

            if (s_sample == nullptr)
            {
                py_instance = sample_cls(s_settings);
                s_sample = nanobind::cast<pyb2d::Sample*>(py_instance);

                s_sample->p_debugDraw = &(g_draw.m_debugDraw);
            }

            s_sample->p_debugDraw->drawingBounds = g_camera.GetViewBounds();

            s_sample->Step(s_settings);
            g_draw.Flush();

            UpdateUI();

            if (g_draw.m_showUI)
            {
                snprintf(
                    buffer,
                    128,
                    "%.1f ms - step %d - camera (%g, %g, %g)",
                    1000.0f * frameTime,
                    s_sample->m_stepCount,
                    g_camera.m_center.x,
                    g_camera.m_center.y,
                    g_camera.m_zoom
                );
                // snprintf( buffer, 128, "%.1f ms", 1000.0f * frameTime );

                ImGui::Begin(
                    "Overlay",
                    nullptr,
                    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoInputs
                        | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar
                );
                ImGui::SetCursorPos(ImVec2(5.0f, g_camera.m_height - 20.0f));
                ImGui::TextColored(ImColor(153, 230, 153, 255), "%s", buffer);
                ImGui::End();
            }

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            glfwSwapBuffers(g_mainWindow);

            glfwPollEvents();

            // Limit frame rate to 60Hz
            double time2 = glfwGetTime();
            double targetTime = time1 + 1.0f / 60.0f;
            int loopCount = 0;
            while (time2 < targetTime)
            {
                b2Yield();
                time2 = glfwGetTime();
                ++loopCount;
            }

            frameTime = (float) (time2 - time1);
            ++frame;
        }

        // delete s_sample;
        if (s_sample)
        {
            py_instance.dec_ref();
            s_sample = nullptr;
        }
        s_sample = nullptr;
        g_draw.Destroy();

#if defined(_WIN32)
        _CrtDumpMemoryLeaks();
#endif

        return 0;
    }

}  // namespace pyb2d
