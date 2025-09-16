/*
 * s3d.c
 * -----
 *
 * By Marcus.
 */

#include "s3d.h"
#include "renv.h"
#include "n7mm.h"
#include "syscmd.h"
#include "naalaa_image.h"
#include "windowing.h"

#include "math.h"
#include "stdlib.h"
#include "stdio.h"

#define COLORIZE_MESHES

#define STACK_SIZE 64
//#define MAX_PRIMS 8192
#define MAX_PRIMS 131072
#define FACE_MAX_POINTS 5

#define NONE 0

#define TRIANGLES 1
#define QUADS 2

/* Z buffer modes. */
#define Z_BUFFER_READ       1
#define Z_BUFFER_WRITE      2
#define Z_BUFFER_READ_WRITE 3

/* Sorting */
#define FAR_TO_NEAR 1
#define NEAR_TO_FAR 2

/* RenderFace. */
typedef struct {
    Image *texture;
    short pointCount;
    unsigned int color;
    float z; // used for sorting with painter's algorithm.
    float points[FACE_MAX_POINTS*6]; // xyzwuv.
} Face;

/* MeshFace. */
typedef struct {
    Image *texture;
    short pointCount;
    unsigned int color;
    int xyzw[FACE_MAX_POINTS];
    int uv[FACE_MAX_POINTS];
} MeshFace;

/* Mesh. */
typedef struct {
    int faceCount;
    int pointCount;
    int uvCount;
    int frameCount;
    float *xyzw; // xyzw.
    float *xyzwTrans; // xyzw transformed.
    float *uv;
    MeshFace *faces;
} Mesh;

/* Destination image. In most cases this is n7's primary image. Since the primary image changes if
   'set window' is called or the window is resized we can't keep a reference to the actual image.
   Instead we store the identifier and fetch it through the windowing api when needed. */
static int sDstImageId = SYS_PRIMARY_IMAGE;
static Image *sDstImage;
static int *sZBuffer = 0;
/*static int sZBufferWidth = 0, sZBufferHeight = 0;*/
static float sZMin = 0.1f, sZMax = 10.0f, sZMaxFix = (int)(10.0f*65536.0f);
static int sSorting = NONE; 
static int sDepthBuffer = Z_BUFFER_READ_WRITE;

static Mesh **sMeshes = 0;
static int sMeshSize = 0;
static int sRenderMesh = -1;

/* Projection, transformation and operation matrices. */
static float sProjMat[16];
static float sTransMat[16];
static float sTransOpMat[16];
static float sOpMat[16];
/* Transformation stack. */
static float sTransMatStack[16*STACK_SIZE];
static int sTransMatStackPos = 0;
/* List of faces to be rendered. */
static Face sPrims[MAX_PRIMS];
static Face *sPrimRefs[MAX_PRIMS];
static int sPrimCount = 0;
static Face sClippedFace;

/* Current primitive. */
static int sPrimType = 0;
static short sPrimVertexCount = 0;
static Image *sTexture = 0;
static unsigned int sColor = 0x80ffffff;
static unsigned char sRed = 255, sGreen = 255, sBlue = 255, sAlpha = 128;
static char sAdditive = 0;

void RenderFace(Face *face);
Face *ZMinClippedFace(Face *face); 

void ValidateTarget();
void ClearTransformation();
int ClearDepthBuffer();

void Mat4Copy(float *dst, float *src);
void Mat4MakeIdent(float *m);
void Mat4PostMul(float *m, float *n); 
void Mat4VecMult(float *dst, float x, float y, float z, float w, float *m);


/*
 * S3D_SetPerspectiveCorrection
 * ----------------------------
 * S3D_SetPerspectiveCorrection(S3D_NONE/S3D_FAST/S3D_NORMAL)
 */
Variable S3D_SetPerspectiveCorrection(int argc, Variable *argv) {
    Variable result;
    
    IMG_SetPerspectiveDiv((int)ToNumber(&argv[0]));
    
    result.type = VAR_UNSET;
    return result;
}

/*
 * S3D_SetView
 * -----------
 * S3D_SetView(img, fov, zmin, zmax)
 */
Variable S3D_SetView(int argc, Variable *argv) {
    Variable result;
    float fov, lence;
    
    sDstImageId = (int)ToNumber(&argv[0]);
    ValidateTarget();
    Mat4MakeIdent(sProjMat);
    fov = (float)ToNumber(&argv[1]);
    sZMin = (float)ToNumber(&argv[2]);
    sZMax = (float)ToNumber(&argv[3]);
    sZMaxFix = (int)(sZMax*65536.0f);
    lence = 1.0f/tanf(0.5f*fov);
    sProjMat[10] = (1.0f/(1.0f - (sZMin/sZMax)))/lence;
    sProjMat[11] = 1.0f/lence;
    sProjMat[14] = (-sZMin/(1.0f - (sZMin/sZMax)))/lence;
    sProjMat[15] = 0.0f;
    if (sDstImage) {
        Mat4MakeIdent(sTransOpMat);
        sTransOpMat[0] = (float)IMG_Height(sDstImage)/(float)IMG_Width(sDstImage);
        Mat4PostMul(sProjMat, sTransOpMat);
    }
    
    result.type = VAR_UNSET;
    return result;
}

/*
 * S3D_SetDepthBuffer
 * ------------------
 */
Variable S3D_SetDepthBuffer(int argc, Variable *argv) {
    Variable result;
    
    sDepthBuffer = (int)ToNumber(&argv[0]);
    if (!(sDepthBuffer >= NONE && sDepthBuffer <= Z_BUFFER_READ_WRITE)) {
        RuntimeError("S3D_SetDepthBuffer: Invalid value");
    }
    
    result.type = VAR_UNSET;
    return result;
}

/*
 * S3D_SetSorting
 * --------------
 */
Variable S3D_SetSorting(int argc, Variable *argv) {
    Variable result;
    
    sSorting = (int)ToNumber(&argv[0]);
    if (!(sSorting >= NONE && sSorting <= NEAR_TO_FAR)) {
        RuntimeError("S3D_SetSorting: Invalid value");
    }
    
    result.type = VAR_UNSET;
    return result;
}

/*
 * S3D_ClearTransformation
 * -----------------------
 * S3D_ClearTransformation()
 */
Variable S3D_ClearTransformation(int argc, Variable *argv) {
    Variable result;

    ClearTransformation();

    result.type = VAR_UNSET;
    return result;
}

/*
 * S3D_ClearDepthBuffer
 * --------------------
 * S3D_ClearDepthBuffer()
 */
Variable S3D_ClearDepthBuffer(int argc, Variable *argv) {
    Variable result;

    if (!ClearDepthBuffer()) {
        RuntimeError("S3D_ClearDepthBuffer: No target image");
    }

    result.type = VAR_UNSET;
    return result;
}

/*
 * S3D_Clear
 * ---------
 * S3D_Clear()
 */
Variable S3D_Clear(int argc, Variable *argv) {
    Variable result;
    result.type = VAR_UNSET;

    if (!ClearDepthBuffer()) {
        RuntimeError("S3D_Clear: No target image");
        return result;
    }
    if (sPrimType) {
        RuntimeError("S3D_Clear: Called within S3D_Begin/S3D_End");
        return result;
    }
    ClearTransformation();

    sPrimCount = 0;
    sPrimVertexCount = 0;
    sTransMatStackPos = 0;

    sRed = 255;
    sGreen = 255;
    sBlue = 255;
    sAlpha = 128;
    sColor = ToRGB(sRed, sGreen, sBlue);
    sAdditive = 0;

    return result;
}


/*
 * S3D_Translate
 * -------------
 * S3D_Translate(x, y, z)
 */    
Variable S3D_Translate(int argc, Variable *argv) {
    Variable result;

    Mat4MakeIdent(sTransOpMat);
    sTransOpMat[12] = (float)ToNumber(&argv[0]);
    sTransOpMat[13] = (float)ToNumber(&argv[1]);
    sTransOpMat[14] = (float)ToNumber(&argv[2]);
    Mat4PostMul(sTransMat, sTransOpMat);

    result.type = VAR_UNSET;
    return result;
}

/*
 * S3D_RotateX
 * -----------
 * S3D_RotateX(a)
 */
Variable S3D_RotateX(int argc, Variable *argv) {
    Variable result;
    float a = (float)ToNumber(&argv[0]);
    float c = cosf(a), s = sinf(a);

    Mat4MakeIdent(sTransOpMat);
    sTransOpMat[5] = c;
    sTransOpMat[6] = s;
    sTransOpMat[9] = -s;
    sTransOpMat[10] = c;
    Mat4PostMul(sTransMat, sTransOpMat);

    result.type = VAR_UNSET;
    return result;
} 

