// https://gist.github.com/Kinwailo

#pragma once

#include "./logger.h"
#include "raylib.h"
#include <float.h>
#include <raymath.h>
#include <stdint.h>
#include <stdio.h>

typedef struct Plane {
    float a, b, c, d;
} Plane;

typedef struct Frustum {
    Plane plane[6];
} Frustum;

void NormalizePlane(Plane *p);

void ExtractFrustumPlanes(Frustum *frustum, const Matrix comboMatrix,
                          bool normalize);

Frustum GetCameraFrustum(Camera cam, float aspect);

// Assumes that the normals of the planes point inwards.
int FrustumBoxIntersect(Frustum frustum, BoundingBox box);
// Assumes that the normals of the planes point inwards.
int FrustumPointIntersect(Frustum frustum, Vector3 point);

BoundingBox BoxTransform(BoundingBox box, Matrix trans);
uint8_t BoxIntersect(BoundingBox a, BoundingBox b);

Vector3 TriGetNormal(Vector3 v0, Vector3 v1, Vector3 v2);
Plane PlaneFromTri(Vector3 v0, Vector3 v1, Vector3 v2);
float DistanceFromPlane(Plane plane, Vector3 in);
Vector3 ProjectPointOntoPlane(Plane plane, Vector3 point);

Vector3 Vector3SetLength(Vector3 vec, float length);

Matrix GetProjectionMatrix(Camera cam, float aspect, uint8_t zInvert);
Matrix GetViewMatrix(Camera cam);