/*========================================================================================
 Noesis SDK shim — standalone D3D9 replacement for pluginshare.h
 Declares every type and function used by the model_ff11 parser modules.
========================================================================================*/

#pragma once

#include <windows.h>
#include <d3d9.h>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cassert>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>

//========================================================================================
// Utility
//========================================================================================

#define NoeAssert(x)  assert(x)
#define MAX_NOESIS_PATH 512

//========================================================================================
// Math types
//========================================================================================

struct RichVec2
{
    float v[2];
    RichVec2()                        { v[0]=0; v[1]=0; }
    RichVec2(float x, float y)        { v[0]=x; v[1]=y; }
    float &operator[](int i)          { return v[i]; }
    const float &operator[](int i) const { return v[i]; }
};

struct RichVec3
{
    float v[3];
    RichVec3()                             { v[0]=0; v[1]=0; v[2]=0; }
    RichVec3(float x, float y, float z)    { v[0]=x; v[1]=y; v[2]=z; }
    explicit RichVec3(const float *p)      { v[0]=p[0]; v[1]=p[1]; v[2]=p[2]; }

    float &operator[](int i)               { return v[i]; }
    const float &operator[](int i) const   { return v[i]; }

    RichVec3 operator-() const             { return RichVec3(-v[0],-v[1],-v[2]); }
    RichVec3 operator+(const RichVec3 &o) const { return RichVec3(v[0]+o.v[0],v[1]+o.v[1],v[2]+o.v[2]); }
    RichVec3 operator-(const RichVec3 &o) const { return RichVec3(v[0]-o.v[0],v[1]-o.v[1],v[2]-o.v[2]); }
    RichVec3 operator*(float s) const      { return RichVec3(v[0]*s,v[1]*s,v[2]*s); }
    RichVec3 &operator+=(const RichVec3 &o){ v[0]+=o.v[0]; v[1]+=o.v[1]; v[2]+=o.v[2]; return *this; }
    RichVec3 &operator*=(float s)          { v[0]*=s; v[1]*=s; v[2]*=s; return *this; }

    void Normalize()
    {
        float l = sqrtf(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
        if (l > 1e-8f) { v[0]/=l; v[1]/=l; v[2]/=l; }
    }
};

struct RichVec4
{
    float v[4];
    RichVec4()                                      { v[0]=0; v[1]=0; v[2]=0; v[3]=0; }
    RichVec4(float x, float y, float z, float w)    { v[0]=x; v[1]=y; v[2]=z; v[3]=w; }
    explicit RichVec4(const float *p)               { v[0]=p[0]; v[1]=p[1]; v[2]=p[2]; v[3]=p[3]; }
    explicit RichVec4(const RichVec3 &xyz, float w=0) { v[0]=xyz.v[0]; v[1]=xyz.v[1]; v[2]=xyz.v[2]; v[3]=w; }

    float &operator[](int i)            { return v[i]; }
    const float &operator[](int i) const{ return v[i]; }

    RichVec4 operator-() const          { return RichVec4(-v[0],-v[1],-v[2],-v[3]); }
    RichVec4 &operator+=(const RichVec4 &o){ v[0]+=o.v[0]; v[1]+=o.v[1]; v[2]+=o.v[2]; v[3]+=o.v[3]; return *this; }
};

// Forward declaration — TransformQST uses RichQuat* but RichQuat is defined below.
struct RichQuat;

// RichMat43: 4 rows of RichVec3.
//   Rows 0-2 = rotation/scale (3x3).
//   Row 3    = translation.
// Row-vector convention: transformed_pos = pos * M
typedef RichVec3 modelMatrix_t[4]; // same memory layout as RichMat43::m

struct RichMat43
{
    RichVec3 m[4]; // m[row][col]; m[3] = translation

    RichMat43()
    {
        m[0] = RichVec3(1,0,0);
        m[1] = RichVec3(0,1,0);
        m[2] = RichVec3(0,0,1);
        m[3] = RichVec3(0,0,0);
    }

    RichVec3 &operator[](int i)            { return m[i]; }
    const RichVec3 &operator[](int i) const{ return m[i]; }

    // Affine composition (row vectors): C = A * B  →  p * C = (p * A) * B
    RichMat43 operator*(const RichMat43 &b) const
    {
        RichMat43 r;
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 3; ++col)
                r.m[row].v[col] = m[row].v[0]*b.m[0].v[col]
                                + m[row].v[1]*b.m[1].v[col]
                                + m[row].v[2]*b.m[2].v[col];
        // translation row
        for (int col = 0; col < 3; ++col)
            r.m[3].v[col] = m[3].v[0]*b.m[0].v[col]
                           + m[3].v[1]*b.m[1].v[col]
                           + m[3].v[2]*b.m[2].v[col]
                           + b.m[3].v[col];
        return r;
    }

