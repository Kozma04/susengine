#include "./mathutils.h"

void NormalizePlane(Plane *p) {
    float dist = sqrtf(p->a * p->a + p->b * p->b + p->c * p->c);
    p->a /= dist;
    p->b /= dist;
    p->c /= dist;
    p->d /= dist;
}

void ExtractFrustumPlanes(Frustum *frustum, Matrix comboMatrix,
                          bool normalize) {
    // comboMatrix = MatrixTranspose(comboMatrix);
    Plane *p_planes = frustum->plane;
    // Left clipping plane
    p_planes[0].a = comboMatrix.m3 + comboMatrix.m0;
    p_planes[0].b = comboMatrix.m7 + comboMatrix.m4;
    p_planes[0].c = comboMatrix.m11 + comboMatrix.m8;
    p_planes[0].d = comboMatrix.m15 + comboMatrix.m12;
    // Right clipping plane
    p_planes[1].a = comboMatrix.m3 - comboMatrix.m0;
    p_planes[1].b = comboMatrix.m7 - comboMatrix.m4;
    p_planes[1].c = comboMatrix.m11 - comboMatrix.m8;
    p_planes[1].d = comboMatrix.m15 - comboMatrix.m12;
    // Top clipping plane
    p_planes[2].a = comboMatrix.m3 - comboMatrix.m1;
    p_planes[2].b = comboMatrix.m7 - comboMatrix.m5;
    p_planes[2].c = comboMatrix.m11 - comboMatrix.m9;
    p_planes[2].d = comboMatrix.m15 - comboMatrix.m13;
    // Bottom clipping plane
    p_planes[3].a = comboMatrix.m3 + comboMatrix.m1;
    p_planes[3].b = comboMatrix.m7 + comboMatrix.m5;
    p_planes[3].c = comboMatrix.m11 + comboMatrix.m9;
    p_planes[3].d = comboMatrix.m15 + comboMatrix.m13;
    // Near clipping plane
    p_planes[4].a = comboMatrix.m3 + comboMatrix.m2;
    p_planes[4].b = comboMatrix.m7 + comboMatrix.m6;
    p_planes[4].c = comboMatrix.m11 + comboMatrix.m10;
    p_planes[4].d = comboMatrix.m15 + comboMatrix.m14;
    // Far clipping plane
    p_planes[5].a = comboMatrix.m3 - comboMatrix.m2;
    p_planes[5].b = comboMatrix.m7 - comboMatrix.m6;
    p_planes[5].c = comboMatrix.m11 - comboMatrix.m10;
    p_planes[5].d = comboMatrix.m15 - comboMatrix.m14;
    // Normalize the plane equations, if requested
    if (normalize == 1) {
        NormalizePlane(p_planes);
        NormalizePlane(p_planes + 1);
        NormalizePlane(p_planes + 2);
        NormalizePlane(p_planes + 3);
        NormalizePlane(p_planes + 4);
        NormalizePlane(p_planes + 5);
    }
}

static Plane PlaneFromPoints(Vector3 v0, Vector3 v1, Vector3 v2) {
    Vector3 normal =
        Vector3CrossProduct(Vector3Subtract(v1, v0), Vector3Subtract(v2, v0));
    normal = Vector3Normalize(normal);
    return (Plane){normal.x, normal.y, normal.z,
                   -Vector3DotProduct(v0, normal)};
}