/*
 * S3D_RotateY
 * -----------
 * S3D_RotateY(a)
 */
Variable S3D_RotateY(int argc, Variable *argv) {
    Variable result;
    float a = (float)ToNumber(&argv[0]);
    float c = cosf(a), s = sinf(a);

    Mat4MakeIdent(sTransOpMat);
    sTransOpMat[0] = c;
    sTransOpMat[2] = -s;
    sTransOpMat[8] = s;
    sTransOpMat[10] = c;
    Mat4PostMul(sTransMat, sTransOpMat);

    result.type = VAR_UNSET;
    return result;
}

/*
 * S3D_RotateZ
 * -----------
 * S3D_RotateZ(a)
 */
Variable S3D_RotateZ(int argc, Variable *argv) {
    Variable result;
    float a = (float)ToNumber(&argv[0]);
    float c = cosf(a), s = sinf(a);

    Mat4MakeIdent(sTransOpMat);
    sTransOpMat[0] = c;
    sTransOpMat[1] = s;
    sTransOpMat[4] = -s;
    sTransOpMat[5] = c;
    Mat4PostMul(sTransMat, sTransOpMat);

    result.type = VAR_UNSET;
    return result;
}

/*
 * S3D_Scale
 * ---------
 * S3D_Scale(x, y, z)
 */
Variable S3D_Scale(int argc, Variable *argv) {
    Variable result;
    
    Mat4MakeIdent(sTransOpMat);
    sTransOpMat[0] = (float)ToNumber(&argv[0]);
    sTransOpMat[5] = (float)ToNumber(&argv[1]);
    sTransOpMat[10] = (float)ToNumber(&argv[2]);
    Mat4PostMul(sTransMat, sTransOpMat);

    result.type = VAR_UNSET;
    return result;
}

/*
 * S3D_Push
 * --------
 * S3D_Push()
 */
Variable S3D_Push(int argc, Variable *argv) {
    Variable result;
    
    if (sTransMatStackPos < STACK_SIZE) {
        Mat4Copy(&sTransMatStack[sTransMatStackPos*16], sTransMat);
        sTransMatStackPos++;
    }
    else {
        RuntimeError("S3D_Push: Stack overflow");
    }

    result.type = VAR_UNSET;
    return result;
}

/*
 * S3D_Pop
 * -------
 * S3D_Pop()
 */
Variable S3D_Pop(int argc, Variable *argv) {
    Variable result;
    
    if (sTransMatStackPos > 0) {
        sTransMatStackPos--;
        Mat4Copy(sTransMat, &sTransMatStack[sTransMatStackPos*16]);
    }
    else {
        RuntimeError("S3D_Pop: Stack underflow");
    }

    result.type = VAR_UNSET;
    return result;
}

/*
 * S3D_Begin
 * ---------
 * S3D_Begin(type)
 */
Variable S3D_Begin(int argc, Variable *argv) {
    Variable result;

    if (!sPrimType) {
        sPrimType = (int)ToNumber(&argv[0]);
        if (sPrimType < 0 || sPrimType > 2) RuntimeError("S3D_Begin: Invalid type");
        sPrimVertexCount = 0;
    }
    else {
        RuntimeError("S3D_Begin: Missing S3D_End");
    }
    
    result.type = VAR_UNSET;
    return result;
}

/*
 * S3D_End
 * -------
 * S3D_End()
 */
Variable S3D_End(int argc, Variable *argv) {
    Variable result;

    if (sPrimType) sPrimType = 0;
    else RuntimeError("S3D_End: Unmatched S3D_End");

    result.type = VAR_UNSET;
    return result;
}

/*
 * S3D_Texture
 * -----------
 * S3D_Texture(img)
 */
Variable S3D_Texture(int argc, Variable *argv) {
    Variable result;
    
    if (argv[0].type == VAR_UNSET) sTexture = 0;
    else sTexture = (Image *)WIN_GetImage((int)ToNumber(&argv[0]));
    
    result.type = VAR_UNSET;
    return result;
}

/*
 * S3D_Color
 * ---------
 * S3D_Color(r, g, b[, a])
 */
Variable S3D_Color(int argc, Variable *argv) {
    Variable result;
    int r = (int)ToNumber(&argv[0]), g = (int)ToNumber(&argv[1]), b = (int)ToNumber(&argv[2]);
    int a = argc > 3 ? (int)ToNumber(&argv[3]) : 255;
    sRed = r < 0 ? 0 : r > 255 ? 255 : r;
    sGreen = g < 0 ? 0 : g > 255 ? 255 : g;
    sBlue = b < 0 ? 0 : b > 255 ? 255 : b;
    sAlpha = (a < 0 ? 0 : a > 255 ? 255 : a)*128/255;
    sColor = ToRGBA(sRed, sGreen, sBlue, sAlpha);
    result.type = VAR_UNSET;
    return result;
}

/*
 * S3D_Additive
 * ------------
 * S3D_Additive(value)
 */
Variable S3D_Additive(int argc, Variable *argv) {
    Variable result;
    sAdditive = (int)ToNumber(&argv[0]);
    result.type = VAR_UNSET;
    return result;
}

/*
 * S3D_TransformVector
 * -------------------
 * S3D_TransformVector(dst, src)
 */
Variable S3D_TransformVector(int argc, Variable *argv) {
    Variable result;
    Variable dst = argv[0];
    Variable src = argv[1];

    result.type = VAR_UNSET;
    
    if (dst.type != VAR_TBL) {
        RuntimeError("S3D_TransformVector: Expected array as first parameter");
        return result;
    }
    if (src.type != VAR_TBL) {
        RuntimeError("S3D_TransformVector: Expected array as second parameter");
        return result;
    }
    
    Variable *xvar = (Variable *)HT_Get(src.value.t, 0, 0);
    Variable *yvar = (Variable *)HT_Get(src.value.t, 0, 1);
    Variable *zvar = (Variable *)HT_Get(src.value.t, 0, 2);
    Variable *wvar = (Variable *)HT_Get(src.value.t, 0, 3);
    if (xvar->type == VAR_NUM && yvar->type == VAR_NUM && zvar->type == VAR_NUM && (!wvar || wvar->type == VAR_NUM)) {
        float v[4];

        v[0] = xvar->value.n;
        v[1] = yvar->value.n;
        v[2] = zvar->value.n;
        v[3] = wvar ? wvar->value.n : 1.0f;
        Mat4VecMult(v, v[0], v[1], v[2], v[3], sTransMat);
        
        for (int i = 0; i < 4; i++) {
            HashEntry *he = HT_GetOrCreateEntry(dst.value.t, 0, i);
            Variable *var;
            if (he->data) {
                var = (Variable *)he->data;
                if (var->type == VAR_STR) free(var->value.s);
            }
            else {
                he->data = var = (Variable *)MM_Malloc(sizeof(Variable)); 
            }
            var->type = VAR_NUM;
            var->value.n = v[i];
        }
    }
    else {
        RuntimeError("S3D_TransformVector: Invalid source vector");
    }
    
    return result;
}

/*
 * S3D_ProjectVector
 * -----------------
 * (NUM)S3D_ProjectVector(dst, src)
 */
Variable S3D_ProjectVector(int argc, Variable *argv) {
    Variable result;
    Variable dst = argv[0];
    Variable src = argv[1];

    result.type = VAR_NUM;
    result.value.n = 0;
    
    if (!sDstImage) {
        RuntimeError("S3D_ProjectVector: Invalid target image");
        return result;
    }
    
    if (dst.type != VAR_TBL) {
        RuntimeError("S3D_ProjectVector: Expected array as first parameter");
        return result;
    }
    if (src.type != VAR_TBL) {
        RuntimeError("S3D_ProjectVector: Expected array as second parameter");
        return result;
    }

    Variable *xvar = (Variable *)HT_Get(src.value.t, 0, 0);
    Variable *yvar = (Variable *)HT_Get(src.value.t, 0, 1);
    Variable *zvar = (Variable *)HT_Get(src.value.t, 0, 2);
    Variable *wvar = (Variable *)HT_Get(src.value.t, 0, 3);
    if (xvar->type == VAR_NUM && yvar->type == VAR_NUM && zvar->type == VAR_NUM && (!wvar || wvar->type == VAR_NUM)) {
        float v[4];
        v[0] = xvar->value.n;
        v[1] = yvar->value.n;
        v[2] = zvar->value.n;
        v[3] = wvar ? wvar->value.n : 1.0f;
        Mat4VecMult(v, v[0], v[1], v[2], v[3], sTransMat);
        if (v[2] > sZMin) {
            float z = v[2], wi;
            Mat4VecMult(v, v[0], v[1], v[2], v[3], sProjMat);
            wi = 1.0f/v[3];
            v[0] = (int)roundf((v[0]*wi + 1)*sDstImage->w*0.5f);
            v[1] = (int)roundf((v[1]*wi + 1)*sDstImage->h*0.5f);
            v[2] = z;            
            for (int i = 0; i < 4; i++) {
                HashEntry *he = HT_GetOrCreateEntry(dst.value.t, 0, i);
                Variable *var;
                if (he->data) {
                    var = (Variable *)he->data;
                    if (var->type == VAR_STR) free(var->value.s);
                }
                else {
                    he->data = var = (Variable *)MM_Malloc(sizeof(Variable)); 
                }
                var->type = VAR_NUM;
                var->value.n = v[i];
            }
            result.value.n = 1;
        }
    }
    else {
        RuntimeError("S3D_ProjectVector: Invalid source vector");
    }
    
    return result;
}

