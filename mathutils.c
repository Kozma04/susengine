#include "mathutils.h"


void NormalizePlane(Plane *p) {
    float dist = sqrtf(p->a * p->a + p->b * p->b + p->c * p->c);
    p->a /= dist;
    p->b /= dist;
    p->c /= dist;
    p->d /= dist;
}


void ExtractFrustumPlanes(
    Frustum *frustum,
    Matrix comboMatrix,
    bool normalize
) {
    //comboMatrix = MatrixTranspose(comboMatrix);
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
    if(normalize == 1)
    {
        NormalizePlane(p_planes);
        NormalizePlane(p_planes + 1);
        NormalizePlane(p_planes + 2);
        NormalizePlane(p_planes + 3);
        NormalizePlane(p_planes + 4);
        NormalizePlane(p_planes + 5);
    }
}

static Plane PlaneFromPoints(Vector3 v0, Vector3 v1, Vector3 v2) {
    Vector3 normal = Vector3CrossProduct(
        Vector3Subtract(v1, v0),
        Vector3Subtract(v2, v0)
    );
    normal = Vector3Normalize(normal);
    return (Plane){
        normal.x, normal.y, normal.z,
        -Vector3DotProduct(v0, normal)
    };
}

// https://cgvr.cs.uni-bremen.de/teaching/cg_literatur/lighthouse3d_view_frustum_culling/index.html
Frustum GetCameraFrustum(Camera cam, float ratio) {
    const static float nearD = .01f, farD = 1000.f;
    //const float ratio = (float)GetScreenWidth() / GetScreenHeight();

    Frustum fr;

    float tang = (float)tan(DEG2RAD * cam.fovy * 0.5);
	float nh = cam.projection == CAMERA_PERSPECTIVE ? nearD * tang : cam.fovy / 2;
	float nw = nh * ratio; 
	float fh = cam.projection == CAMERA_PERSPECTIVE ? farD * tang : cam.fovy / 2;
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
    Vector3 ntl = (Vector3){
        nc.x + Y.x * nh - X.x * nw,
        nc.y + Y.y * nh - X.y * nw,
        nc.z + Y.z * nh - X.z * nw
    };
    Vector3 ntr = (Vector3){
        nc.x + Y.x * nh + X.x * nw,
        nc.y + Y.y * nh + X.y * nw,
        nc.z + Y.z * nh + X.z * nw
    };
    Vector3 nbl = (Vector3){
        nc.x - Y.x * nh - X.x * nw,
        nc.y - Y.y * nh - X.y * nw,
        nc.z - Y.z * nh - X.z * nw
    };
    Vector3 nbr = (Vector3){
        nc.x - Y.x * nh + X.x * nw,
        nc.y - Y.y * nh + X.y * nw,
        nc.z - Y.z * nh + X.z * nw
    };

	// compute the 4 corners of the frustum on the far plane
    Vector3 ftl = (Vector3){
        fc.x + Y.x * fh - X.x * fw,
        fc.y + Y.y * fh - X.y * fw,
        fc.z + Y.z * fh - X.z * fw
    };
    Vector3 ftr = (Vector3){
        fc.x + Y.x * fh + X.x * fw,
        fc.y + Y.y * fh + X.y * fw,
        fc.z + Y.z * fh + X.z * fw
    };
    Vector3 fbl = (Vector3){
        fc.x - Y.x * fh - X.x * fw,
        fc.y - Y.y * fh - X.y * fw,
        fc.z - Y.z * fh - X.z * fw
    };
    Vector3 fbr = (Vector3){
        fc.x - Y.x * fh + X.x * fw,
        fc.y - Y.y * fh + X.y * fw,
        fc.z - Y.z * fh + X.z * fw
    };

    fr.plane[0] = PlaneFromPoints(ntr, ntl, ftl); // top
    fr.plane[1] = PlaneFromPoints(nbl, nbr, fbr); // bottom
    fr.plane[2] = PlaneFromPoints(ntl, nbl, fbl); // left
    fr.plane[3] = PlaneFromPoints(nbr, ntr, fbr); // right
    fr.plane[4] = PlaneFromPoints(ntl, ntr, nbr); // near
    fr.plane[5] = PlaneFromPoints(ftr, ftl, fbl); // far

    return fr;
}


int FrustumBoxIntersect(Frustum frustum, BoundingBox box) { 
   int ret = 0;  // inside
   Vector3 vmin, vmax, normal;
   Plane *planes = frustum.plane;

   for(int i = 0; i < 6; ++i) { 
        normal = (Vector3){-planes[i].a, -planes[i].b, -planes[i].c};
        // X axis
        if(normal.x > 0) { 
            vmin.x = box.min.x; 
            vmax.x = box.max.x; 
        } else { 
            vmin.x = box.max.x; 
            vmax.x = box.min.x; 
        }
        // Y axis
        if(normal.y > 0) { 
            vmin.y = box.min.y; 
            vmax.y = box.max.y; 
        } else { 
            vmin.y = box.max.y; 
            vmax.y = box.min.y; 
        }
        // Z axis
        if(normal.z > 0) { 
            vmin.z = box.min.z; 
            vmax.z = box.max.z; 
        } else { 
            vmin.z = box.max.z; 
            vmax.z = box.min.z; 
        }
        if(Vector3DotProduct(normal, vmin) - planes[i].d > 0) 
            return 2; // outside
        if(Vector3DotProduct(normal, vmax) - planes[i].d >= 0) 
            ret = 1; // intersect
    } 
    return ret;
}

int FrustumPointIntersect(Frustum frustum, Vector3 point) {
    Plane *planes = frustum.plane;
    for(int i = 0; i < 6; i++) {
        if(planes[i].a * point.x + planes[i].b * point.y + planes[i].c * point.z + planes[i].d <= 0)
            return 0;
    }
    return 1;
}


BoundingBox BoxTransform(BoundingBox box, Matrix trans) {
    BoundingBox res;
    box.min = Vector3Transform(box.min, trans);
    box.max = Vector3Transform(box.max, trans);
    
    if(box.max.x > box.min.x)
        res.min.x = box.min.x, res.max.x = box.max.x;
    else
        res.min.x = box.max.x, res.max.x = box.min.x;

    if(box.max.y > box.min.y)
        res.min.y = box.min.y, res.max.y = box.max.y;
    else
        res.min.y = box.max.y, res.max.y = box.min.y;

    if(box.max.z > box.min.z)
        res.min.z = box.min.z, res.max.z = box.max.z;
    else
        res.min.z = box.max.z, res.max.z = box.min.z;

    return res;
}


Matrix GetProjectionMatrix(Camera cam, float aspect, uint8_t zInvert) {
    Matrix proj = MatrixIdentity();
    
    if(cam.projection == CAMERA_PERSPECTIVE) {
        proj = MatrixPerspective(cam.fovy / 2 * DEG2RAD, aspect, .01f, 1000.f);
        if(zInvert) {
            proj.m10 *= -1;
            proj.m11 *= -1;
        }
    }
    else if(cam.projection == CAMERA_ORTHOGRAPHIC) {
        float top = cam.fovy / 2.0;
        float right = top * aspect;
        proj = MatrixOrtho(-right, right, -top, top, .01f, 1000.f);
    }
    else {
        logMsg(LOG_LVL_FATAL, "invalid camera projection: %d\n", cam.projection);
    }
    return proj;
}


Matrix GetViewMatrix(Camera cam) {
    Matrix mat = MatrixLookAt(cam.position, cam.target, cam.up);
    return mat;
}