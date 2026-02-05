#include "VulkanRenderer.h"
#include <IConsole.h>
#include <ILog.h>

// --------------------------------------------------------------------------------
// Vulkan Renderer Implementation Skeleton
// --------------------------------------------------------------------------------

CVulkanRenderer::CVulkanRenderer()
{
    m_type = R_VULKAN_RENDERER;
}

CVulkanRenderer::~CVulkanRenderer()
{
}

#ifndef PS2
WIN_HWND CVulkanRenderer::Init(int x,int y,int width,int height,unsigned int cbpp, int zbpp, int sbits, bool fullscreen,WIN_HINSTANCE hinst, WIN_HWND Glhwnd, WIN_HDC Glhdc, WIN_HGLRC hGLrc, bool bReInit)
{
    m_width = width;
    m_height = height;
    m_cbpp = cbpp;
    m_zbpp = zbpp;
    m_sbpp = sbits;
    m_FullScreen = fullscreen;
    m_hWnd = Glhwnd;

    if (bReInit)
        return m_hWnd;

    if (m_pLog) m_pLog->Log("Initializing Vulkan Renderer...");

    // TODO: Initialize Vulkan Instance
    // TODO: Create Surface
    // TODO: Pick Physical Device
    // TODO: Create Logical Device
    // TODO: Create Swapchain

    return m_hWnd;
}
#else
bool CVulkanRenderer::Init(int x,int y,int width,int height,unsigned int cbpp, int zbpp, int sbits, bool fullscreen, bool bReInit)
{
    return true;
}
#endif

void CVulkanRenderer::ShutDown(bool bReInit)
{
    if (m_pLog) m_pLog->Log("Shutting down Vulkan Renderer...");

    // TODO: Destroy Vulkan resources
}

void CVulkanRenderer::BeginFrame()
{
    // TODO: Acquire next image from swapchain
    // TODO: Begin command buffer recording
}

void CVulkanRenderer::Update()
{
    // TODO: End command buffer recording
    // TODO: Submit to queue
    // TODO: Present image
}

// --------------------------------------------------------------------------------
// IRenderer Interface Implementation Stubs
// --------------------------------------------------------------------------------

bool CVulkanRenderer::SetCurrentContext(WIN_HWND hWnd) { return true; }
bool CVulkanRenderer::CreateContext(WIN_HWND hWnd, bool bAllowFSAA) { return true; }
bool CVulkanRenderer::DeleteContext(WIN_HWND hWnd) { return true; }

int CVulkanRenderer::CreateRenderTarget (int nWidth, int nHeight, ETEX_Format eTF) { return 0; }
bool CVulkanRenderer::DestroyRenderTarget (int nHandle) { return true; }
bool CVulkanRenderer::SetRenderTarget (int nHandle) { return true; }

int CVulkanRenderer::EnumDisplayFormats(TArray<SDispFormat>& Formats, bool bReset) { return 0; }
int CVulkanRenderer::EnumAAFormats(TArray<SAAFormat>& Formats, bool bReset) { return 0; }

bool CVulkanRenderer::ChangeResolution(int nNewWidth, int nNewHeight, int nNewColDepth, int nNewRefreshHZ, bool bFullScreen)
{
    // TODO: Recreate swapchain
    return true;
}

void CVulkanRenderer::Reset() {}

void *CVulkanRenderer::GetDynVBPtr(int nVerts, int &nOffs, int Pool) { return NULL; }
void CVulkanRenderer::DrawDynVB(int nOffs, int Pool, int nVerts) {}
void CVulkanRenderer::DrawDynVB(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *pBuf, ushort *pInds, int nVerts, int nInds, int nPrimType) {}