/*
 * S3D_ProjectFace
 * ---------------
 */
Variable S3D_ProjectFace(int argc, Variable *argv) {
    Variable result;
    Variable dst = argv[0];
    Variable src = argv[1];
    Face face;
    Variable *c;
    int zmins, zmaxs;
    int i;
    
    result.type = VAR_NUM;
    result.value.n = 0;
    
    if (!sDstImage) {
        RuntimeError("S3D_ProjectFace: Invalid target image");
        return result;
    }
    
    if (dst.type != VAR_TBL) {
        RuntimeError("S3D_ProjectFace: Expected array as first parameter");
        return result;
    }
    if (src.type != VAR_TBL) {
        RuntimeError("S3D_ProjectFace: Expected array as second parameter");
        return result;
    }

    face.pointCount = 0;
    i = 0;
    while ((c = (Variable *)HT_Get(src.value.t, 0, i))) {
        int offset = face.pointCount*6;
        
        if (face.pointCount > 4) {
            RuntimeError("S3D_ProjectFace: Invalid source array");
            return result;
        }
        
        if (c->type != VAR_NUM) {
            RuntimeError("S3D_ProjectFace: Invalid source array");
            return result;
        }

        face.points[offset + i%3] = c->value.n;
        i++;
        if (i%3 == 0) {
            face.points[offset + 3] = 1.0f;
            face.points[offset + 4] = 0.0f;
            face.points[offset + 5] = 0.0f;
            Mat4VecMult(&face.points[offset],
                    face.points[offset], face.points[offset + 1], face.points[offset + 2], face.points[offset + 3],
                    sTransMat);
            face.pointCount++;
        }
    }
    if (i%3 != 0 || face.pointCount < 3 || face.pointCount > 4) {
        RuntimeError("S3D_ProjectFace: Invalid source array");
        return result;
    }
    
    zmins = 0;
    zmaxs = 0;
    for (i = 0; i < face.pointCount; i++) {
        zmins += face.points[i*6 + 2] > sZMin;
        zmaxs += face.points[i*6 + 2] < sZMax;
    }
    if (!zmins || !zmaxs) return result;
    Face *f = &face;
    if (zmins < face.pointCount) f = ZMinClippedFace(f);
    if (!f->pointCount) return result;
    
    for (i = 0; i < f->pointCount; i++) {
        float *v = &f->points[i*6];
        float z = v[2];
        float wi;
        Mat4VecMult(v, v[0], v[1], v[2], v[3], sProjMat);
        wi = 1.0f/v[3];
        v[0] = (int)roundf((v[0]*wi + 1)*sDstImage->w*0.5f);
        v[1] = (int)roundf((v[1]*wi + 1)*sDstImage->h*0.5f);
        v[2] = z;
        
        for (int j = 0; j < 3; j++) {
            HashEntry *he = HT_GetOrCreateEntry(dst.value.t, 0, i*3 + j);
            Variable *var;
            if (he->data) {
                var = (Variable *)he->data;
                if (var->type == VAR_STR) free(var->value.s);
            }
            else {
                he->data = var = (Variable *)MM_Malloc(sizeof(Variable)); 
            }
            var->type = VAR_NUM;
            var->value.n = v[j];
        }
    }
    result.value.n = f->pointCount;
    
    return result;
}

/*
 * S3D_Vertex
 * ----------
 * S3D_Vertex(x, y, z, u, v)
 */
Variable S3D_Vertex(int argc, Variable *argv) {
    Variable result;
    
    if (sPrimType) {
        Face *face = &sPrims[sPrimCount];
        float *point = face->points + sPrimVertexCount*6;
        float u, v;

        Mat4VecMult(point,
                (float)ToNumber(&argv[0]), (float)ToNumber(&argv[1]), (float)ToNumber(&argv[2]), 1.0f,
                sTransMat);
        u = (float)ToNumber(&argv[3]);
        v = (float)ToNumber(&argv[4]);
        point[4] = u;
        point[5] = v;
        sPrimVertexCount++;
        if (sPrimType == TRIANGLES) {
            if (sPrimVertexCount%3 == 0) {
                face->texture = sTexture;
                face->color = sColor;
                face->pointCount = 3;
                face->z = (face->points[2] + face->points[8] + face->points[14])/3.0f;
                if (sRenderMesh < 0 && sSorting == NONE) RenderFace(face);
                else if (++sPrimCount >= MAX_PRIMS) RuntimeError("S3D_Vertex: Face buffer overflow");
                sPrimVertexCount = 0;
            }
        }
        else if (sPrimType == QUADS) {
            if (sPrimVertexCount%4 == 0) {
                face->texture = sTexture;
                face->color = sColor;
                face->pointCount = 4;
                face->z = (face->points[2] + face->points[8] + face->points[14] + face->points[20])/4.0f;
                if (sRenderMesh < 0 && sSorting == NONE) RenderFace(face);
                else if (++sPrimCount >= MAX_PRIMS) RuntimeError("S3D_Vertex: Face buffer overflow");
                sPrimVertexCount = 0;
            }
        }
    }
    else {
        RuntimeError("S3D_Vertex: Missing S3D_Begin");
    }
    
    result.type = VAR_UNSET;
    return result;
}

/*
 * BackToFrontCompareFaces
 * -----------------------
 */
int BackToFrontCompareFaces(const void *a, const void *b) {
    Face *af = *(Face **)a;
    Face *bf = *(Face **)b;
    if (af->z > bf->z) return -1;
    else if (af->z < bf->z) return 1;
    return 0;
}

/*
 * FrontToBackCompareFaces
 * -----------------------
 */
int FrontToBackCompareFaces(const void *a, const void *b) {
    Face *af = *(Face **)a;
    Face *bf = *(Face **)b;
    if (af->z < bf->z) return -1;
    else if (af->z > bf->z) return 1;
    return 0;
}

/*
 * S3D_Render
 * ----------
 * S3d_Render()
 */
Variable S3D_Render(int argc, Variable *argv) {
    Variable result;
    
    if (sRenderMesh >= 0) {
        RuntimeError("S3D_Render: Can't render while building mesh");
    }
    else if (sPrimCount) {
        if (sSorting == FAR_TO_NEAR) {
            for (int i = 0; i < sPrimCount; i++) sPrimRefs[i] = &sPrims[i];
            qsort(sPrimRefs, sPrimCount, sizeof(Face *), BackToFrontCompareFaces);
            for (int i = 0; i < sPrimCount; i++) RenderFace(sPrimRefs[i]);
        }
        else if (sSorting == NEAR_TO_FAR) {
            for (int i = 0; i < sPrimCount; i++) sPrimRefs[i] = &sPrims[i];
            qsort(sPrimRefs, sPrimCount, sizeof(Face *), FrontToBackCompareFaces);
            for (int i = 0; i < sPrimCount; i++) RenderFace(sPrimRefs[i]);
        }

        sPrimCount = 0;
        sPrimVertexCount = 0;
    }
    
    result.type = VAR_UNSET;
    return result;
}