// https://cgvr.cs.uni-bremen.de/teaching/cg_literatur/lighthouse3d_view_frustum_culling/index.html
Frustum GetCameraFrustum(Camera cam, float ratio) {
    const static float nearD = .01f, farD = 1000.f;
    // const float ratio = (float)GetScreenWidth() / GetScreenHeight();

    Frustum fr;

    float tang = (float)tan(DEG2RAD * cam.fovy * 0.5);
    float nh =
        cam.projection == CAMERA_PERSPECTIVE ? nearD * tang : cam.fovy / 2;
    float nw = nh * ratio;
    float fh =
        cam.projection == CAMERA_PERSPECTIVE ? farD * tang : cam.fovy / 2;
    float fw = fh * ratio;

    Vector3 dir, nc, fc, X, Y, Z;

    // compute the Z axis of camera
    // this axis points in the opposite direction from
    // the looking direction
    Z = Vector3Normalize(Vector3Subtract(cam.position, cam.target));

    // X axis of camera with given "up" vector and Z axis
    X = Vector3Normalize(Vector3CrossProduct(cam.up, Z));

    // the real "up" vector is the cross product of Z and X
    Y = Vector3CrossProduct(Z, X);

    // compute the centers of the near and far planes
    nc = Vector3Subtract(cam.position, Vector3Scale(Z, nearD));
    fc = Vector3Subtract(cam.position, Vector3Scale(Z, farD));

    // compute the 4 corners of the frustum on the near plane
    Vector3 ntl =
        (Vector3){nc.x + Y.x * nh - X.x * nw, nc.y + Y.y * nh - X.y * nw,
                  nc.z + Y.z * nh - X.z * nw};
    Vector3 ntr =
        (Vector3){nc.x + Y.x * nh + X.x * nw, nc.y + Y.y * nh + X.y * nw,
                  nc.z + Y.z * nh + X.z * nw};
    Vector3 nbl =
        (Vector3){nc.x - Y.x * nh - X.x * nw, nc.y - Y.y * nh - X.y * nw,
                  nc.z - Y.z * nh - X.z * nw};
    Vector3 nbr =
        (Vector3){nc.x - Y.x * nh + X.x * nw, nc.y - Y.y * nh + X.y * nw,
                  nc.z - Y.z * nh + X.z * nw};

    // compute the 4 corners of the frustum on the far plane
    Vector3 ftl =
        (Vector3){fc.x + Y.x * fh - X.x * fw, fc.y + Y.y * fh - X.y * fw,
                  fc.z + Y.z * fh - X.z * fw};
    Vector3 ftr =
        (Vector3){fc.x + Y.x * fh + X.x * fw, fc.y + Y.y * fh + X.y * fw,
                  fc.z + Y.z * fh + X.z * fw};
    Vector3 fbl =
        (Vector3){fc.x - Y.x * fh - X.x * fw, fc.y - Y.y * fh - X.y * fw,
                  fc.z - Y.z * fh - X.z * fw};
    Vector3 fbr =
        (Vector3){fc.x - Y.x * fh + X.x * fw, fc.y - Y.y * fh + X.y * fw,
                  fc.z - Y.z * fh + X.z * fw};

    fr.plane[0] = PlaneFromPoints(ntr, ntl, ftl); // top
    fr.plane[1] = PlaneFromPoints(nbl, nbr, fbr); // bottom
    fr.plane[2] = PlaneFromPoints(ntl, nbl, fbl); // left
    fr.plane[3] = PlaneFromPoints(nbr, ntr, fbr); // right
    fr.plane[4] = PlaneFromPoints(ntl, ntr, nbr); // near
    fr.plane[5] = PlaneFromPoints(ftr, ftl, fbl); // far

    return fr;
}

int FrustumBoxIntersect(Frustum frustum, BoundingBox box) {
    int ret = 0; // inside
    Vector3 vmin, vmax, normal;
    Plane *planes = frustum.plane;

    for (int i = 0; i < 6; ++i) {
        normal = (Vector3){-planes[i].a, -planes[i].b, -planes[i].c};
        // X axis
        if (normal.x > 0) {
            vmin.x = box.min.x;
            vmax.x = box.max.x;
        } else {
            vmin.x = box.max.x;
            vmax.x = box.min.x;
        }
        // Y axis
        if (normal.y > 0) {
            vmin.y = box.min.y;
            vmax.y = box.max.y;
        } else {
            vmin.y = box.max.y;
            vmax.y = box.min.y;
        }
        // Z axis
        if (normal.z > 0) {
            vmin.z = box.min.z;
            vmax.z = box.max.z;
        } else {
            vmin.z = box.max.z;
            vmax.z = box.min.z;
        }
        if (Vector3DotProduct(normal, vmin) - planes[i].d > 0)
            return 2; // outside
        if (Vector3DotProduct(normal, vmax) - planes[i].d >= 0)
            ret = 1; // intersect
    }
    return ret;
}

