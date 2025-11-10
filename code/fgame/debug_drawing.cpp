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
//  Implementation of advanced debug drawing primitives

#include "g_local.h"
#include "debug_drawing.h"
#include "../corepp/vector.h"
#include <cmath>

namespace DebugDrawing
{
    constexpr int   CONE_SEGMENTS   = 16;
    constexpr int   SPHERE_SEGMENTS = 12;
    constexpr float M_PI_F          = 3.14159265358979323846f;

    void DrawCone(
        Vector origin,
        Vector direction,
        float  length,
        float  angleDegrees,
        float  r,
        float  g,
        float  b,
        float  alpha
    )
    {
        // Normalize direction
        Vector forward = direction;
        forward.normalize();

        // Calculate cone radius at the end
        float angleRadians = angleDegrees * M_PI_F / 180.0f;
        float endRadius    = length * tan(angleRadians / 2.0f);

        // Get perpendicular vectors
        Vector right, up;
        forward.AngleVectorsLeft(NULL, &right, &up);

        // Draw cone outline
        Vector endCenter = origin + forward * length;

        // Draw lines from apex to circle points
        for (int i = 0; i < CONE_SEGMENTS; i++) {
            float  angle = (i * 2.0f * M_PI_F) / CONE_SEGMENTS;
            float  nextAngle = ((i + 1) * 2.0f * M_PI_F) / CONE_SEGMENTS;
            Vector circlePoint     = endCenter + right * (cos(angle) * endRadius) + up * (sin(angle) * endRadius);
            Vector nextCirclePoint = endCenter + right * (cos(nextAngle) * endRadius)
                                     + up * (sin(nextAngle) * endRadius);

            // Line from apex to circle
            G_DebugLine(origin, circlePoint, r, g, b, alpha);

            // Circle edge
            G_DebugLine(circlePoint, nextCirclePoint, r, g, b, alpha);
        }
    }

    void DrawSphere(Vector center, float radius, float r, float g, float b, float alpha)
    {
        // Draw three perpendicular circles to form a sphere outline
        for (int axis = 0; axis < 3; axis++) {
            for (int i = 0; i < SPHERE_SEGMENTS; i++) {
                float  angle     = (i * 2.0f * M_PI_F) / SPHERE_SEGMENTS;
                float  nextAngle = ((i + 1) * 2.0f * M_PI_F) / SPHERE_SEGMENTS;
                Vector point, nextPoint;

                switch (axis) {
                case 0: // XY plane
                    point = center + Vector(cos(angle) * radius, sin(angle) * radius, 0);
                    nextPoint = center + Vector(cos(nextAngle) * radius, sin(nextAngle) * radius, 0);
                    break;
                case 1: // XZ plane
                    point = center + Vector(cos(angle) * radius, 0, sin(angle) * radius);
                    nextPoint = center + Vector(cos(nextAngle) * radius, 0, sin(nextAngle) * radius);
                    break;
                case 2: // YZ plane
                    point = center + Vector(0, cos(angle) * radius, sin(angle) * radius);
                    nextPoint = center + Vector(0, cos(nextAngle) * radius, sin(nextAngle) * radius);
                    break;
                }

                G_DebugLine(point, nextPoint, r, g, b, alpha);
            }
        }
    }

    void DrawCrosshair(Vector position, float size, float r, float g, float b, float alpha)
    {
        // Draw horizontal line
        G_DebugLine(position + Vector(-size, 0, 0), position + Vector(size, 0, 0), r, g, b, alpha);

        // Draw vertical line
        G_DebugLine(position + Vector(0, -size, 0), position + Vector(0, size, 0), r, g, b, alpha);

        // Draw depth line
        G_DebugLine(position + Vector(0, 0, -size), position + Vector(0, 0, size), r, g, b, alpha);
    }

    void DrawText3D(Vector position, const char *text, float scale, float r, float g, float b)
    {
        G_DebugString(position, scale, r, g, b, "%s", text);
    }

    void DrawArrow(Vector start, Vector end, float r, float g, float b, float alpha)
    {
        // Draw main line
        G_DebugArrow(start, end - start, (end - start).length(), r, g, b, alpha);
    }

    void DrawBox(Vector mins, Vector maxs, float r, float g, float b, float alpha)
    {
        // Draw the 12 edges of a box
        Vector corners[8];
        corners[0] = Vector(mins.x, mins.y, mins.z);
        corners[1] = Vector(maxs.x, mins.y, mins.z);
        corners[2] = Vector(maxs.x, maxs.y, mins.z);
        corners[3] = Vector(mins.x, maxs.y, mins.z);
        corners[4] = Vector(mins.x, mins.y, maxs.z);
        corners[5] = Vector(maxs.x, mins.y, maxs.z);
        corners[6] = Vector(maxs.x, maxs.y, maxs.z);
        corners[7] = Vector(mins.x, maxs.y, maxs.z);

        // Bottom face
        G_DebugLine(corners[0], corners[1], r, g, b, alpha);
        G_DebugLine(corners[1], corners[2], r, g, b, alpha);
        G_DebugLine(corners[2], corners[3], r, g, b, alpha);
        G_DebugLine(corners[3], corners[0], r, g, b, alpha);

        // Top face
        G_DebugLine(corners[4], corners[5], r, g, b, alpha);
        G_DebugLine(corners[5], corners[6], r, g, b, alpha);
        G_DebugLine(corners[6], corners[7], r, g, b, alpha);
        G_DebugLine(corners[7], corners[4], r, g, b, alpha);

        // Vertical edges
        G_DebugLine(corners[0], corners[4], r, g, b, alpha);
        G_DebugLine(corners[1], corners[5], r, g, b, alpha);
        G_DebugLine(corners[2], corners[6], r, g, b, alpha);
        G_DebugLine(corners[3], corners[7], r, g, b, alpha);
    }

    void DrawCube(Vector center, float size, float r, float g, float b, float alpha)
    {
        Vector mins = center - Vector(size, size, size);
        Vector maxs = center + Vector(size, size, size);
        DrawBox(mins, maxs, r, g, b, alpha);
    }

    void DrawCircle(Vector center, float radius, float r, float g, float b, float alpha, qboolean horizontal)
    {
        G_DebugCircle(center, radius, r, g, b, alpha, horizontal);
    }
}