Variable S3D_RenderFog(int argc, Variable *argv) {
    Variable result;
    
    if (sZBuffer && sDstImage) { /* && sZBufferWidth == sDstImage->w && sZBufferHeight == sDstImage->h) { */
        int fogR = (int)ToNumber(&argv[0]);
        int fogG = (int)ToNumber(&argv[1]);
        int fogB = (int)ToNumber(&argv[2]);
        unsigned int *buffer = sDstImage->buffer;
        int *zbuffer = sZBuffer;
        int size = sDstImage->w*sDstImage->h; /*sZBufferWidth*sZBufferHeight;*/
        
        fogR = fogR < 0 ? 0 : fogR > 255 ? 255 : fogR;
        fogG = fogG < 0 ? 0 : fogG > 255 ? 255 : fogG;
        fogB = fogB < 0 ? 0 : fogB > 255 ? 255 : fogB;
        
        /* Retro mode, only 8 shades? */
        if ((int)ToNumber(&argv[3])) {
            for (int i = 0; i < size; i++) {
                    int z = *zbuffer > sZMaxFix ? sZMaxFix : *zbuffer;
                    unsigned char a = (unsigned char)((z*8)/sZMaxFix);
                    unsigned char invA = 8 - a;
                    unsigned char srcR, srcG, srcB;
                    ColorToRGBComponents(*buffer, srcR, srcG, srcB);
                    *buffer = ToRGB((unsigned char)((fogR*a + srcR*invA) >> 3), (unsigned char)((fogG*a + srcG*invA) >> 3), (unsigned char)((fogB*a + srcB*invA) >> 3));
                buffer++;
                zbuffer++;
            }
        }
        else {
            for (int i = 0; i < size; i++) {
                //if (*zbuffer < 2147483647) {
                    int z = *zbuffer > sZMaxFix ? sZMaxFix : *zbuffer;
                    unsigned char a = (unsigned char)((z*128)/sZMaxFix);
                    unsigned char invA = 128 - a;
                    unsigned char srcR, srcG, srcB;
                    ColorToRGBComponents(*buffer, srcR, srcG, srcB);
                    *buffer = ToRGB((unsigned char)((fogR*a + srcR*invA) >> 7), (unsigned char)((fogG*a + srcG*invA) >> 7), (unsigned char)((fogB*a + srcB*invA) >> 7));
                //}
                buffer++;
                zbuffer++;
            }
        }
    }
    
    result.type = VAR_UNSET;
    return result;
}

/*
 * NewMeshId
 * ---------
 */
int NewMeshId() {
    if (!sMeshSize) {
        sMeshSize = 16;
        sMeshes = (Mesh **)malloc(sizeof(Mesh *)*sMeshSize);
        for (int i = 0; i < sMeshSize; i++) sMeshes[i] = 0;
    }
    /*for (sRenderMesh = 0; sRenderMesh < sMeshSize; sRenderMesh++) if (!sMeshes[sRenderMesh]) break;
    if (sRenderMesh == sMeshSize) {
        sMeshes = (Mesh **)realloc(sMeshes, sizeof(Mesh *)*(sMeshSize + 16));
        for (int i = sMeshSize; i < sMeshSize + 16; i++) sMeshes[i] = 0;
        sMeshSize += 16;
    }
    return sRenderMesh;*/
    int i;
    for (i = 0; i < sMeshSize; i++) if (!sMeshes[i]) break;
    if (i == sMeshSize) {
        sMeshes = (Mesh **)realloc(sMeshes, sizeof(Mesh *)*(sMeshSize + 16));
        for (int j = sMeshSize; j < sMeshSize + 16; j++) sMeshes[j] = 0;
        sMeshSize += 16;
    }
    return i;
}

/*
 * FreeMeshId
 * ----------
 */
void FreeMeshId(int id) {
    if (sMeshes && id >= 0 && id < sMeshSize && sMeshes[id]) {
        if (sMeshes[id]->xyzw) free(sMeshes[id]->xyzw);
        if (sMeshes[id]->xyzwTrans) free(sMeshes[id]->xyzwTrans);
        if (sMeshes[id]->uv) free(sMeshes[id]->uv);
        if (sMeshes[id]->faces) free(sMeshes[id]->faces);
        free(sMeshes[id]);
        sMeshes[id] = 0;
    }
}

/*
 * S3D_CreateMesh
 * --------------
 * (NUM) S3D_CreateMesh(vertexList, uvList, faceList)
 *   vertexList:    [[x, y, z], [x, y, z], ...]
 *   uvList:        [[u, v], [u, v], ...]
 *   materialList:  [[r, g, b], texture]
 *   faceList:      [[v0, v1, v2[, v3], uv, texture]]
 */
