// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#pragma once

#include <string>

namespace pyb2d
{

    // todo add camera and draw and remove globals
    struct Settings
    {
        // void Save();
        // void Load();

        // int windowWidth = 3840;
        // int windowHeight = 2160;
        int windowWidth = 1920;
        int windowHeight = 1080;
        // int windowWidth = 1280;
        // int windowHeight = 720;
        // int windowWidth = 800;
        // int windowHeight = 600;
        float hertz = 60.0f;
        int subStepCount = 4;
        int workerCount = 1;
        bool useCameraBounds = false;
        bool drawShapes = true;
        bool drawJoints = true;
        bool drawJointExtras = false;
        bool drawAABBs = false;
        bool drawContactPoints = false;
        bool drawContactNormals = false;
        bool drawContactImpulses = false;
        bool drawFrictionImpulses = false;
        bool drawMass = false;
        bool drawGraphColors = false;
        bool drawCounters = false;
        bool drawProfile = false;
        bool enableWarmStarting = true;
        bool enableContinuous = true;
        bool enableSleep = true;
        bool pause = false;
        bool singleStep = false;
        bool restart = false;

        std::string data_dir;
    };

}  // namespace pyb2d