    // Inverse of an orthonormal affine: R_inv = R^T, t_inv = -t * R^T
    RichMat43 GetInverse() const
    {
        RichMat43 inv;
        // Transpose rotation
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 3; ++c)
                inv.m[r].v[c] = m[c].v[r];
        // Translation: -t * R^T
        for (int c = 0; c < 3; ++c)
            inv.m[3].v[c] = -(m[3].v[0]*inv.m[0].v[c]
                             + m[3].v[1]*inv.m[1].v[c]
                             + m[3].v[2]*inv.m[2].v[c]);
        return inv;
    }

    // Defined after RichQuat (needs complete type for pQ->v[]).
    void TransformQST(const RichVec3 *pCenter, const RichQuat *pQ, const RichVec3 *pS,
                       const RichVec3 *pPivotCenter, const RichQuat *pPivotQ,
                       const RichVec3 *pT);

    struct RichMat44 ToMat44() const; // defined after RichMat44
};

// RichMat44: column-major 4x4.
//   c[j] = column j (a RichVec4).
// Column-vector convention: transformed = M * v
struct RichMat44
{
    RichVec4 c[4]; // c[col][row]

    RichMat44() // identity
    {
        c[0] = RichVec4(1,0,0,0);
        c[1] = RichVec4(0,1,0,0);
        c[2] = RichVec4(0,0,1,0);
        c[3] = RichVec4(0,0,0,1);
    }
    RichMat44(const RichVec4 &c0, const RichVec4 &c1, const RichVec4 &c2, const RichVec4 &c3)
    { c[0]=c0; c[1]=c1; c[2]=c2; c[3]=c3; }

    RichMat44 operator*(const RichMat44 &b) const
    {
        RichMat44 r;
        for (int col = 0; col < 4; ++col)
            for (int row = 0; row < 4; ++row)
                r.c[col].v[row] = c[0].v[row]*b.c[col].v[0]
                                + c[1].v[row]*b.c[col].v[1]
                                + c[2].v[row]*b.c[col].v[2]
                                + c[3].v[row]*b.c[col].v[3];
        return r;
    }

    // M * v  (v is a column vector)
    RichVec4 TransformVec4(const RichVec4 &v) const
    {
        return RichVec4(
            c[0].v[0]*v.v[0] + c[1].v[0]*v.v[1] + c[2].v[0]*v.v[2] + c[3].v[0]*v.v[3],
            c[0].v[1]*v.v[0] + c[1].v[1]*v.v[1] + c[2].v[1]*v.v[2] + c[3].v[1]*v.v[3],
            c[0].v[2]*v.v[0] + c[1].v[2]*v.v[1] + c[2].v[2]*v.v[2] + c[3].v[2]*v.v[3],
            c[0].v[3]*v.v[0] + c[1].v[3]*v.v[1] + c[2].v[3]*v.v[2] + c[3].v[3]*v.v[3]
        );
    }

    // Upper-left 3x3 * normal (no translation, no perspective divide)
    RichVec3 TransformNormal(const RichVec3 &n) const
    {
        return RichVec3(
            c[0].v[0]*n.v[0] + c[1].v[0]*n.v[1] + c[2].v[0]*n.v[2],
            c[0].v[1]*n.v[0] + c[1].v[1]*n.v[1] + c[2].v[1]*n.v[2],
            c[0].v[2]*n.v[0] + c[1].v[2]*n.v[1] + c[2].v[2]*n.v[2]
        );
    }
};

// Defined after RichMat44 is complete
inline RichMat44 RichMat43::ToMat44() const
{
    // Each row of the 4x3 becomes a column of the 4x4 (row→column convention bridge)
    return RichMat44(
        RichVec4(m[0].v[0], m[0].v[1], m[0].v[2], 0.0f),
        RichVec4(m[1].v[0], m[1].v[1], m[1].v[2], 0.0f),
        RichVec4(m[2].v[0], m[2].v[1], m[2].v[2], 0.0f),
        RichVec4(m[3].v[0], m[3].v[1], m[3].v[2], 1.0f)
    );
}

struct RichQuat
{
    float v[4]; // x, y, z, w

    RichQuat()                                      { v[0]=0; v[1]=0; v[2]=0; v[3]=1; }
    RichQuat(float x, float y, float z, float w)    { v[0]=x; v[1]=y; v[2]=z; v[3]=w; }

    float       &operator[](int i)       { return v[i]; }
    const float &operator[](int i) const { return v[i]; }