Variable S3D_CreateMesh(int argc, Variable *argv) {
    Variable result;
    Variable vertexList = argv[0];
    Variable uvList = argv[1];
    Variable matList = argv[2];
    Variable faceList = argv[3];
    int vertexCount;
    int uvCount;
    int matCount;
    int faceCount;
    Mesh *mesh;
    int meshId;
    int error = 0;
    
    result.type = VAR_UNSET;
    
    if (vertexList.type != VAR_TBL) {
        RuntimeError("S3D_CreateMesh: Expected array as first parameter");
        return result;
    }
    else if (uvList.type != VAR_TBL) {
        RuntimeError("S3D_CreateMesh: Expected array as second parameter");
        return result;
    }
    else if (!(matList.type == VAR_UNSET || matList.type == VAR_TBL)) {
        RuntimeError("S3D_CreateMesh: Expected array or unset as third parameter");
        return result;
    }
    else if (faceList.type != VAR_TBL) {
        RuntimeError("S3D_CreateMesh: Expected array as fourth parameter");
        return result;
    }
        
    vertexCount = HT_EntryCount(vertexList.value.t);
    uvCount = HT_EntryCount(uvList.value.t);
   
    faceCount = HT_EntryCount(faceList.value.t);
    if (vertexCount == 0) {
        RuntimeError("S3D_CreateMesh: Invalid vertex count");
        return result;
    }
    else if (uvCount == 0) {
        RuntimeError("S3D_CreateMesh: Invalid uv count");
        return result;
    }
    else if (faceCount == 0) {
        RuntimeError("S3D_CreateMesh: Invalid face count");
        return result;
    }

    mesh = (Mesh *)malloc(sizeof(Mesh));
    mesh->pointCount = vertexCount;
    mesh->uvCount = uvCount;
    mesh->faceCount = faceCount;
    mesh->frameCount = 1;
    mesh->xyzw = (float *)malloc(sizeof(float)*vertexCount*4);
    mesh->xyzwTrans = (float *)malloc(sizeof(float)*vertexCount*4);
    mesh->uv = (float *)malloc(sizeof(float)*uvCount*2);
    mesh->faces = (MeshFace *)malloc(sizeof(MeshFace)*faceCount);
    meshId = NewMeshId();
    sMeshes[meshId] = mesh;
    
    for (int i = 0; i < vertexCount && !error; i++) {
        Variable *xyzArray = (Variable *)HT_Get(vertexList.value.t, 0, i);
        if (xyzArray && xyzArray->type == VAR_TBL) {
            for (int j = 0; j < 3 && !error; j++) {
                Variable *xyz = (Variable *)HT_Get(xyzArray->value.t, 0, j);
                if (xyz && xyz->type == VAR_NUM) mesh->xyzw[i*4 + j] = (float)xyz->value.n;
                else error = 1;
            }
            mesh->xyzw[i*4 + 3] = 1.0f;
        }
        else {
            error = 1;
        }
    }
    if (error) {
        RuntimeError("S3D_CreateMesh: Invalid vertex array");
        FreeMeshId(meshId);
        return result;
    }
    
    for (int i = 0; i < uvCount && !error; i++) {
        Variable *uvArray = (Variable *)HT_Get(uvList.value.t, 0, i);
        if (uvArray && uvArray->type == VAR_TBL) {
            for (int j = 0; j < 2 && !error; j++) {
                Variable *uv = (Variable *)HT_Get(uvArray->value.t, 0, j);
                if (uv && uv->type == VAR_NUM) mesh->uv[i*2 + j] = (float)uv->value.n;
                else error = 1;
            }
        }
        else {
            error = 1;
        }
    }
    if (error) {
        RuntimeError("S3D_CreateMesh: Invalid uv array");
        FreeMeshId(meshId);
        return result;
    }

    if (matList.type == VAR_TBL) {
        matCount = HT_EntryCount(matList.value.t);
        for (int i = 0; i < matCount && !error; i++) {
            Variable *material = (Variable *)HT_Get(matList.value.t, 0, i);
            if (material && material->type == VAR_TBL) {
                Variable *color = (Variable *)HT_Get(material->value.t, 0, 0);
                Variable *texture = (Variable *)HT_Get(material->value.t, 0, 1);
                if (color) {
                    if (color->type == VAR_TBL) {
                        /*for (int j = 0; j < 3 && !error; j++) {
                            Variable *c = (Variable *)HT_Get(color->value.t, 0, j);
                            if (!(c && c->type == VAR_NUM)) error = 1;
                        }*/
                        for (int j = 0; j < 4 && !error; j++) {
                            Variable *c = (Variable *)HT_Get(color->value.t, 0, j);
                            if ((!c && j < 3) || (c && c->type != VAR_NUM)) error = 1;
                        }
                    }
                    else if (color->type != VAR_UNSET) {
                        error = 1;
                    }
                }
                else {
                    error = 1;
                }
                if (!(texture && (texture->type == VAR_NUM || texture->type == VAR_UNSET))) {
                    error = 1;
                }
            }
            else {
                error = 1;
            }
        }
    }
    else {
        matCount = 0;
    }
    if (error) {
        RuntimeError("S3D_CreateMesh: Invalid material array");
        FreeMeshId(meshId);
        return result;        
    }

    
    for (int i = 0; i < faceCount && !error; i++) {
        Variable *faceArray = (Variable *)HT_Get(faceList.value.t, 0, i);
        if (faceArray && faceArray->type == VAR_TBL) {
            int count = HT_EntryCount(faceArray->value.t);
            // 7: [v0, v1, v2, uv0, uv1, uv2, material]
            // 9: [v0, v1, v2, v3, uv0, uv1, uv2, uv3, material]
            if (count == 7 || count == 9) {
                Variable *material = (Variable *)HT_Get(faceArray->value.t, 0, count - 1);
                if (material) {
                    if (material->type == VAR_NUM) {
                        if (matList.type == VAR_TBL) {
                            material = (Variable *)HT_Get(matList.value.t, 0, (int)material->value.n);
                            if (material) {
                                Variable *color = (Variable *)HT_Get(material->value.t, 0, 0);
                                Variable *texture = (Variable *)HT_Get(material->value.t, 0, 1);
                                if (color && color->type == VAR_TBL) {
                                    Variable *rvar = (Variable *)HT_Get(color->value.t, 0, 0);
                                    Variable *gvar = (Variable *)HT_Get(color->value.t, 0, 1);
                                    Variable *bvar = (Variable *)HT_Get(color->value.t, 0, 2);
                                    Variable *avar = (Variable *)HT_Get(color->value.t, 0, 3);
                                    if (rvar && rvar->type == VAR_NUM && gvar && gvar->type == VAR_NUM && bvar && bvar->type == VAR_NUM) {
                                        int r = (int)rvar->value.n, g = (int)gvar->value.n, b = (int)bvar->value.n;
                                        int a = avar && avar->type == VAR_NUM ? (int)avar->value.n : 255;
                                        r = r < 0 ? 0 : r > 255 ? 255 : r;
                                        g = g < 0 ? 0 : g > 255 ? 255 : g;
                                        b = b < 0 ? 0 : b > 255 ? 255 : b;
                                        a = (a < 0 ? 0 : a > 255 ? 255 : a)*128/255;
                                        mesh->faces[i].color = ToRGBA(r, g, b, a);
                                    }
                                    else {
                                        error = 1;
                                    }
                                }
                                else if (color && color->type == VAR_UNSET) {
                                    //mesh->faces[i].color = 0;
                                    mesh->faces[i].color = ToRGB(255, 255, 255);
                                }
                                else {
                                    error = 1;
                                }
                                if (texture && texture->type == VAR_NUM) {
                                    mesh->faces[i].texture = (Image *)WIN_GetImage((int)texture->value.n);
                                }
                                else if (texture && texture->type == VAR_UNSET) {
                                    mesh->faces[i].texture = 0;
                                }
                                else {
                                    error = 1;
                                }
                            }
                            else {
                                error = 1;
                            }
                        }
                        else {
                            error = 1;
                        }
                    }
                    else if (material->type == VAR_UNSET) {
                        //mesh->faces[i].color = 0;
                        mesh->faces[i].color = ToRGB(255, 255, 255);
                        mesh->faces[i].texture = 0;
                    }
                    else {
                        error = 1;
                    }
                }
                else {
                    error = 1;
                }
                mesh->faces[i].pointCount = (short)(count - 1)/2;
                for (int j = 0; j < mesh->faces[i].pointCount && !error; j++) {
                    Variable *vertex = (Variable *)HT_Get(faceArray->value.t, 0, j);
                    Variable *uv = (Variable *)HT_Get(faceArray->value.t, 0, mesh->faces[i].pointCount + j); 
                    if (vertex && vertex->type == VAR_NUM) {
                        mesh->faces[i].xyzw[j] = (int)vertex->value.n;
                        if (mesh->faces[i].xyzw[j] < 0 || mesh->faces[i].xyzw[j] >= vertexCount) error = 1;
                    }
                    else error = 1;
                    if (uv && uv->type == VAR_NUM) {
                        mesh->faces[i].uv[j] = (int)uv->value.n;
                        if (mesh->faces[i].uv[j] < 0 || mesh->faces[i].uv[j] >= uvCount) error = 1;
                    }
                    else if (uv && uv->type == VAR_UNSET) {
                        mesh->faces[i].uv[j] = 0; // ugh, assume it has no texture?
                    }
                    else error = 1;
                }
            }
            else {
                error = 1;
            }
        }
        else {
            error = 1;
        }
    }
    if (error) {
        RuntimeError("S3D_CreateMesh: Invalid face array");
        FreeMeshId(meshId);
        return result;
    }
    
    if (!error) {
        result.type = VAR_NUM;
        result.value.n = meshId + 1;
    }
    
    
    return result;
}

/*
 * S3D_AddMeshFrame
 * ----------------
 * S3D_AddMeshFrame(mesh, vertexList)
 */
Variable S3D_AddMeshFrame(int argc, Variable *argv) {
    Variable result;
    int meshId = (int)ToNumber(&argv[0]) - 1;
    Mesh *mesh;
    Variable vertexList = argv[1];
    Variable *vertex;
    int vertexCount = 0;
    int error = 0;
    
    result.type = VAR_UNSET;
    
    if (meshId < 0 || meshId >= sMeshSize || sMeshes[meshId] == 0) {
        RuntimeError("S3D_AddMeshFrame: Invalid mesh");
        return result;
    }
    mesh = sMeshes[meshId];
    
    if (vertexList.type != VAR_TBL) {
        RuntimeError("S3D_AddMeshFrame: Expected array as second parameter");
        return result;
    }
    error = 0;
    while ((vertex = (Variable *)HT_Get(vertexList.value.t, 0, vertexCount)) && !error) {
        if (vertex->type == VAR_TBL) {
            for (int i = 0; i < 3; i++) {
                Variable *c = (Variable *)HT_Get(vertex->value.t, 0, i);
                if (!(c && c->type == VAR_NUM)) {
                    error = 1;
                    break;
                }
            }
        }
        else {
            error = 1;
        }  
        vertexCount++;
    }
    if (error || vertexCount != mesh->pointCount) {
        RuntimeError("S3D_AddMeshFrame: Invalid vertex array");
        return result;
    }
    
    mesh->frameCount++;
    mesh->xyzw = (float *)realloc(mesh->xyzw, sizeof(float)*4*mesh->pointCount*mesh->frameCount);
    float *p = &mesh->xyzw[mesh->pointCount*4*(mesh->frameCount - 1)];
    for (int i = 0; i < vertexCount; i++) {
        vertex = (Variable *)HT_Get(vertexList.value.t, 0, i);
        Variable *x = (Variable *)HT_Get(vertex->value.t, 0, 0);
        Variable *y = (Variable *)HT_Get(vertex->value.t, 0, 1);
        Variable *z = (Variable *)HT_Get(vertex->value.t, 0, 2);
        p[0] = x->value.n;
        p[1] = y->value.n;
        p[2] = z->value.n;
        p[3] = 1.0f;
        p += 4;
    }
    
    return result;
}


/*
 * S3D_BeginMesh
 * -------------
 * (NUM) S3D_BeginMesh()
 */
Variable S3D_BeginMesh(int argc, Variable *argv) {
    Variable result;
    
    if (sRenderMesh >= 0) {
        RuntimeError("S3D_BeginMesh: Already building mesh");
        result.type = VAR_UNSET;
    }
    else if (sPrimType) {
        RuntimeError("S3D_BeginMesh: Called within S3D_Begin/S3D_End");
    }
    else {
        sRenderMesh = NewMeshId();
        Mat4MakeIdent(sTransMat);
        sPrimCount = 0;
        sPrimVertexCount = 0;
        sTransMatStackPos = 0;
        sRed = 255;
        sGreen = 255;
        sBlue = 255;
        sAlpha = 128;
        sColor = ToRGBA(sRed, sGreen, sBlue, sAlpha);
        result.type = VAR_NUM;
        result.value.n = sRenderMesh + 1;
    }

    return result;
}

/*
 * S3D_EndMesh()
 * -------------
 * NUM S3D_EndMesh()
 */
