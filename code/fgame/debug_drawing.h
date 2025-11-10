/*
===========================================================================
Copyright (C) 2024 the OpenMoHAA team

This file is part of OpenMoHAA source code.

OpenMoHAA source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

OpenMoHAA source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with OpenMoHAA source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

// Added in OPM - Phase 3 Task 3.5
//  Advanced debug drawing primitives for bot AI visualization

#pragma once

#include "g_local.h"
#include "debuglines.h"

/**
 * Advanced debug drawing utilities for bot AI visualization.
 * These extend the basic primitives in debuglines.h with more complex shapes.
 */
namespace DebugDrawing
{
    /**
     * Draw a 3D cone (for vision FOV visualization)
     * @param origin Cone apex position
     * @param direction Forward direction (normalized)
     * @param length Length of cone
     * @param angleDegrees Cone angle in degrees
     * @param r Red component (0.0-1.0)
     * @param g Green component (0.0-1.0)
     * @param b Blue component (0.0-1.0)
     * @param alpha Alpha component (0.0-1.0)
     */
    void DrawCone(Vector origin, Vector direction, float length, float angleDegrees, float r, float g, float b, float alpha);

    /**
     * Draw a wireframe sphere
     * @param center Sphere center
     * @param radius Sphere radius
     * @param r Red component (0.0-1.0)
     * @param g Green component (0.0-1.0)
     * @param b Blue component (0.0-1.0)
     * @param alpha Alpha component (0.0-1.0)
     */
    void DrawSphere(Vector center, float radius, float r, float g, float b, float alpha);

    /**
     * Draw a crosshair at a position (for marking targets)
     * @param position Center position
     * @param size Crosshair size
     * @param r Red component (0.0-1.0)
     * @param g Green component (0.0-1.0)
     * @param b Blue component (0.0-1.0)
     * @param alpha Alpha component (0.0-1.0)
     */
    void DrawCrosshair(Vector position, float size, float r, float g, float b, float alpha);

    /**
     * Draw a 3D text label at a world position
     * @param position World position
     * @param text Text to display
     * @param scale Text scale
     * @param r Red component (0.0-1.0)
     * @param g Green component (0.0-1.0)
     * @param b Blue component (0.0-1.0)
     */
    void DrawText3D(Vector position, const char *text, float scale, float r, float g, float b);

    /**
     * Draw an arrow (directional indicator)
     * @param start Arrow start position
     * @param end Arrow end position
     * @param r Red component (0.0-1.0)
     * @param g Green component (0.0-1.0)
     * @param b Blue component (0.0-1.0)
     * @param alpha Alpha component (0.0-1.0)
     */
    void DrawArrow(Vector start, Vector end, float r, float g, float b, float alpha);

    /**
     * Draw a wireframe box
     * @param mins Minimum corner
     * @param maxs Maximum corner
     * @param r Red component (0.0-1.0)
     * @param g Green component (0.0-1.0)
     * @param b Blue component (0.0-1.0)
     * @param alpha Alpha component (0.0-1.0)
     */
    void DrawBox(Vector mins, Vector maxs, float r, float g, float b, float alpha);

    /**
     * Draw a cube at a position
     * @param center Cube center
     * @param size Cube size (half-extents)
     * @param r Red component (0.0-1.0)
     * @param g Green component (0.0-1.0)
     * @param b Blue component (0.0-1.0)
     * @param alpha Alpha component (0.0-1.0)
     */
    void DrawCube(Vector center, float size, float r, float g, float b, float alpha);

    /**
     * Draw a circle at a position
     * @param center Circle center
     * @param radius Circle radius
     * @param r Red component (0.0-1.0)
     * @param g Green component (0.0-1.0)
     * @param b Blue component (0.0-1.0)
     * @param alpha Alpha component (0.0-1.0)
     * @param horizontal True for horizontal, false for vertical
     */
    void DrawCircle(Vector center, float radius, float r, float g, float b, float alpha, qboolean horizontal);
}