    // Convert to RichMat43.
    // colMajor=true (the normal usage): output is transposed relative to the "standard" rotation matrix,
    // which is what you want when using row-vector transforms (p' = p * M).
    RichMat43 ToMat43(bool colMajor) const
    {
        float x=v[0], y=v[1], z=v[2], w=v[3];
        RichMat43 mat;
        // Standard rotation matrix (column-vector, M*v convention):
        mat.m[0].v[0] = 1.0f - 2.0f*(y*y + z*z);
        mat.m[0].v[1] = 2.0f*(x*y + w*z);
        mat.m[0].v[2] = 2.0f*(x*z - w*y);
        mat.m[1].v[0] = 2.0f*(x*y - w*z);
        mat.m[1].v[1] = 1.0f - 2.0f*(x*x + z*z);
        mat.m[1].v[2] = 2.0f*(y*z + w*x);
        mat.m[2].v[0] = 2.0f*(x*z + w*y);
        mat.m[2].v[1] = 2.0f*(y*z - w*x);
        mat.m[2].v[2] = 1.0f - 2.0f*(x*x + y*y);
        mat.m[3]       = RichVec3(0,0,0);

        if (colMajor)
        {
            // Transpose rotation to convert to row-vector convention
            float t;
            t=mat.m[0].v[1]; mat.m[0].v[1]=mat.m[1].v[0]; mat.m[1].v[0]=t;
            t=mat.m[0].v[2]; mat.m[0].v[2]=mat.m[2].v[0]; mat.m[2].v[0]=t;
            t=mat.m[1].v[2]; mat.m[1].v[2]=mat.m[2].v[1]; mat.m[2].v[1]=t;
        }
        return mat;
    }
};

// TransformQST defined here because it needs RichQuat to be complete.
inline void RichMat43::TransformQST(const RichVec3 *pCenter, const RichQuat *pQ, const RichVec3 *pS,
                                      const RichVec3 * /*pPivotCenter*/, const RichQuat * /*pPivotQ*/,
                                      const RichVec3 *pT)
{
    const float qx=pQ->v[0], qy=pQ->v[1], qz=pQ->v[2], qw=pQ->v[3];
    // Match the skeleton conversion used by the FF11 character skinning path.
    const float r00 = 1.0f-2.0f*(qy*qy+qz*qz);
    const float r01 = 2.0f*(qx*qy+qw*qz);
    const float r02 = 2.0f*(qx*qz-qw*qy);
    const float r10 = 2.0f*(qx*qy-qw*qz);
    const float r11 = 1.0f-2.0f*(qx*qx+qz*qz);
    const float r12 = 2.0f*(qy*qz+qw*qx);
    const float r20 = 2.0f*(qx*qz+qw*qy);
    const float r21 = 2.0f*(qy*qz-qw*qx);
    const float r22 = 1.0f-2.0f*(qx*qx+qy*qy);

    // Apply scale per-row
    const float sx = pS->v[0], sy = pS->v[1], sz = pS->v[2];
    m[0].v[0]=r00*sx; m[0].v[1]=r01*sx; m[0].v[2]=r02*sx;
    m[1].v[0]=r10*sy; m[1].v[1]=r11*sy; m[1].v[2]=r12*sy;
    m[2].v[0]=r20*sz; m[2].v[1]=r21*sz; m[2].v[2]=r22*sz;

    // Translation: center + T - center * RS
    const float cx=pCenter->v[0], cy=pCenter->v[1], cz=pCenter->v[2];
    const float tx=pT?pT->v[0]:0.0f, ty=pT?pT->v[1]:0.0f, tz=pT?pT->v[2]:0.0f;
    m[3].v[0] = cx - (cx*m[0].v[0]+cy*m[1].v[0]+cz*m[2].v[0]) + tx;
    m[3].v[1] = cy - (cx*m[0].v[1]+cy*m[1].v[1]+cz*m[2].v[1]) + ty;
    m[3].v[2] = cz - (cx*m[0].v[2]+cy*m[1].v[2]+cz*m[2].v[2]) + tz;
}

struct RichAngles
{
    float v[3]; // x, y, z angles
    bool  mRadians;

    RichAngles(const float *angles, bool radians)
        : mRadians(radians)
    { v[0]=angles[0]; v[1]=angles[1]; v[2]=angles[2]; }

    // XYZ Euler angles -> RichMat43 (row-vector convention).
    // Matches the Noesis RichAngles convention: true means radians.
    RichMat43 ToMat43_XYZ() const
    {
        const float toRad = mRadians ? 1.0f : (3.14159265358979323846f / 180.0f);
        const float ax = v[0]*toRad, ay = v[1]*toRad, az = v[2]*toRad;
        const float cx=cosf(ax), sx=sinf(ax);
        const float cy=cosf(ay), sy=sinf(ay);
        const float cz=cosf(az), sz=sinf(az);

        // Rx * Ry * Rz (row-major, each applied left-to-right)
        RichMat43 mat;
        mat.m[0].v[0] =  cy*cz;
        mat.m[0].v[1] =  cy*sz;
        mat.m[0].v[2] = -sy;
        mat.m[1].v[0] =  sx*sy*cz - cx*sz;
        mat.m[1].v[1] =  sx*sy*sz + cx*cz;
        mat.m[1].v[2] =  sx*cy;
        mat.m[2].v[0] =  cx*sy*cz + sx*sz;
        mat.m[2].v[1] =  cx*sy*sz - sx*cz;
        mat.m[2].v[2] =  cx*cy;
        mat.m[3]       = RichVec3(0,0,0);
        return mat;
    }
};

// Global identity matrix (columns c1–c4 for RichMat44 construction)
static const struct
{
    float c1[4]; float c2[4]; float c3[4]; float c4[4];
} g_identityMatrix4x4 = { {1,0,0,0}, {0,1,0,0}, {0,0,1,0}, {0,0,0,1} };