Variable S3D_EndMesh(int argc, Variable *argv) {
    Variable result;
    
    result.type = VAR_UNSET;
    
    if (sRenderMesh < 0) {
        RuntimeError("S3D_EndMesh: Not building mesh");
    }
    else if (!sPrimCount) {
        RuntimeError("S3D_EndMesh: Nothing to build");
    }
    else if (sPrimType) {
        RuntimeError("S3D_EndMesh: Called within S3D_Begin/S3D_End");
    }
    else {
        /* Count unique points. */
        int uniqueXYZCount = 0;
        int uniqueUVCount = 0;
        for (int curPrim = 0; curPrim < sPrimCount; curPrim++) {
            for (int curPrimPoint = 0; curPrimPoint < sPrims[curPrim].pointCount; curPrimPoint++) {
                float *curXYZ = &sPrims[curPrim].points[curPrimPoint*6];
                float *curUV = &sPrims[curPrim].points[curPrimPoint*6 + 4];
                int xyzFound = 0;
                int uvFound = 0;
                for (int prim = curPrim - 1; prim >= 0 && !(xyzFound && uvFound); prim--) {
                    for (int primPoint = 0; primPoint < sPrims[prim].pointCount; primPoint++) {
                        float *xyz = &sPrims[prim].points[primPoint*6];
                        float *uv = &sPrims[prim].points[primPoint*6 + 4];
                        if (xyz[0] == curXYZ[0] && xyz[1] == curXYZ[1] && xyz[2] == curXYZ[2]) xyzFound = 1;
                        if (uv[0] == curUV[0] && uv[1] == curUV[1]) uvFound = 1; 
                    }
                }
                if (!xyzFound) uniqueXYZCount++;
                if (!uvFound) uniqueUVCount++;
            }
        }
        /* Create mesh. */
        Mesh *mesh = (Mesh *)malloc(sizeof(Mesh));
        mesh->faceCount = sPrimCount;
        mesh->pointCount = uniqueXYZCount;
        mesh->uvCount = uniqueUVCount;
        mesh->frameCount = 1;
        mesh->faces = (MeshFace *)malloc(sizeof(MeshFace)*mesh->faceCount);
        mesh->xyzw = (float *)malloc(sizeof(float)*mesh->pointCount*4);
        mesh->xyzwTrans = (float *)malloc(sizeof(float)*mesh->pointCount*4);
        mesh->uv = (float *)malloc(sizeof(float)*mesh->uvCount*2);
        uniqueXYZCount = 0;
        uniqueUVCount = 0;
        for (int i = 0; i < sPrimCount; i++) {
            MeshFace *mf = &mesh->faces[i];
            Face *f = &sPrims[i];
            mf->texture = f->texture;
            mf->color = f->color;
            mf->pointCount = f->pointCount;
            for (int j = 0; j < f->pointCount; j++) {
                float *fp = &f->points[j*6];
                float *fuv = &f->points[j*6 + 4];
                int k;
                for (k = 0; k < uniqueXYZCount; k++) {
                    float *xyzw = &mesh->xyzw[k*4];
                    if (xyzw[0] == fp[0] && xyzw[1] == fp[1] && xyzw[2] == fp[2]) break;
                }
                if (k == uniqueXYZCount) {
                    float *xyzw = &mesh->xyzw[k*4];
                    xyzw[0] = fp[0];
                    xyzw[1] = fp[1];
                    xyzw[2] = fp[2];
                    xyzw[3] = 1.0f;
                    uniqueXYZCount++;
                }
                mf->xyzw[j] = k;
                for (k = 0; k < uniqueUVCount; k++) {
                    float *uv = &mesh->uv[k*2];
                    if (uv[0] == fuv[0] && uv[1] == fuv[1]) break;
                }
                if (k == uniqueUVCount) {
                    float *uv = &mesh->uv[k*2];
                    uv[0] = fuv[0];
                    uv[1] = fuv[1];
                    uniqueUVCount++;
                }
                mf->uv[j] = k;
            }
        }
        sMeshes[sRenderMesh] = mesh;
        sRenderMesh = -1;
        sPrimCount = 0;
        sPrimVertexCount = 0;
    }
    
    return result;
}

/*
 * S3D_FreeMesh
 * ------------
 * S3D_FreeMesh(id)
 */
Variable S3D_FreeMesh(int argc, Variable *argv) {
    Variable result;
    
    FreeMeshId((int)ToNumber(&argv[0]) - 1);
    
    result.type = VAR_UNSET;
    return result;
}

/*
 * S3D_Mesh
 * --------
 * S3D_Mesh(mesh, frame)
 */
Variable S3D_Mesh(int argc, Variable *argv) {
    Variable result;
    result.type = VAR_UNSET;
    
    int id = (int)ToNumber(&argv[0]) - 1;
    int frame = (int)ToNumber(&argv[1]);
    if (id >= 0 && id < sMeshSize) {
        Mesh *mesh = sMeshes[id];
        if (mesh) {
            if (frame >= 0 && frame < mesh->frameCount) {
                int offs = mesh->pointCount*frame*4;
                for (int i = 0; i < mesh->pointCount; i++) {
                    int j = i*4;
                     Mat4VecMult(&mesh->xyzwTrans[j],
                            mesh->xyzw[offs + j],
                            mesh->xyzw[offs + j + 1],
                            mesh->xyzw[offs + j + 2],
                            1.0f, sTransMat);
                }
                for (int i = 0; i < mesh->faceCount; i++) {
#ifdef COLORIZE_MESHES
                    unsigned char faceR, faceG, faceB, faceA;
                    int r, g, b, a;
#endif
                    float z = 0;
                    int zminok = 0;
                    int zmaxok = 0;
                    float *pp = sPrims[sPrimCount].points;
                    MeshFace *mf = &mesh->faces[i];
                    sPrims[sPrimCount].texture = mf->texture ? mf->texture : sTexture;
#ifdef COLORIZE_MESHES
                    if (mf->color) {
                        ColorToRGBAComponents(mf->color, faceR, faceG, faceB, faceA);
                        r = faceR*sRed >> 8;
                        g = faceG*sGreen >> 8;
                        b = faceB*sBlue >> 8;
                        a = faceA*sAlpha >> 7;
                        sPrims[sPrimCount].color = ToRGBA(r, g, b, a);
                    }
                    else {
                        sPrims[sPrimCount].color = sColor;
                    }
#else
                    sPrims[sPrimCount].color = mf->color ? mf->color : sColor;
#endif
                    
                    sPrims[sPrimCount].pointCount = mf->pointCount;
                    for (int j = 0; j < mf->pointCount; j++) {
                        float *mp = &mesh->xyzwTrans[mf->xyzw[j]*4];
                        float *uv = &mesh->uv[mf->uv[j]*2];
                        *pp++ = mp[0];
                        *pp++ = mp[1];
                        *pp++ = mp[2];
                        *pp++ = mp[3];
                        *pp++ = uv[0];
                        *pp++ = uv[1];
                        z += mp[2];
                        zminok |= mp[2] > sZMin;
                        zmaxok |= mp[2] < sZMax; 
                    }
                    /* Skip if all points <= sZMin. */
                    if (zminok && zmaxok) { /* 2025-07-14, can't have that check when building mesh!!! */
                        sPrims[sPrimCount].z = z/mf->pointCount;
                        if (sRenderMesh < 0 && sSorting == NONE) {
                            RenderFace(&sPrims[sPrimCount]);
                        }
                        else if (++sPrimCount >= MAX_PRIMS) {
                            RuntimeError("S3D_Mesh: Face buffer overflow");
                            break;
                        }
                    }
                }
            }
            else {
                RuntimeError("S3D_Mesh: Invalid frame");
            }
        }
        else {
            RuntimeError("S3D_Mesh: Invalid mesh id");
        }
    }
    else {
        RuntimeError("S3D_Mesh: Invalid mesh id");
    }
    
    return result;
}

