#ifndef VULKAN_RENDERER_H
#define VULKAN_RENDERER_H

#include "../Common/Renderer.h"

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <vector>

struct VulkanBuffer {
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkDeviceSize size;
};

class CVulkanRenderer : public CRenderer
{
public:
    CVulkanRenderer();
    virtual ~CVulkanRenderer();

    // IRenderer interface implementation
#ifndef PS2
    virtual WIN_HWND Init(int x,int y,int width,int height,unsigned int cbpp, int zbpp, int sbits, bool fullscreen,WIN_HINSTANCE hinst, WIN_HWND Glhwnd=0, WIN_HDC Glhdc=0, WIN_HGLRC hGLrc=0, bool bReInit=false);
#else
    virtual bool Init(int x,int y,int width,int height,unsigned int cbpp, int zbpp, int sbits, bool fullscreen, bool bReInit=false);
#endif

    virtual void ShutDown(bool bReInit=false);
    virtual void BeginFrame();
    virtual void Update();

    // New methods
    virtual int GetMaxActiveShadowMaps() { return -1; }
    virtual bool IsHiResScreenshotSupported() { return true; }

    // Implementing pure virtuals
    virtual bool SetCurrentContext(WIN_HWND hWnd);
    virtual bool CreateContext(WIN_HWND hWnd, bool bAllowFSAA=false);
    virtual bool DeleteContext(WIN_HWND hWnd);
    virtual int CreateRenderTarget (int nWidth, int nHeight, ETEX_Format eTF=eTF_8888);
    virtual bool DestroyRenderTarget (int nHandle);
    virtual bool SetRenderTarget (int nHandle);
    virtual int EnumDisplayFormats(TArray<SDispFormat>& Formats, bool bReset);
    virtual int EnumAAFormats(TArray<SAAFormat>& Formats, bool bReset);
    virtual bool ChangeResolution(int nNewWidth, int nNewHeight, int nNewColDepth, int nNewRefreshHZ, bool bFullScreen);
    virtual void Reset();
    virtual void *GetDynVBPtr(int nVerts, int &nOffs, int Pool);
    virtual void DrawDynVB(int nOffs, int Pool, int nVerts);
    virtual void DrawDynVB(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *pBuf, ushort *pInds, int nVerts, int nInds, int nPrimType);
    virtual CVertexBuffer *CreateBuffer(int vertexcount,int vertexformat, const char *szSource, bool bDynamic=false);
    virtual void CreateBuffer(int size, int vertexformat, CVertexBuffer *buf, int Type, const char *szSource);
    virtual void ReleaseBuffer(CVertexBuffer *bufptr);
    virtual void DrawBuffer(CVertexBuffer *src,SVertexStream *indicies,int numindices, int offsindex, int prmode,int vert_start=0,int vert_stop=0, CMatInfo *mi=NULL);
    virtual void UpdateBuffer(CVertexBuffer *dest,const void *src,int vertexcount, bool bUnLock, int offs=0, int Type=0);
    virtual void CreateIndexBuffer(SVertexStream *dest,const void *src,int indexcount);
    virtual void UpdateIndexBuffer(SVertexStream *dest,const void *src,int indexcount, bool bUnLock=true);
    virtual void ReleaseIndexBuffer(SVertexStream *dest);
    virtual void CheckError(const char *comment);
    virtual void Draw3dBBox(const Vec3 &mins, const Vec3 &maxs, int nPrimType);
    virtual void SetCamera(const CCamera &cam);
    virtual void SetViewport(int x=0, int y=0, int width=0, int height=0);
    virtual void SetScissor(int x=0, int y=0, int width=0, int height=0);
    virtual void SetCullMode(int mode=R_CULL_BACK);
    virtual bool EnableFog(bool enable);
    virtual void SetFog(float density,float fogstart,float fogend,const float *color,int fogmode);
    virtual void SetFogColor(float * color);
    virtual void EnableTexGen(bool enable);
    virtual void SetTexgen(float scaleX,float scaleY,float translateX=0,float translateY=0);
    virtual void SetTexgen3D(float x1, float y1, float z1, float x2, float y2, float z2);
    virtual void SetLodBias(float value=R_DEFAULT_LODBIAS);
    virtual void EnableVSync(bool enable);
    virtual void DrawTriStrip(CVertexBuffer *src, int vert_num=4);
    virtual void PushMatrix();
    virtual void RotateMatrix(float a,float x,float y,float z);
    virtual void RotateMatrix(const Vec3 & angels);
    virtual void TranslateMatrix(float x,float y,float z);
    virtual void ScaleMatrix(float x,float y,float z);
    virtual void TranslateMatrix(const Vec3 &pos);
    virtual void MultMatrix(float * mat);
    virtual void LoadMatrix(const Matrix44 *src=0);
    virtual void PopMatrix();
    virtual void EnableTMU(bool enable);
    virtual void SelectTMU(int tnum);
    virtual bool ChangeDisplay(unsigned int width,unsigned int height,unsigned int cbpp);
    virtual void ChangeViewport(unsigned int x,unsigned int y,unsigned int width,unsigned int height);
    virtual unsigned int DownLoadToVideoMemory(unsigned char *data,int w, int h, ETEX_Format eTFSrc, ETEX_Format eTFDst, int nummipmap, bool repeat=true, int filter=FILTER_BILINEAR, int Id=0, char *szCacheName=NULL, int flags=0);
    virtual void UpdateTextureInVideoMemory(uint tnum, unsigned char *newdata,int posx,int posy,int w,int h,ETEX_Format eTF=eTF_0888);
    virtual unsigned int LoadTexture(const char * filename,int *tex_type=NULL,unsigned int def_tid=0,bool compresstodisk=true,bool bWarn=true);
    virtual bool SetGammaDelta(const float fGamma);
    virtual void RemoveTexture(unsigned int TextureId);
    virtual void RemoveTexture(ITexPic * pTexPic);
    virtual void SetTexture(int tnum, ETexType Type=(ETexType)0);
    virtual void SetWhiteTexture();
    virtual void Draw2dImage(float xpos,float ypos,float w,float h,int texture_id,float s0=0,float t0=0,float s1=1,float t1=1,float angle=0,float r=1,float g=1,float b=1,float a=1,float z=1);
    virtual int SetPolygonMode(int mode);
    virtual void ResetToDefault();
    virtual int GenerateAlphaGlowTexture(float k);
    virtual void SetMaterialColor(float r, float g, float b, float a);
    virtual int LoadAnimatedTexture(const char * format,const int nCount);
    virtual char * GetStatusText(ERendStats type);
    virtual void GetMemoryUsage(ICrySizer* Sizer);
    virtual void ProjectToScreen( float ptx, float pty, float ptz, float *sx, float *sy, float *sz );
    virtual int UnProject(float sx, float sy, float sz, float *px, float *py, float *pz, const float modelMatrix[16], const float projMatrix[16], const int viewport[4]);
    virtual int UnProjectFromScreen( float  sx, float  sy, float  sz, float *px, float *py, float *pz);
    virtual void Draw2dLine(float x1,float y1,float x2,float y2);
    virtual void DrawLine(const Vec3 & vPos1, const Vec3 & vPos2);
    virtual void DrawLineColor(const Vec3 & vPos1, const CFColor & vColor1, const Vec3 & vPos2, const CFColor & vColor2);
    virtual void DrawBall(float x, float y, float z, float radius);
    virtual void DrawBall(const Vec3 & pos, float radius );
    virtual void DrawPoint(float x, float y, float z, float fSize = 0.0f);
    virtual void PrepareDepthMap(ShadowMapFrustum * lof, bool make_new_tid=0);
    virtual void SetupShadowOnlyPass(int Num, ShadowMapFrustum * pFrustum, Vec3 * vShadowTrans, const float fShadowScale, Vec3 vObjTrans, float fObjScale, const Vec3 vObjAngles, Matrix44 * pObjMat);
    virtual void DrawAllShadowsOnTheScreen();
    virtual void OnEntityDeleted(IEntityRender * pEntityRender);
    virtual void SetClipPlane( int id, float * params );
    virtual void EF_SetClipPlane (bool bEnable, float *pPlane, bool bRefract);
    virtual void SetColorOp(byte eCo, byte eAo, byte eCa, byte eAa);
    virtual void GetModelViewMatrix(float *mat);
    virtual void GetProjectionMatrix(float *mat);
    virtual void DrawObjSprites(list2<CStatObjInst*> *pList, float fMaxViewDist, CObjManager *pObjMan);
    virtual void DrawQuad(const Vec3 &right, const Vec3 &up, const Vec3 &origin,int nFlipMode=0);
    virtual void DrawQuad(float dy,float dx, float dz, float x, float y, float z);
    virtual void ClearDepthBuffer();
    virtual void ClearColorBuffer(const Vec3 vColor);
    virtual void ReadFrameBuffer(unsigned char * pRGB, int nSizeX, int nSizeY, bool bBackBuffer, bool bRGBA, int nScaledX=-1, int nScaledY=-1);
    virtual void TransformTextureMatrix(float x, float y, float angle, float scale);
    virtual void ResetTextureMatrix();
    virtual void ScreenShot(const char *filename=NULL);
    virtual char* GetVertexProfile(bool bSupportedProfile);
    virtual char* GetPixelProfile(bool bSupportedProfile);
    virtual unsigned int MakeSprite(float object_scale, int tex_size, float angle, IStatObj * pStatObj, uchar * pTmpBuffer, uint def_tid);
    virtual unsigned int Make3DSprite(int nTexSize, float fAngleStep, IStatObj * pStatObj);
    virtual void Set2DMode(bool enable, int ortox, int ortoy);
    virtual int ScreenToTexture();
    virtual void SetTexClampMode(bool clamp);
    virtual void DrawPoints(Vec3 v[], int nump, CFColor& col, int flags);
    virtual void DrawLines(Vec3 v[], int nump, CFColor& col, int flags, float fGround);
    virtual void EF_Release(int nFlags);
    virtual void EF_PipelineShutdown();
    virtual void EF_LightMaterial(SLightMaterial *lm, int Flags);
    virtual void EF_CheckOverflow(int nVerts, int nTris, CRendElement *re);
    virtual void EF_Start(SShader *ef, SShader *efState, SRenderShaderResources *Res, int nFog, CRendElement *re);
    virtual void EF_Start(SShader *ef, SShader *efState, SRenderShaderResources *Res, CRendElement *re);
    virtual bool EF_SetLightHole(Vec3 vPos, Vec3 vNormal, int idTex, float fScale=1.0f, bool bAdditive=true);
    virtual STexPic *EF_MakePhongTexture(int Exp);
    virtual void EF_EndEf3D (int nFlags);
    virtual void EF_EndEf2D(bool bSort);
    virtual int EF_RegisterFogVolume(float fMaxFogDist, float fFogLayerZ, CFColor color, int nIndex=-1, bool bCaustics=false);
    virtual bool FontUploadTexture(class CFBitmap*, ETEX_Format eTF=eTF_8888);
    virtual int FontCreateTexture(int Width, int Height, byte *pData, ETEX_Format eTF=eTF_8888);
    virtual bool FontUpdateTexture(int nTexId, int X, int Y, int USize, int VSize, byte *pData);
    virtual void FontReleaseTexture(class CFBitmap *pBmp);
    virtual void FontSetTexture(class CFBitmap*, int nFilterMode);
    virtual void FontSetTexture(int nTexId, int nFilterMode);
    virtual void FontSetRenderingState(unsigned long nVirtualScreenWidth, unsigned long nVirtualScreenHeight);
    virtual void FontSetBlending(int src, int dst);
    virtual void FontRestoreRenderingState();
    virtual WIN_HWND GetHWND();

public:
    ILog* m_pLog;
    ISystem* m_pSystem;