CVertexBuffer *CVulkanRenderer::CreateBuffer(int vertexcount,int vertexformat, const char *szSource, bool bDynamic) { return NULL; }
void CVulkanRenderer::CreateBuffer(int size, int vertexformat, CVertexBuffer *buf, int Type, const char *szSource) {}
void CVulkanRenderer::ReleaseBuffer(CVertexBuffer *bufptr) {}
void CVulkanRenderer::DrawBuffer(CVertexBuffer *src,SVertexStream *indicies,int numindices, int offsindex, int prmode,int vert_start,int vert_stop, CMatInfo *mi) {}
void CVulkanRenderer::UpdateBuffer(CVertexBuffer *dest,const void *src,int vertexcount, bool bUnLock, int offs, int Type) {}

void CVulkanRenderer::CreateIndexBuffer(SVertexStream *dest,const void *src,int indexcount) {}
void CVulkanRenderer::UpdateIndexBuffer(SVertexStream *dest,const void *src,int indexcount, bool bUnLock) {}
void CVulkanRenderer::ReleaseIndexBuffer(SVertexStream *dest) {}

void CVulkanRenderer::CheckError(const char *comment) {}

void CVulkanRenderer::Draw3dBBox(const Vec3 &mins, const Vec3 &maxs, int nPrimType) {}
void CVulkanRenderer::SetCamera(const CCamera &cam) {}
void CVulkanRenderer::SetViewport(int x, int y, int width, int height) {}
void CVulkanRenderer::SetScissor(int x, int y, int width, int height) {}

void CVulkanRenderer::SetCullMode(int mode) {}
bool CVulkanRenderer::EnableFog(bool enable) { return true; }
void CVulkanRenderer::SetFog(float density,float fogstart,float fogend,const float *color,int fogmode) {}
void CVulkanRenderer::SetFogColor(float * color) {}

void CVulkanRenderer::EnableTexGen(bool enable) {}
void CVulkanRenderer::SetTexgen(float scaleX,float scaleY,float translateX,float translateY) {}
void CVulkanRenderer::SetTexgen3D(float x1, float y1, float z1, float x2, float y2, float z2) {}
void CVulkanRenderer::SetLodBias(float value) {}
void CVulkanRenderer::EnableVSync(bool enable) {}

void CVulkanRenderer::DrawTriStrip(CVertexBuffer *src, int vert_num) {}

// Matrix operations
void CVulkanRenderer::PushMatrix() {}
void CVulkanRenderer::RotateMatrix(float a,float x,float y,float z) {}
void CVulkanRenderer::RotateMatrix(const Vec3 & angels) {}
void CVulkanRenderer::TranslateMatrix(float x,float y,float z) {}
void CVulkanRenderer::ScaleMatrix(float x,float y,float z) {}
void CVulkanRenderer::TranslateMatrix(const Vec3 &pos) {}
void CVulkanRenderer::MultMatrix(float * mat) {}
void CVulkanRenderer::LoadMatrix(const Matrix44 *src) {}
void CVulkanRenderer::PopMatrix() {}

void CVulkanRenderer::EnableTMU(bool enable) {}
void CVulkanRenderer::SelectTMU(int tnum) {}

bool CVulkanRenderer::ChangeDisplay(unsigned int width,unsigned int height,unsigned int cbpp) { return true; }
void CVulkanRenderer::ChangeViewport(unsigned int x,unsigned int y,unsigned int width,unsigned int height) {}

unsigned int CVulkanRenderer::DownLoadToVideoMemory(unsigned char *data,int w, int h, ETEX_Format eTFSrc, ETEX_Format eTFDst, int nummipmap, bool repeat, int filter, int Id, char *szCacheName, int flags) { return 0; }
void CVulkanRenderer::UpdateTextureInVideoMemory(uint tnum, unsigned char *newdata,int posx,int posy,int w,int h,ETEX_Format eTF) {}
unsigned int CVulkanRenderer::LoadTexture(const char * filename,int *tex_type,unsigned int def_tid,bool compresstodisk,bool bWarn) { return 0; }
bool CVulkanRenderer::SetGammaDelta(const float fGamma) { return true; }
void CVulkanRenderer::RemoveTexture(unsigned int TextureId) {}
void CVulkanRenderer::RemoveTexture(ITexPic * pTexPic) {}
void CVulkanRenderer::SetTexture(int tnum, ETexType Type) {}
void CVulkanRenderer::SetWhiteTexture() {}
void CVulkanRenderer::Draw2dImage(float xpos,float ypos,float w,float h,int texture_id,float s0,float t0,float s1,float t1,float angle,float r,float g,float b,float a,float z) {}