Variable S3D_BlendMesh(int argc, Variable *argv) {
    Variable result;
    result.type = VAR_UNSET;
    
    int id = (int)ToNumber(&argv[0]) - 1;
    int frame0 = (int)ToNumber(&argv[1]);
    int frame1 = (int)ToNumber(&argv[2]);
    float blend1 = ToNumber(&argv[3]);
    float blend0;
    
    if (id >= 0 && id < sMeshSize) {
        Mesh *mesh = sMeshes[id];
        if (mesh) {
            if (frame0 < 0 || frame0 >= mesh->frameCount) {
                RuntimeError("S3D_BlendMesh: Invalid first frame");
                return result;
            }
            if (frame1 < 0 || frame1 >= mesh->frameCount) {
                RuntimeError("S3D_BlendMesh: Invalid second frame");
                return result;
            }
            blend1 = blend1 < 0.0f ? 0.0f : blend1 > 1.0f ? 1.0f : blend1;
            blend0 = 1.0f - blend1;
            int offs0 = mesh->pointCount*frame0*4;
            int offs1 = mesh->pointCount*frame1*4;
            for (int i = 0; i < mesh->pointCount; i++) {
                int j = i*4;
                Mat4VecMult(&mesh->xyzwTrans[j],
                        blend0*mesh->xyzw[offs0 + j] + blend1*mesh->xyzw[offs1 + j],
                        blend0*mesh->xyzw[offs0 + j + 1] + blend1*mesh->xyzw[offs1 + j + 1],
                        blend0*mesh->xyzw[offs0 + j + 2] + blend1*mesh->xyzw[offs1 + j + 2],
                        1.0f, sTransMat);
            }
            for (int i = 0; i < mesh->faceCount; i++) {
#ifdef COLORIZE_MESHES
                unsigned char faceR, faceG, faceB, faceA;
                int r, g, b, a;
#endif
                float z = 0;
                int zminok = 0;
                int zmaxok = 0;
                float *pp = sPrims[sPrimCount].points;
                MeshFace *mf = &mesh->faces[i];
                sPrims[sPrimCount].texture = mf->texture ? mf->texture : sTexture;
#ifdef COLORIZE_MESHES
                if (mf->color) {
                    ColorToRGBAComponents(mf->color, faceR, faceG, faceB, faceA);
                    r = faceR*sRed >> 8;
                    g = faceG*sGreen >> 8;
                    b = faceB*sBlue >> 8;
                    a = faceA*sAlpha >> 7;
                    sPrims[sPrimCount].color = ToRGBA(r, g, b, a);
                }
                else {
                    sPrims[sPrimCount].color = sColor;
                }
#else
                sPrims[sPrimCount].color = mf->color ? mf->color : sColor;
#endif
                sPrims[sPrimCount].pointCount = mf->pointCount;
                for (int j = 0; j < mf->pointCount; j++) {
                    float *mp = &mesh->xyzwTrans[mf->xyzw[j]*4];
                    float *uv = &mesh->uv[mf->uv[j]*2];
                    *pp++ = mp[0];
                    *pp++ = mp[1];
                    *pp++ = mp[2];
                    *pp++ = mp[3];
                    *pp++ = uv[0];
                    *pp++ = uv[1];
                    z += mp[2];
                    zminok |= mp[2] > sZMin;
                    zmaxok |= mp[2] < sZMax; 
                }
                /* Skip if all points <= sZMin. */
                if (zminok && zmaxok) {
                    sPrims[sPrimCount].z = z/mf->pointCount;
                    if (sRenderMesh < 0 && sSorting == NONE) {
                        RenderFace(&sPrims[sPrimCount]);
                    }
                    else if (++sPrimCount >= MAX_PRIMS) {
                        RuntimeError("S3D_BlendMesh: Face buffer overflow");
                        break;
                    }
                }
            }
        }
        else {
            RuntimeError("S3D_BlendMesh: Invalid mesh");
        }
    }
    else {
        RuntimeError("S3D_BlendMesh: Invalid mesh");
    }
    
    return result;
}


/*
 * RenderFace
 * ----------
 */
void RenderFace(Face *face) {
    int count = face->pointCount;
    int xy[5*2];
    float uvz[5*3];
    float hw, hh;
    int xmins, xmaxs, ymins, ymaxs, zmaxs;
    int zmins = 0;
    float *a, *b, *c;
    int i;
    
    if (!sDstImage) return;
    
    /* Abort if all points are beind view. */
    for (i = 0; i < face->pointCount; i++) zmins += face->points[i*6 + 2] > sZMin;
    if (!zmins) return;
    /* Clip against z-min? ZMinClip returns the pointer of a static Face. */
    if (zmins < count) {
        face = ZMinClippedFace(face);
        count = face->pointCount;
    }
    
    hw = 0.5f*(float)sDstImage->w;
    hh = 0.5*(float)sDstImage->h;
    xmins = 0; ymins = 0; xmaxs = 0; ymaxs = 0; zmaxs = 0;
    for (i = 0; i < face->pointCount; i++) {
        float *v = &face->points[i*6];
        float z = v[2];
        float wi;
        int x, y;
        
        zmaxs += v[2] > z;
        
        Mat4VecMult(v, v[0], v[1], v[2], v[3], sProjMat);
        wi = 1.0f/v[3]; v[0] *= wi; v[1] *= wi;
        x = (int)roundf((v[0] + 1)*hw);
        y = (int)roundf((v[1] + 1)*hh);
        
        xmins += x < sDstImage->xMin;
        xmaxs += x > sDstImage->xMax;
        ymins += y < sDstImage->yMin;
        ymaxs += y > sDstImage->yMax;
        
        xy[i*2] = x;
        xy[i*2 + 1] = y;
        uvz[i*3] = v[4];
        uvz[i*3 + 1] = v[5];
        uvz[i*3 + 2] = z;
        
    }
    if (xmins >= count || xmaxs >= count || ymins >= count || ymaxs >= count || zmaxs >= count) return;
    a = &face->points[0]; b = &face->points[6]; c = &face->points[12];
    if ((b[0] - a[0])*(c[1] - a[1]) - (c[0] - a[0])*(b[1] - a[1]) <= 0) return;

    if (face->texture) {
        float umax = (float)face->texture->w - 0.01f;
        float vmax = (float)face->texture->h - 0.01f;
        for (i = 0; i < face->pointCount; i++) {
            float *uv = &uvz[i*3];
            uv[0] *= face->texture->w;
            uv[1] *= face->texture->h;
            if (uv[0] < 0.01f) uv[0] = 0.01f;
            if (uv[0] > umax) uv[0] = umax;
            if (uv[1] < 0.01f) uv[1] = 0.01f;
            if (uv[1] > vmax) uv[1] = vmax;
        }
    }

    if (sDepthBuffer == NONE) {
        if (face->texture) {
            IMG_TexturePolygonZ(sDstImage, face->pointCount, xy, uvz, face->texture, face->color, 1, sAdditive);
        }
        else {
            IMG_FillPolygon(sDstImage, face->pointCount, xy, face->color, sAdditive);
        }
    }
    else if (sDepthBuffer == Z_BUFFER_READ) {
        if (face->texture) {
            IMG_TexturePolygonZBR(sDstImage, face->pointCount, xy, uvz, face->texture, face->color, 1, sAdditive, sZBuffer);
        }
        else {
            IMG_FillPolygonZBR(sDstImage, face->pointCount, xy, uvz, face->color, sAdditive, sZBuffer);
        }
    }
    else if (sDepthBuffer == Z_BUFFER_WRITE) {
        if (face->texture) {
            IMG_TexturePolygonZBW(sDstImage, face->pointCount, xy, uvz, face->texture, face->color, 1, sAdditive, sZBuffer);
        }
        else {
            IMG_FillPolygonZBW(sDstImage, face->pointCount, xy, uvz, face->color, sAdditive, sZBuffer);
        }
    }
    else if (sDepthBuffer == Z_BUFFER_READ_WRITE) {
        if (face->texture) {
            IMG_TexturePolygonZBRW(sDstImage, face->pointCount, xy, uvz, face->texture, face->color, 1, sAdditive, sZBuffer);
        }
        else {
            IMG_FillPolygonZBRW(sDstImage, face->pointCount, xy, uvz, face->color, sAdditive, sZBuffer);
        }
    }
}

/*
 * ZMinClippedFace
 * ---------------
 * Return face clipped against z-min.
 */
Face *ZMinClippedFace(Face *face) {
    int count = face->pointCount;
    int clippedCount = 0;
  
    for (int i = 0; i < count; i++) {
        float *v0 = &face->points[i*6];
        float *v1 = &face->points[((i + 1)%count)*6];
        float z0 = v0[2], z1 = v1[2];
        if (z0 >= sZMin) {
            float *v = &sClippedFace.points[clippedCount*6];
            v[0] = v0[0]; v[1] = v0[1]; v[2] = v0[2]; v[3] = v0[3];
            v[4] = v0[4]; v[5] = v0[5];
            if (++clippedCount >= 5) break;
        }
        if ((z1 >= sZMin && z0 < sZMin) || (z0 >= sZMin && z1 < sZMin)) {
            float k = (sZMin - z0)/(z1 - z0);
            float *v = &sClippedFace.points[clippedCount*6];
            v[0] = v0[0] + k*(v1[0] - v0[0]);
            v[1] = v0[1] + k*(v1[1] - v0[1]);
            v[2] = v0[2] + k*(v1[2] - v0[2]);
            v[3] = sZMin;
            v[4] = v0[4] + k*(v1[4] - v0[4]);
            v[5] = v0[5] + k*(v1[5] - v0[5]);
            if (++clippedCount >= 5) break;
        }
    }
    sClippedFace.pointCount = clippedCount;
    sClippedFace.texture = face->texture;
    sClippedFace.color = face->color;
    
    return &sClippedFace;
}