    // Helper methods for Vulkan
    void CreateVulkanBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

private:
    WIN_HWND m_hWnd;

    VkInstance m_Instance;
    VkPhysicalDevice m_PhysicalDevice;
    VkDevice m_Device;
    VkQueue m_Queue;
    VkSurfaceKHR m_Surface;
    VkSwapchainKHR m_Swapchain;

    std::vector<VkImage> m_SwapchainImages;
    VkFormat m_SwapchainImageFormat;
    VkExtent2D m_SwapchainExtent;

    VkCommandPool m_CommandPool;
    std::vector<VkCommandBuffer> m_CommandBuffers;
    std::vector<VkSemaphore> m_ImageAvailableSemaphores;
    std::vector<VkSemaphore> m_RenderFinishedSemaphores;
    std::vector<VkFence> m_InFlightFences;
    uint32_t m_CurrentFrame;
    uint32_t m_ImageIndex;

    void CreateInstance();
    void CreateSurface(WIN_HINSTANCE hinst, WIN_HWND hWnd);
    void PickPhysicalDevice();
    void CreateLogicalDevice();
    void CreateSwapchain();
    void CreateCommandPool();
    void CreateCommandBuffers();
    void CreateSyncObjects();
    bool IsDeviceSuitable(VkPhysicalDevice device);
    bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };
    SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);
};

extern "C" DLL_EXPORT IRenderer * PackageRenderConstructor(int argc, char * argv[], SCryRenderInterface * sp);

#endif
