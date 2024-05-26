#include "./heightmap.h"

static inline void putVec(float *vertices, Vector3 vec) {
    vertices[0] = vec.x;
    vertices[1] = vec.y;
    vertices[2] = vec.z;
}

static inline Vector3 normalAtCell(Image hmap, int x, int y) {
    float ha = GetImageColor(hmap, x, y).r / 255.f;
    float hb = ha;
    float hc = ha;
    float hd = ha;

    if (x + 1 < hmap.width)
        hb = GetImageColor(hmap, x + 1, y).r / 255.f;
    if (y + 1 < hmap.height)
        hc = GetImageColor(hmap, x, y + 1).r / 255.f;
    if (x + 1 < hmap.width && y + 1 < hmap.height)
        hd = GetImageColor(hmap, x + 1, y + 1).r / 255.f;

    Vector3 va = (Vector3){(float)x, ha, (float)y};
    Vector3 vb = (Vector3){(float)x + 1, hb, (float)y};
    Vector3 vc = (Vector3){(float)x, hc, (float)y + 1};
    Vector3 nor = Vector3Scale(TriGetNormal(va, vc, vb), .5f);

    va = (Vector3){(float)x + 1, hb, (float)y};
    vb = (Vector3){(float)x + 1, hd, (float)y + 1};
    vc = (Vector3){(float)x, hc, (float)y + 1};
    nor = Vector3Add(nor, Vector3Scale(TriGetNormal(va, vc, vb), .5f));
    return nor;
}

static inline Vector3 normalAtPointSmooth(Image hmap, int x, int y) {
    int terms = 0;
    Vector3 nor = (Vector3){0, 0, 0};
    if (x <= hmap.width && y <= hmap.height) {
        terms = 1;
        nor = normalAtCell(hmap, x, y);
    }
    if (x > 0) {
        terms++;
        nor = Vector3Add(nor, normalAtCell(hmap, x - 1, y));
    }
    if (y > 0) {
        terms++;
        nor = Vector3Add(nor, normalAtCell(hmap, x, y - 1));
        if (x > 0) {
            terms++;
            nor = Vector3Add(nor, normalAtCell(hmap, x - 1, y - 1));
        }
    }

    if (terms == 0)
        return (Vector3){0, 0, 0};
    return Vector3Normalize(Vector3Scale(nor, 1.f / terms));
}

Mesh GenMeshHeightmapChunk(Image hmap, int px, int py, int w, int h) {
    Mesh mesh = {0};
    float ha, hb, hc, hd;
    Vector3 triA, triB, triC;
    Vector3 triNorA, triNorB, triNorC;
    mesh.triangleCount = 2 * (w - 1) * (h - 1);
    mesh.vertexCount = mesh.triangleCount * 3;
    mesh.vertices = (float *)malloc(sizeof(float) * mesh.vertexCount * 3);
    mesh.texcoords = (float *)malloc(sizeof(float) * mesh.vertexCount * 2);
    mesh.normals = (float *)malloc(sizeof(float) * mesh.vertexCount * 3);

    if (w < 1 || h < 1) {
        logMsg(LOG_LVL_FATAL, "invalid width/height: %d x %d", w, h);
        return mesh;
    }

    int vpos = 0, tpos = 0;
    for (int x = px; x < px + w - 1; x++) {
        for (int y = py; y < py + h - 1; y++) {
            ha = GetImageColor(hmap, x, y).r / 255.f;
            hb = GetImageColor(hmap, x + 1, y).r / 255.f;
            hc = GetImageColor(hmap, x, y + 1).r / 255.f;
            hd = GetImageColor(hmap, x + 1, y + 1).r / 255.f;

            // First triangle
            triA = (Vector3){(float)x, ha, (float)y};
            triB = (Vector3){(float)x, hc, (float)y + 1};
            triC = (Vector3){(float)x + 1, hb, (float)y};
            triNorA = normalAtPointSmooth(hmap, x, y);
            triNorB = normalAtPointSmooth(hmap, x, y + 1);
            triNorC = normalAtPointSmooth(hmap, x + 1, y);
            putVec(mesh.vertices + vpos, triA);
            putVec(mesh.vertices + vpos + 3, triB);
            putVec(mesh.vertices + vpos + 6, triC);
            putVec(mesh.normals + vpos, triNorA);
            putVec(mesh.normals + vpos + 3, triNorB);
            putVec(mesh.normals + vpos + 6, triNorC);
            mesh.texcoords[tpos] = (float)x / hmap.width;
            mesh.texcoords[tpos + 1] = (float)y / hmap.height;
            mesh.texcoords[tpos + 2] = (float)x / hmap.width;
            mesh.texcoords[tpos + 3] = (float)(y + 1) / hmap.height;
            mesh.texcoords[tpos + 4] = (float)(x + 1) / hmap.width;
            mesh.texcoords[tpos + 5] = (float)y / hmap.height;
            vpos += 9;
            tpos += 6;

            // Second triangle
            triA = (Vector3){(float)x + 1, hb, (float)y};
            triB = (Vector3){(float)x, hc, (float)y + 1};
            triC = (Vector3){(float)x + 1, hd, (float)y + 1};
            triNorA = normalAtPointSmooth(hmap, x + 1, y);
            triNorB = normalAtPointSmooth(hmap, x, y + 1);
            triNorC = normalAtPointSmooth(hmap, x + 1, y + 1);
            putVec(mesh.vertices + vpos, triA);
            putVec(mesh.vertices + vpos + 3, triB);
            putVec(mesh.vertices + vpos + 6, triC);
            putVec(mesh.normals + vpos, triNorA);
            putVec(mesh.normals + vpos + 3, triNorB);
            putVec(mesh.normals + vpos + 6, triNorC);
            mesh.texcoords[tpos] = (float)(x + 1) / hmap.width;
            mesh.texcoords[tpos + 1] = (float)y / hmap.height;
            mesh.texcoords[tpos + 2] = (float)x / hmap.width;
            mesh.texcoords[tpos + 3] = (float)(y + 1) / hmap.height;
            mesh.texcoords[tpos + 4] = (float)(x + 1) / hmap.width;
            mesh.texcoords[tpos + 5] = (float)(y + 1) / hmap.height;
            vpos += 9;
            tpos += 6;
        }
    }

    UploadMesh(&mesh, 0);

    return mesh;
}