//========================================================================================
// CArrayList<T> — simple dynamic array matching the Noesis API
//========================================================================================

template<typename T>
class CArrayList
{
public:
    CArrayList()  {}
    ~CArrayList() {}

    void Append(const T &item)          { mItems.push_back(item); }
    int  Num()   const                  { return (int)mItems.size(); }
    bool IsEmpty() const                { return mItems.empty(); }

    T       &operator[](int i)          { return mItems[i]; }
    const T &operator[](int i) const    { return mItems[i]; }

    typename std::vector<T>::iterator begin() { return mItems.begin(); }
    typename std::vector<T>::iterator end()   { return mItems.end(); }

private:
    std::vector<T> mItems;
};

//========================================================================================
// CRefCountedObject / CRefCountedPtr<T>
//========================================================================================

class CRefCountedObject
{
public:
    CRefCountedObject() : mRefCount(0) {}
    virtual ~CRefCountedObject() {}

    void AddRef()  { ++mRefCount; }
    void Release() { if (--mRefCount <= 0) delete this; }

private:
    int mRefCount;
};

template<typename T>
class CRefCountedPtr
{
public:
    CRefCountedPtr()           : mpObj(nullptr) {}
    CRefCountedPtr(T *p)       : mpObj(nullptr) { Set(p); }
    CRefCountedPtr(const CRefCountedPtr &o) : mpObj(nullptr) { Set(o.mpObj); }
    ~CRefCountedPtr()          { Set(nullptr); }

    CRefCountedPtr &operator=(T *p)                  { Set(p); return *this; }
    CRefCountedPtr &operator=(const CRefCountedPtr &o){ Set(o.mpObj); return *this; }

    T *operator->() const   { return mpObj; }
    T *get()        const   { return mpObj; }
    operator T*()   const   { return mpObj; }
    explicit operator bool() const { return mpObj != nullptr; }
    bool operator!() const  { return mpObj == nullptr; }

private:
    void Set(T *p)
    {
        if (p)  p->AddRef();
        if (mpObj) mpObj->Release();
        mpObj = p;
    }
    T *mpObj;
};

//========================================================================================
// CLocalResHash — name-keyed integer resource table
//========================================================================================

class CLocalResHash
{
public:
    explicit CLocalResHash(int keyLen) : mKeyLen(keyLen) {}

    // Find the value stored under 'key' (first mKeyLen chars).
    // If not found and addIfMissing=true, stores defaultVal and returns it.
    // If not found and addIfMissing=false, returns defaultVal without storing.
    int FindOrAddResource(const char *key, int defaultVal, bool addIfMissing = false)
    {
        std::string k(key, (size_t)mKeyLen);
        auto it = mMap.find(k);
        if (it != mMap.end())
            return it->second;
        if (addIfMissing)
            mMap[k] = defaultVal;
        return defaultVal;
    }

private:
    int mKeyLen;
    std::map<std::string, int> mMap;
};

//========================================================================================
// Bone
//========================================================================================

struct modelBoneEData_t
{
    struct modelBone_t *parent;
};

struct modelBone_t
{
    char             name[64]; // bone name
    int              index;
    float            mat[4][3]; // 4 rows × 3 cols — RichMat43-compatible layout
    modelBoneEData_t eData;
};

//========================================================================================
// Texture / material types
//========================================================================================

enum noesisTexType_e
{
    NOESISTEX_UNKNOWN = 0,
    NOESISTEX_RGBA32  = 1,
    NOESISTEX_DXT1    = 2,
    NOESISTEX_DXT3    = 3,
    NOESISTEX_DXT5    = 4,
};

// Raw pixel-format tags used by Noesis_ImageDecodeRaw
static const int b8g8r8a8 = 0;  // 4 bytes: B G R A
static const int b5g5r5a1 = 1;  // 2 bytes: B5 G5 R5 A1

// Material flags
static const int NMATFLAG_TWOSIDED = (1 << 0);

struct noesisTex_t
{
    char            *name;          // pooled name string
    int              w, h;
    noesisTexType_e  texType;
    unsigned char   *data;          // raw pixel data (RGBA32 or compressed)
    int              dataLen;
    bool             shouldFreeData;

    IDirect3DTexture9 *pD3DTex;     // D3D9 texture (NULL until uploaded)

    noesisTex_t()
        : name(nullptr), w(0), h(0), texType(NOESISTEX_UNKNOWN)
        , data(nullptr), dataLen(0), shouldFreeData(false), pD3DTex(nullptr)
    {}
};

struct noesisMaterial_t
{
    char   *name;            // pooled name string
    int     texIdx;          // index into the texture list (-1 = none)
    int     normalTexIdx;    // normal map texture index (-1 = none)
    int     specularTexIdx;  // specular map texture index (-1 = none)
    int     flags;           // NMATFLAG_*
    float   specular[4];     // specular colour + exponent
    float   alphaTest;       // alpha test threshold (0 = disabled)
    bool    noDefaultBlend;  // suppress additive/alpha blending

