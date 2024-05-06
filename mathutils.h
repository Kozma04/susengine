// https://gist.github.com/Kinwailo

#pragma once


#include "raylib.h"
#include <stdint.h>
#include <raymath.h>
#include <stdint.h>


typedef struct Plane {
    float a, b, c, d;
} Plane;

typedef struct Frustum {
    Plane plane[6];
} Frustum;


void NormalizePlane(Plane *p);

void ExtractFrustumPlanes(
    Frustum *frustum,
    const Matrix comboMatrix,
    bool normalize
);

Frustum GetCameraFrustum(Camera cam);

// Assumes that the normals of the planes point inwards.
int FrustumBoxIntersect(Frustum frustum, BoundingBox box);
// Assumes that the normals of the planes point inwards.
int FrustumPointIntersect(Frustum frustum, Vector3 point);

BoundingBox BoxTransform(BoundingBox box, Matrix trans);

Matrix GetProjectionMatrix(Camera cam, uint8_t zInvert);
Matrix GetViewMatrix(Camera cam);