/*
 * ValidateTarget
 * --------------
 */
void ValidateTarget() {
    sDstImage = (Image *)WIN_GetImage(sDstImageId);
    if (sDstImage) {
        /*if (sZBuffer && !(sZBufferWidth == sDstImage->w && sZBufferHeight == sDstImage->h)) {
            free(sZBuffer);
            sZBuffer = 0;
        }
        if (!sZBuffer) {
            int s;
            sZBufferWidth = sDstImage->w;
            sZBufferHeight = sDstImage->h;
            s = sZBufferWidth*sZBufferHeight;            
            sZBuffer = (int *)malloc(sizeof(int)*s);
            for (int i = 0; i < s; i++) sZBuffer[i] = 2147483647;
        }*/
        IMG_AddZBuffer(sDstImage);
        sZBuffer = IMG_ZBuffer(sDstImage);
    }
    else if (sZBuffer) {
        /*free(sZBuffer);
        sZBuffer = 0;
        sZBufferWidth = 0;
        sZBufferHeight = 0;*/
        sZBuffer = 0;
    }
}

/*
 * ClearTransformation
 * -------------------
 */
void ClearTransformation() {
    Mat4MakeIdent(sTransMat);    
    ValidateTarget();
}

/*
 * ClearDepthBuffer
 * ----------------
 */
int ClearDepthBuffer() {
    ValidateTarget(); /* ... why? */
    if (sZBuffer) {
        /*int s;
        s = sZBufferWidth*sZBufferHeight;*/
        int s = sDstImage->w*sDstImage->h;
        for (int i = 0; i < s; i++) sZBuffer[i] = 2147483647;
        return 1;
    }
    return 0;
}

/*
 * Mat4MakeIdent
 * -------------
 */
void Mat4MakeIdent(float *m) {
    m[0] = 1.0f; m[4] = 0.0f; m[8] = 0.0f; m[12] = 0.0f;
    m[1] = 0.0f; m[5] = 1.0f; m[9] = 0.0f; m[13] = 0.0f;
    m[2] = 0.0f; m[6] = 0.0f; m[10] = 1.0f; m[14] = 0.0f;
    m[3] = 0.0f; m[7] = 0.0f; m[11] = 0.0f; m[15] = 1.0f;
}

/*
 * Mat4PostMul
 * -----------
 */
void Mat4PostMul(float *m, float *n) {
    sOpMat[0] = n[0]*m[0] + n[1]*m[4] + n[2]*m[8] + n[3]*m[12];
    sOpMat[1] = n[0]*m[1] + n[1]*m[5] + n[2]*m[9] + n[3]*m[13];
    sOpMat[2] = n[0]*m[2] + n[1]*m[6] + n[2]*m[10] + n[3]*m[14];
    sOpMat[3] = n[0]*m[3] + n[1]*m[7] + n[2]*m[11] + n[3]*m[15];
    sOpMat[4] = n[4]*m[0] + n[5]*m[4] + n[6]*m[8] + n[7]*m[12];
    sOpMat[5] = n[4]*m[1] + n[5]*m[5] + n[6]*m[9] + n[7]*m[13];
    sOpMat[6] = n[4]*m[2] + n[5]*m[6] + n[6]*m[10] + n[7]*m[14];
    sOpMat[7] = n[4]*m[3] + n[5]*m[7] + n[6]*m[11] + n[7]*m[15];
    sOpMat[8] = n[8]*m[0] + n[9]*m[4] + n[10]*m[8] + n[11]*m[12];
    sOpMat[9] = n[8]*m[1] + n[9]*m[5] + n[10]*m[9] + n[11]*m[13];
    sOpMat[10] = n[8]*m[2] + n[9]*m[6] + n[10]*m[10] + n[11]*m[14];
    sOpMat[11] = n[8]*m[3] + n[9]*m[7] + n[10]*m[11] + n[11]*m[15];
    sOpMat[12] = n[12]*m[0] + n[13]*m[4] + n[14]*m[8] + n[15]*m[12];
    sOpMat[13] = n[12]*m[1] + n[13]*m[5] + n[14]*m[9] + n[15]*m[13];
    sOpMat[14] = n[12]*m[2] + n[13]*m[6] + n[14]*m[10] + n[15]*m[14];
    sOpMat[15] = n[12]*m[3] + n[13]*m[7] + n[14]*m[11] + n[15]*m[15];
    Mat4Copy(m, sOpMat);
}

/* Mat4Copy
 * --------
 */
void Mat4Copy(float *dst, float *src) {
    for (int i = 0; i < 16; i++) dst[i] = src[i];
}

/*
 * Mat4VecMul
 * ----------
 */
void Mat4VecMult(float *dst, float x, float y, float z, float w, float *m) {
    dst[0] = x*m[0] + y*m[4] + z*m[8] + w*m[12];
    dst[1] = x*m[1] + y*m[5] + z*m[9] + w*m[13];
    dst[2] = x*m[2] + y*m[6] + z*m[10] + w*m[14];
    dst[3] = x*m[3] + y*m[7] + z*m[11] + w*m[15];
}

/*
 * S3D_Init
 * --------
 */
void S3D_Init() {
    RegisterN7CFunction("s3d_set_view", S3D_SetView);
    RegisterN7CFunction("s3d_set_perspective_correction", S3D_SetPerspectiveCorrection);
    RegisterN7CFunction("s3d_set_depth_buffer", S3D_SetDepthBuffer);
    RegisterN7CFunction("s3d_set_sorting", S3D_SetSorting);
    RegisterN7CFunction("s3d_clear_transformation", S3D_ClearTransformation);
    RegisterN7CFunction("s3d_clear_depth_buffer", S3D_ClearDepthBuffer);
    RegisterN7CFunction("s3d_clear", S3D_Clear);
    RegisterN7CFunction("s3d_translate", S3D_Translate);
    RegisterN7CFunction("s3d_rotate_x", S3D_RotateX);
    RegisterN7CFunction("s3d_rotate_y", S3D_RotateY);
    RegisterN7CFunction("s3d_rotate_z", S3D_RotateZ);
    RegisterN7CFunction("s3d_scale", S3D_Scale);
    RegisterN7CFunction("s3d_push", S3D_Push);
    RegisterN7CFunction("s3d_pop", S3D_Pop);
    RegisterN7CFunction("s3d_begin", S3D_Begin);
    RegisterN7CFunction("s3d_end", S3D_End);
    RegisterN7CFunction("s3d_texture", S3D_Texture);
    RegisterN7CFunction("s3d_color", S3D_Color);
    RegisterN7CFunction("s3d_additive", S3D_Additive);
    RegisterN7CFunction("s3d_vertex", S3D_Vertex);
    RegisterN7CFunction("s3d_render", S3D_Render);
    RegisterN7CFunction("s3d_render_fog", S3D_RenderFog);
    RegisterN7CFunction("s3d_create_mesh", S3D_CreateMesh);
    RegisterN7CFunction("s3d_add_mesh_frame", S3D_AddMeshFrame);
    RegisterN7CFunction("s3d_begin_mesh", S3D_BeginMesh);
    RegisterN7CFunction("s3d_end_mesh", S3D_EndMesh);
    RegisterN7CFunction("s3d_free_mesh", S3D_FreeMesh);
    RegisterN7CFunction("s3d_mesh", S3D_Mesh);
    RegisterN7CFunction("s3d_blend_mesh", S3D_BlendMesh);
    RegisterN7CFunction("s3d_transform_vector", S3D_TransformVector);
    RegisterN7CFunction("s3d_project_vector", S3D_ProjectVector);
    RegisterN7CFunction("s3d_project_face", S3D_ProjectFace);
}

/*
 * S3D_Terminate
 * -------------
 */
void S3D_Terminate() {
    if (sMeshes) {
        for (int i = 0; i < sMeshSize; i++) FreeMeshId(i);
        free(sMeshes);
        sMeshes = 0;
        sMeshSize = 0;
    }
    /*if (sZBuffer) {
        free(sZBuffer);
        sZBuffer = 0;
    }*/
}