    noesisMaterial_t()
        : name(nullptr), texIdx(-1), normalTexIdx(-1), specularTexIdx(-1)
        , flags(0), alphaTest(0.0f), noDefaultBlend(false)
    { specular[0]=specular[1]=specular[2]=0; specular[3]=0; }
};

struct noesisMatData_t
{
    noesisMaterial_t **mats;
    int                matCount;
    noesisTex_t      **textures;
    int                texCount;

    noesisMatData_t()
        : mats(nullptr), matCount(0), textures(nullptr), texCount(0)
    {}

    // Find a material by name; returns nullptr if not found.
    noesisMaterial_t *FindMaterial(const char *matName) const
    {
        for (int i = 0; i < matCount; ++i)
            if (mats[i] && mats[i]->name && strcmp(mats[i]->name, matName) == 0)
                return mats[i];
        return nullptr;
    }
};

// Animation flags
static const int NANIMFLAG_FILENAMETOSEQ = (1 << 0); // treat filename as sequence name

struct noesisAnim_t
{
    char  *filename;    // pooled sequence name
    int    flags;       // NANIMFLAG_*
    int    frameCount;
    float  fps;
    int    boneCount;
    bool   allowExtendedPoseBounds;
    std::vector<RichMat43> frameWorldMats;
    // Optional model-space trajectory extracted from a skeletal root. Keeping
    // it separate lets scene code move the whole actor through the world while
    // CPU skinning evaluates a stable, root-relative pose.
    std::vector<RichVec3> frameRootMotion;
    RichVec3 rootMotionOrigin;

    noesisAnim_t()
        : filename(nullptr), flags(0), frameCount(0), fps(30.0f), boneCount(0)
        , allowExtendedPoseBounds(false)
    {}
};

//========================================================================================
// noesisModel_t — D3D9-backed mesh container returned by rpgConstructModel
//========================================================================================

// Interleaved vertex format used for all FFXI geometry.
// Must match FFXI_VERTEX_FVF below.
struct FFXIVertex
{
    float    pos[3];   // POSITION
    float    nrm[3];   // NORMAL
    D3DCOLOR diffuse;  // DIFFUSE  (ARGB)
    float    uv[2];    // TEXCOORD0
};

#define FFXI_VERTEX_FVF (D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_TEX1)
#define FFXI_VERTEX_STRIDE (sizeof(FFXIVertex))

struct FFXISkinVertex
{
    static const int kMaxWeights = 8;
    bool  skinned;
    int   weightCount;
    int   boneIdx[kMaxWeights];
    int   mirrorAxis[kMaxWeights];
    float boneWt[kMaxWeights];
    float pos[kMaxWeights][3];
    float nrm[kMaxWeights][3];

    FFXISkinVertex() { Reset(); }
    void Reset()
    {
        skinned = false;
        weightCount = 0;
        memset(boneIdx, 0, sizeof(boneIdx));
        memset(mirrorAxis, 0, sizeof(mirrorAxis));
        memset(boneWt, 0, sizeof(boneWt));
        memset(pos, 0, sizeof(pos));
        memset(nrm, 0, sizeof(nrm));
    }
};

// SQLE creation models describe their animation layout in the skeleton itself.
// The five counts are, in order, translation, quaternion, scale, and two
// currently-unused channel groups.  Keeping the parent-relative bind transform
// here lets the character-creation viewer build FrameChannel clips after the
// mesh and its separate head skeleton have been combined.
struct FFXISqleBoneInfo
{
    int parentIndex;
    int fileIndex;
    int sourceBoneIndex;
    int channelCounts[5];
    float bindTranslation[3];
    float bindQuaternion[4];
    float bindScale[3];
    float rootOffset[3];
};

struct noesisModel_t
{
    struct StaticBufferGroup
    {
        IDirect3DVertexBuffer9 *pVB;
        IDirect3DIndexBuffer9  *pIB;
        IDirect3DIndexBuffer9  *pOpaqueBatchIB;
        int                     vertCount;
        int                     indexCount;

        StaticBufferGroup()
            : pVB(nullptr), pIB(nullptr), pOpaqueBatchIB(nullptr)
            , vertCount(0), indexCount(0)
        {}
    };

    struct OpaqueBatch
    {
        noesisMaterial_t *pMaterial;
        noesisTex_t      *pTexture;
        int               bufferGroupIndex;
        int               startIndex;
        int               triCount;
        int               minVertexIndex;
        int               vertexCount;
        float             boundsMin[3];
        float             boundsMax[3];
        bool              animatedWater;
        bool              hasBounds;

        OpaqueBatch()
            : pMaterial(nullptr), pTexture(nullptr), bufferGroupIndex(-1)
            , startIndex(0), triCount(0), minVertexIndex(0), vertexCount(0)
            , animatedWater(false), hasBounds(false)
        {
            memset(boundsMin, 0, sizeof(boundsMin));
            memset(boundsMax, 0, sizeof(boundsMax));
        }
    };