int CVulkanRenderer::SetPolygonMode(int mode) { return 0; }
void CVulkanRenderer::ResetToDefault() {}
int CVulkanRenderer::GenerateAlphaGlowTexture(float k) { return 0; }
void CVulkanRenderer::SetMaterialColor(float r, float g, float b, float a) {}
int CVulkanRenderer::LoadAnimatedTexture(const char * format,const int nCount) { return 0; }
char * CVulkanRenderer::GetStatusText(ERendStats type) { return ""; }
void CVulkanRenderer::GetMemoryUsage(ICrySizer* Sizer) {}

void CVulkanRenderer::ProjectToScreen( float ptx, float pty, float ptz, float *sx, float *sy, float *sz ) {}
int CVulkanRenderer::UnProject(float sx, float sy, float sz, float *px, float *py, float *pz, const float modelMatrix[16], const float projMatrix[16], const int viewport[4]) { return 0; }
int CVulkanRenderer::UnProjectFromScreen( float  sx, float  sy, float  sz, float *px, float *py, float *pz) { return 0; }
void CVulkanRenderer::Draw2dLine(float x1,float y1,float x2,float y2) {}
void CVulkanRenderer::DrawLine(const Vec3 & vPos1, const Vec3 & vPos2) {}
void CVulkanRenderer::DrawLineColor(const Vec3 & vPos1, const CFColor & vColor1, const Vec3 & vPos2, const CFColor & vColor2) {}
void CVulkanRenderer::DrawBall(float x, float y, float z, float radius) {}
void CVulkanRenderer::DrawBall(const Vec3 & pos, float radius ) {}
void CVulkanRenderer::DrawPoint(float x, float y, float z, float fSize) {}

// Shadows
void CVulkanRenderer::PrepareDepthMap(ShadowMapFrustum * lof, bool make_new_tid) {}
void CVulkanRenderer::SetupShadowOnlyPass(int Num, ShadowMapFrustum * pFrustum, Vec3 * vShadowTrans, const float fShadowScale, Vec3 vObjTrans, float fObjScale, const Vec3 vObjAngles, Matrix44 * pObjMat) {}
void CVulkanRenderer::DrawAllShadowsOnTheScreen() {}
void CVulkanRenderer::OnEntityDeleted(IEntityRender * pEntityRender) {}

void CVulkanRenderer::SetClipPlane( int id, float * params ) {}
void CVulkanRenderer::EF_SetClipPlane (bool bEnable, float *pPlane, bool bRefract) {}
void CVulkanRenderer::SetColorOp(byte eCo, byte eAo, byte eCa, byte eAa) {}

void CVulkanRenderer::GetModelViewMatrix(float *mat) {}
void CVulkanRenderer::GetProjectionMatrix(float *mat) {}

void CVulkanRenderer::DrawObjSprites(list2<CStatObjInst*> *pList, float fMaxViewDist, CObjManager *pObjMan) {}
void CVulkanRenderer::DrawQuad(const Vec3 &right, const Vec3 &up, const Vec3 &origin,int nFlipMode) {}
void CVulkanRenderer::DrawQuad(float dy,float dx, float dz, float x, float y, float z) {}

void CVulkanRenderer::ClearDepthBuffer() {}
void CVulkanRenderer::ClearColorBuffer(const Vec3 vColor) {}
void CVulkanRenderer::ReadFrameBuffer(unsigned char * pRGB, int nSizeX, int nSizeY, bool bBackBuffer, bool bRGBA, int nScaledX, int nScaledY) {}