int FrustumPointIntersect(Frustum frustum, Vector3 point) {
    Plane *planes = frustum.plane;
    for (int i = 0; i < 6; i++) {
        if (planes[i].a * point.x + planes[i].b * point.y +
                planes[i].c * point.z + planes[i].d <=
            0)
            return 0;
    }
    return 1;
}

BoundingBox BoxTransform(BoundingBox box, Matrix trans) {
    Vector3 vec[8] = {
        {box.min.x, box.min.y, box.min.z}, {box.min.x, box.min.y, box.max.z},
        {box.min.x, box.max.y, box.min.z}, {box.min.x, box.max.y, box.max.z},
        {box.max.x, box.min.y, box.min.z}, {box.max.x, box.min.y, box.max.z},
        {box.max.x, box.max.y, box.min.z}, {box.max.x, box.max.y, box.max.z}};
    Vector3 min = (Vector3){FLT_MAX, FLT_MAX, FLT_MAX};
    Vector3 max = (Vector3){-FLT_MAX, -FLT_MAX, -FLT_MAX};

    for (int i = 0; i < 8; i++) {
        vec[i] = Vector3Transform(vec[i], trans);
        if (vec[i].x < min.x)
            min.x = vec[i].x;
        if (vec[i].x > max.x)
            max.x = vec[i].x;
        if (vec[i].y < min.y)
            min.y = vec[i].y;
        if (vec[i].y > max.y)
            max.y = vec[i].y;
        if (vec[i].z < min.z)
            min.z = vec[i].z;
        if (vec[i].z > max.z)
            max.z = vec[i].z;
    }

    return (BoundingBox){min, max};
}

uint8_t BoxIntersect(BoundingBox a, BoundingBox b) {
    return (a.min.x <= b.max.x && a.max.x >= b.min.x) &&
           (a.min.y <= b.max.y && a.max.y >= b.min.y) &&
           (a.min.z <= b.max.z && a.max.z >= b.min.z);
}

Vector3 TriGetNormal(Vector3 v0, Vector3 v1, Vector3 v2) {
    Vector3 v1v0 = Vector3Subtract(v1, v0);
    Vector3 v2v0 = Vector3Subtract(v2, v0);
    Vector3 normal = Vector3CrossProduct(v1v0, v2v0);
    return Vector3Normalize(normal);
}

Plane PlaneFromTri(Vector3 v0, Vector3 v1, Vector3 v2) {
    Plane p;
    Vector3 normal = TriGetNormal(v0, v1, v2);
    p.d = -Vector3DotProduct(v0, normal);
    p.a = normal.x, p.b = normal.y, p.c = normal.z;
    return p;
}

float DistanceFromPlane(Plane plane, Vector3 in) {
    return Vector3DotProduct(in, (Vector3){plane.a, plane.b, plane.c}) +
           plane.d;
}

Vector3 ProjectPointOntoPlane(Plane plane, Vector3 point) {
    float dist = DistanceFromPlane(plane, point);
    return Vector3Subtract(
        point, Vector3Scale((Vector3){plane.a, plane.b, plane.c}, dist));
}

Vector3 Vector3SetLength(Vector3 vec, float length) {
    return Vector3Scale(Vector3Normalize(vec), length);
}

Matrix GetProjectionMatrix(Camera cam, float aspect, uint8_t zInvert) {
    Matrix proj = MatrixIdentity();

    if (cam.projection == CAMERA_PERSPECTIVE) {
        proj = MatrixPerspective(cam.fovy / 2 * DEG2RAD, aspect, .01f, 1000.f);
        if (zInvert) {
            proj.m10 *= -1;
            proj.m11 *= -1;
        }
    } else if (cam.projection == CAMERA_ORTHOGRAPHIC) {
        float top = cam.fovy / 2.0;
        float right = top * aspect;
        proj = MatrixOrtho(-right, right, -top, top, .01f, 1000.f);
    } else {
        logMsg(LOG_LVL_FATAL, "invalid camera projection: %d\n",
               cam.projection);
    }
    return proj;
}

Matrix GetViewMatrix(Camera cam) {
    Matrix mat = MatrixLookAt(cam.position, cam.target, cam.up);
    return mat;
}