    struct Submesh
    {
        std::vector<FFXIVertex> cpuVerts;
        std::vector<FFXIVertex> cpuBindVerts;
        std::vector<FFXISkinVertex> cpuSkinVerts;
        std::vector<DWORD>      cpuIndices;
        std::string             materialName;
        std::string             objectName;

        IDirect3DVertexBuffer9 *pVB;
        IDirect3DIndexBuffer9  *pIB;
        int                     vertCount;
        int                     triCount;
        int                     staticBufferGroupIndex;
        int                     staticVertexOffset;
        int                     staticStartIndex;

        // Immutable render metadata. DATura resolves this once after loading;
        // rendering never has to rescan material or object-name strings.
        noesisMaterial_t       *pResolvedMaterial;
        noesisTex_t            *pResolvedTexture;
        float                   boundsMin[3];
        float                   boundsMax[3];
        float                   boundsCenter[3];
        bool                    hasBounds;
        bool                    environmentObject;
        bool                    animatedWater;
        bool                    softBlend;

        Submesh()
            : pVB(nullptr), pIB(nullptr), vertCount(0), triCount(0)
            , staticBufferGroupIndex(-1), staticVertexOffset(0), staticStartIndex(0)
            , pResolvedMaterial(nullptr), pResolvedTexture(nullptr)
            , hasBounds(false), environmentObject(false), animatedWater(false)
            , softBlend(false)
        {
            memset(boundsMin, 0, sizeof(boundsMin));
            memset(boundsMax, 0, sizeof(boundsMax));
            memset(boundsCenter, 0, sizeof(boundsCenter));
        }
    };

    std::vector<Submesh>  submeshes;
    std::vector<StaticBufferGroup> staticBufferGroups;
    std::vector<OpaqueBatch> opaqueBatches;
    std::vector<size_t> opaqueSubmeshOrder;
    std::vector<size_t> softBlendSubmeshOrder;
    noesisMatData_t      *pMatData;
    noesisAnim_t         *pAnim;
    modelBone_t          *pBones;
    int                   boneCount;
    std::vector<FFXISqleBoneInfo> sqleBones;
    bool                  renderMetadataPrepared;

    noesisModel_t()
        : pMatData(nullptr), pAnim(nullptr), pBones(nullptr), boneCount(0)
        , renderMetadataPrepared(false)
    {}

    // Upload all submesh CPU data to D3D9 vertex/index buffers.
    void BuildD3DBuffers(IDirect3DDevice9 *pDevice);
    void UpdateSubmeshBounds();
    void RestoreBindPose(IDirect3DDevice9 *pDevice);
    void UpdateAnimation(float animTime, IDirect3DDevice9 *pDevice);

    // Release all D3D9 resources (call before destruction or device reset).
    void ReleaseD3DBuffers();
};

//========================================================================================
// rpg primitive types and options
//========================================================================================

enum rpgeoPrimType_e
{
    RPGEO_TRIANGLE              = 0,  // every 3 verts = 1 triangle
    RPGEO_TRIANGLE_STRIP        = 1,  // standard triangle strip
    RPGEO_TRIANGLE_STRIP_FLIPPED= 2,  // strip with initial winding flipped
};

static const int RPGOPT_TRIWINDBACKWARD = 0; // option tag for rpgSetOption

//========================================================================================
// Text parser (minimal — used by Model_FF11_LoadDATSet)
//========================================================================================

struct parseToken_t
{
    char text[512];
};

struct textParser_t
{
    char *pText;
    int   pos;
    int   len;
};

//========================================================================================
// g_mfn stub — only referenced in _DEBUG_MAP_MESHES path, never compiled in release
//========================================================================================

struct NoesisMasterFunctions
{
    float Math_RandFloatOnSeed(float lo, float hi, unsigned int &seed)
    {
        seed = seed * 1664525u + 1013904223u;
        float t = (float)(seed & 0xFFFFFF) / (float)0x1000000;
        return lo + t * (hi - lo);
    }
};
static NoesisMasterFunctions *g_mfn = nullptr; // intentionally null; _DEBUG_MAP_MESHES path unused

//========================================================================================
// noeRAPI_t — rendering API: geometry accumulator + D3D9 resource factory
//========================================================================================

class noeRAPI_t
{
public:
    explicit noeRAPI_t(IDirect3DDevice9 *pDevice = nullptr);
    ~noeRAPI_t();

    // ---- D3D9 device binding -------------------------------------------------------
    void SetDevice(IDirect3DDevice9 *pDevice) { mpDevice = pDevice; }
    IDirect3DDevice9 *GetDevice() const       { return mpDevice; }
    void SetTextureCompressionEnabled(bool enabled);

    // ---- Current-file tracking (for paired file and DAT-set loading) ---------------
    void        SetCurrentFilePath(const char *path);
    const char *GetCurrentFilePath() const { return mCurrentFilePath; }