void CVulkanRenderer::TransformTextureMatrix(float x, float y, float angle, float scale) {}
void CVulkanRenderer::ResetTextureMatrix() {}
void CVulkanRenderer::ScreenShot(const char *filename) {}

char* CVulkanRenderer::GetVertexProfile(bool bSupportedProfile) { return ""; }
char* CVulkanRenderer::GetPixelProfile(bool bSupportedProfile) { return ""; }
unsigned int CVulkanRenderer::MakeSprite(float object_scale, int tex_size, float angle, IStatObj * pStatObj, uchar * pTmpBuffer, uint def_tid) { return 0; }
unsigned int CVulkanRenderer::Make3DSprite(int nTexSize, float fAngleStep, IStatObj * pStatObj) { return 0; }

void CVulkanRenderer::Set2DMode(bool enable, int ortox, int ortoy) {}
int CVulkanRenderer::ScreenToTexture() { return 0; }
void CVulkanRenderer::SetTexClampMode(bool clamp) {}

void CVulkanRenderer::DrawPoints(Vec3 v[], int nump, CFColor& col, int flags) {}
void CVulkanRenderer::DrawLines(Vec3 v[], int nump, CFColor& col, int flags, float fGround) {}

// Shaders / Effects
void CVulkanRenderer::EF_Release(int nFlags) {}
void CVulkanRenderer::EF_PipelineShutdown() {}
void CVulkanRenderer::EF_LightMaterial(SLightMaterial *lm, int Flags) {}
void CVulkanRenderer::EF_CheckOverflow(int nVerts, int nTris, CRendElement *re) {}
void CVulkanRenderer::EF_Start(SShader *ef, SShader *efState, SRenderShaderResources *Res, int nFog, CRendElement *re) {}
void CVulkanRenderer::EF_Start(SShader *ef, SShader *efState, SRenderShaderResources *Res, CRendElement *re) {}
bool CVulkanRenderer::EF_SetLightHole(Vec3 vPos, Vec3 vNormal, int idTex, float fScale, bool bAdditive) { return true; }
STexPic *CVulkanRenderer::EF_MakePhongTexture(int Exp) { return NULL; }
void CVulkanRenderer::EF_EndEf3D (int nFlags) {}
void CVulkanRenderer::EF_EndEf2D(bool bSort) {}
int CVulkanRenderer::EF_RegisterFogVolume(float fMaxFogDist, float fFogLayerZ, CFColor color, int nIndex, bool bCaustics) { return 0; }

bool CVulkanRenderer::FontUploadTexture(class CFBitmap*, ETEX_Format eTF) { return true; }
int CVulkanRenderer::FontCreateTexture(int Width, int Height, byte *pData, ETEX_Format eTF) { return 0; }
bool CVulkanRenderer::FontUpdateTexture(int nTexId, int X, int Y, int USize, int VSize, byte *pData) { return true; }
void CVulkanRenderer::FontReleaseTexture(class CFBitmap *pBmp) {}
void CVulkanRenderer::FontSetTexture(class CFBitmap*, int nFilterMode) {}
void CVulkanRenderer::FontSetTexture(int nTexId, int nFilterMode) {}
void CVulkanRenderer::FontSetRenderingState(unsigned long nVirtualScreenWidth, unsigned long nVirtualScreenHeight) {}
void CVulkanRenderer::FontSetBlending(int src, int dst) {}
void CVulkanRenderer::FontRestoreRenderingState() {}
WIN_HWND CVulkanRenderer::GetHWND() { return m_hWnd; }


extern "C" DLL_EXPORT IRenderer * PackageRenderConstructor(int argc, char * argv[], SCryRenderInterface * sp)
{
    CVulkanRenderer *pRenderer = new CVulkanRenderer();
    if (sp)
    {
        pRenderer->m_pLog = sp->ipLog;
        pRenderer->m_pSystem = sp->ipSystem;
    }
    return pRenderer;
}