    // ---- Memory management ---------------------------------------------------------
    void       *Noesis_UnpooledAlloc(int size);
    void        Noesis_UnpooledFree(void *ptr);
    char       *Noesis_PooledString(const char *str);
    modelBone_t*Noesis_AllocBones(int count);

    // ---- Texture factory -----------------------------------------------------------
    // Decode a palette or raw buffer to RGBA32.
    // format: b8g8r8a8 or b5g5r5a1
    unsigned char *Noesis_ImageDecodeRaw(unsigned char *data, int dataSize,
                                          int w, int h, int format);

    // Software-decompress a DXT buffer to RGBA32.
    unsigned char *Noesis_ConvertDXT(int w, int h, unsigned char *data,
                                      noesisTexType_e texType);

    // Allocate a texture (RGBA32 only, no extra flags).
    noesisTex_t   *Noesis_TextureAlloc(const char *name, int w, int h,
                                        unsigned char *data, noesisTexType_e texType);

    // Allocate a texture with extended parameters (texType may be DXT).
    noesisTex_t   *Noesis_TextureAllocEx(const char *name, int w, int h,
                                          unsigned char *data, int dataLen,
                                          noesisTexType_e texType, int flags, int flags2);

    // Allocate count un-initialised noesisMaterial_t objects.
    // pooled=true means they live in the rapi pool and don't need manual deletion.
    noesisMaterial_t *Noesis_GetMaterialList(int count, bool pooled);

    // Build a noesisMatData_t from accumulated material and texture lists.
    noesisMatData_t *Noesis_GetMatDataFromLists(
        CArrayList<noesisMaterial_t *> &mats,
        CArrayList<noesisTex_t *>      &textures);

    // Allocate an empty model container (holds textures/anims with no geometry).
    noesisModel_t *Noesis_AllocModelContainer(noesisMatData_t *pMd,
                                               noesisAnim_t    *pAnim,
                                               int              animCount);

    // ---- File I/O ------------------------------------------------------------------
    // Opens a "paired file" dialog — returns allocated buffer or nullptr.
    unsigned char *Noesis_LoadPairedFile(const char *desc, const char *ext,
                                          int &outSize, void *flags);

    // Read a file from disk — returns allocated buffer or nullptr.
    unsigned char *Noesis_ReadFile(const char *path, int *outSize);

    // Extract the directory portion of filePath into outDir.
    void        Noesis_GetDirForFilePath(char *outDir, const char *filePath);

    // Return the path of the last file checked / opened.
    const char *Noesis_GetLastCheckedName();

    // ---- Text parser ---------------------------------------------------------------
    textParser_t *Parse_InitParser(char *text);
    bool          Parse_GetNextToken(textParser_t *parser, parseToken_t *tok);
    void          Parse_FreeParser(textParser_t *parser);

    // ---- Logging -------------------------------------------------------------------
    void LogOutput(const char *fmt, ...);

    // ---- Preview -------------------------------------------------------------------
    void SetPreviewAngOfs(float *angles) { (void)angles; } // no-op in standalone

    // ---- RPG context ---------------------------------------------------------------
    void *rpgCreateContext();
    void  rpgDestroyContext(void *ctx);
    void  rpgSetOption(int option, bool value);
    void  rpgSetName(char *name);
    void  rpgSetMaterial(const char *matName);
    // Start the next primitive in a separate GPU submesh even when its name
    // and material match an earlier primitive. Transparent zone records need
    // this so their original layer boundaries survive until render sorting.
    void  rpgForceNextSubmesh();
    void  rpgSetTransform(modelMatrix_t *pMat); // NULL = identity

    // Per-vertex attribute setters.  Call any combination, then rpgVertex3f to commit.
    void rpgVertUV2f      (float *uv,      int channel); // NULL = zero UV
    void rpgVertNormal3f  (float *nrm);                  // NULL = zero normal
    void rpgVertBoneIndexI(int   *indices, int count);   // NULL = no bones
    void rpgVertBoneWeightF(float *weights, int count);  // NULL = no weights
    void rpgVertColor4ub  (unsigned char *rgba);
    void rpgVertColor4f   (float *rgba);
    void rpgSetPendingSkinData(const FFXISkinVertex *pSkin); // NULL = no CPU animation skin data
    void rpgVertex3f      (float *pos);     // commits the current pending vertex

    void rpgBegin(rpgeoPrimType_e primType);
    void rpgEnd();

    // Attach extra data to the next constructed model
    void rpgSetExData_Bones    (modelBone_t     *pBones, int count);
    void rpgSetExData_Anims    (noesisAnim_t    *pAnim);
    void rpgSetExData_Materials(noesisMatData_t *pMd);

    // Build a noesisAnim_t from a flat array of per-frame bone matrices.
    // pFrameMats layout: [frame0_bone0, frame0_bone1, ..., frame1_bone0, ...]
    // Returns a heap-allocated noesisAnim_t (owned by the caller / rapi pool).
    noesisAnim_t *rpgAnimFromBonesAndMatsFinish(modelBone_t  *pBones, int boneCount,
                                                  modelMatrix_t *pFrameMats,
                                                  int frameCount, float fps);

    // Combine a list of noesisAnim_t pointers into a single anim (or return the first).
    noesisAnim_t *Noesis_AnimFromAnimsList(CArrayList<noesisAnim_t *> &anims, int count);

    // Multiply every bone matrix by its parent's (local → world).
    void rpgMultiplyBones(modelBone_t *pBones, int count);

    // Finalise accumulated geometry into a noesisModel_t with D3D9 buffers.
    noesisModel_t *rpgConstructModel();
    noesisModel_t *rpgConstructModelAndSort(); // same; sorting is a no-op for now
    void           rpgOptimize();              // no-op for now

private:
    //---- Internal geometry state ---------------------------------------------------

    // Pending per-vertex attributes (reset after each rpgVertex3f)
    struct PendingVertex
    {
        float        pos[3];
        float        nrm[3];
        float        uv[2];
        D3DCOLOR     diffuse;
        int          boneIdx[4];
        float        boneWt[4];
        bool         hasBones;
        FFXISkinVertex skin;

        PendingVertex() { Reset(); }
        void Reset()
        {
            memset(pos,  0, sizeof(pos));
            memset(nrm,  0, sizeof(nrm));
            memset(uv,   0, sizeof(uv));
            diffuse  = 0xFFFFFFFF;
            memset(boneIdx, 0, sizeof(boneIdx));
            memset(boneWt,  0, sizeof(boneWt));
            hasBones = false;
            skin.Reset();
        }
        FFXIVertex ToFFXIVertex() const
        {
            FFXIVertex v;
            memcpy(v.pos, pos, sizeof(pos));
            memcpy(v.nrm, nrm, sizeof(nrm));
            v.diffuse = diffuse;
            memcpy(v.uv,  uv,  sizeof(uv));
            return v;
        }
    };

    // A submesh under construction: a specific material + accumulated triangles
    struct ActiveSubmesh
    {
        std::string              materialName;
        std::string              objectName;
        std::vector<FFXIVertex>  verts;
        std::vector<FFXISkinVertex> skinVerts;
        std::vector<DWORD>       indices;
        bool                     hasSkinning;
        // Maps a vertex fingerprint → index for de-duplication (optional future work)

        ActiveSubmesh() : hasSkinning(false) {}

        void AddTriangle(const PendingVertex &v0, const PendingVertex &v1, const PendingVertex &v2)
        {
            const size_t previousVertCount = verts.size();
            const bool triangleHasSkinning =
                v0.hasBones || v1.hasBones || v2.hasBones ||
                v0.skin.skinned || v1.skin.skinned || v2.skin.skinned;
            if (triangleHasSkinning && !hasSkinning)
            {
                skinVerts.resize(previousVertCount);
                hasSkinning = true;
            }

            DWORD base = (DWORD)verts.size();
            verts.push_back(v0.ToFFXIVertex());
            verts.push_back(v1.ToFFXIVertex());
            verts.push_back(v2.ToFFXIVertex());
            if (hasSkinning)
            {
                skinVerts.push_back(v0.skin);
                skinVerts.push_back(v1.skin);
                skinVerts.push_back(v2.skin);
            }
            indices.push_back(base);
            indices.push_back(base+1);
            indices.push_back(base+2);
        }
    };

    void FlushPrimitive(); // convert mPrimVerts → triangles in current submesh
    ActiveSubmesh &CurrentSubmesh();

    // Texture upload helpers
    void UploadTexture(noesisTex_t *pTex);
    void UploadPendingTextures();

    PendingVertex               mPending;
    rpgeoPrimType_e             mPrimType;
    bool                        mInPrimitive;
    std::vector<PendingVertex>  mPrimVerts;  // vertices accumulated inside Begin/End
    std::vector<ActiveSubmesh>  mSubmeshes;
    std::unordered_map<std::string, size_t> mSubmeshLookup;
    std::string                 mCurrentMaterial;
    std::string                 mCurrentName;
    bool                        mForceNewSubmesh;

    bool        mTriWindBackward; // from rpgSetOption(RPGOPT_TRIWINDBACKWARD)
    bool        mHasTransform;
    RichMat43   mTransform;       // per-object transform set by rpgSetTransform

    // Extra data staged for the next rpgConstructModel call
    modelBone_t     *mpStagedBones;
    int              mStagedBoneCount;
    noesisAnim_t    *mpStagedAnim;
    noesisMatData_t *mpStagedMatData;

    //---- Resource pools ------------------------------------------------------------
    IDirect3DDevice9       *mpDevice;
    bool                    mTextureCompressionEnabled;
    std::vector<void *>     mAllocs;         // tracked for bulk free
    std::vector<char *>     mStringPool;
    std::vector<noesisTex_t *>      mTexPool;
    std::vector<noesisMaterial_t *> mMatPool;
    std::vector<noesisModel_t *>    mModelPool;

    char mCurrentFilePath[MAX_NOESIS_PATH];
};
