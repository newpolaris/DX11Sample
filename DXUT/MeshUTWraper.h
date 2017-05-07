#pragma once

#include "d3dUtil.h"
#include "d3d9.h"
#include "d3dx9tex.h"

#define DXUTERR_MEDIANOTFOUND           MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x0903)


#if defined(DEBUG) || defined(_DEBUG)
#ifndef V
#define V(x)           { hr = (x); if( FAILED(hr) ) { DXTrace( __FILE__, (DWORD)__LINE__, hr, L#x, true ); } }
#endif
#ifndef V_RETURN
#define V_RETURN(x)    { hr = (x); if( FAILED(hr) ) { return DXTrace( __FILE__, (DWORD)__LINE__, hr, L#x, true ); } }
#endif
#else
#ifndef V
#define V(x)           { hr = (x); }
#endif
#ifndef V_RETURN
#define V_RETURN(x)    { hr = (x); if( FAILED(hr) ) { return hr; } }
#endif
#endif

#ifndef SAFE_DELETE
#define SAFE_DELETE(p)       { if (p) { delete (p);     (p)=NULL; } }
#endif
#ifndef SAFE_DELETE_ARRAY
#define SAFE_DELETE_ARRAY(p) { if (p) { delete[] (p);   (p)=NULL; } }
#endif
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if (p) { (p)->Release(); (p)=NULL; } }
#endif

bool DXUTFindMediaSearchParentDirs(__in_ecount(cchSearch) WCHAR* strSearchPath,
	int cchSearch,
	__in WCHAR* strStartAt,
	__in WCHAR* strLeafName);

HRESULT WINAPI DXUTFindDXSDKMediaFileCch(__in_ecount(cchDest) WCHAR* strDestPath,
	int cchDest,
	__in LPCWSTR strFilename);

ID3D11DeviceContext* DXUTGetD3D11DeviceContext();

enum DXUTCACHE_SOURCELOCATION
{
    DXUTCACHE_LOCATION_FILE,
    DXUTCACHE_LOCATION_RESOURCE
};

struct DXUTCache_Texture
{
    DXUTCACHE_SOURCELOCATION Location;
    WCHAR   wszSource[MAX_PATH];
    HMODULE hSrcModule;
    UINT Width;
    UINT Height;
    UINT Depth;
    UINT MipLevels;
    UINT MiscFlags;
    union
    {
        DWORD Usage9;
        D3D11_USAGE Usage11;
    };
    union
    {
        D3DFORMAT Format9;
        DXGI_FORMAT Format;
    };
    union
    {
        D3DPOOL Pool9;
        UINT CpuAccessFlags;
    };
    union
    {
        D3DRESOURCETYPE Type9;
        UINT BindFlags;
    };
    IDirect3DBaseTexture9* pTexture9;
    ID3D11ShaderResourceView* pSRV11;

            DXUTCache_Texture()
            {
                pTexture9 = NULL;
                pSRV11 = NULL;
            }
};

class CDXUTResourceCache
{
public:
                            ~CDXUTResourceCache();

    HRESULT                 CreateTextureFromFile( LPDIRECT3DDEVICE9 pDevice, LPCTSTR pSrcFile,
                                                   LPDIRECT3DTEXTURE9* ppTexture );
    HRESULT                 CreateTextureFromFile( LPDIRECT3DDEVICE9 pDevice, LPCSTR pSrcFile,
                                                   LPDIRECT3DTEXTURE9* ppTexture );
    HRESULT                 CreateTextureFromFile( ID3D11Device* pDevice, ID3D11DeviceContext *pContext, LPCTSTR pSrcFile,
                                                   ID3D11ShaderResourceView** ppOutputRV, bool bSRGB=false );
    HRESULT                 CreateTextureFromFile( ID3D11Device* pDevice, ID3D11DeviceContext *pContext, LPCSTR pSrcFile,
                                                   ID3D11ShaderResourceView** ppOutputRV, bool bSRGB=false );
    HRESULT                 CreateTextureFromFileEx( LPDIRECT3DDEVICE9 pDevice, LPCTSTR pSrcFile, UINT Width,
                                                     UINT Height, UINT MipLevels, DWORD Usage, D3DFORMAT Format,
                                                     D3DPOOL Pool, DWORD Filter, DWORD MipFilter, D3DCOLOR ColorKey,
                                                     D3DXIMAGE_INFO* pSrcInfo, PALETTEENTRY* pPalette,
                                                     LPDIRECT3DTEXTURE9* ppTexture );
    HRESULT                 CreateTextureFromFileEx( ID3D11Device* pDevice, ID3D11DeviceContext* pContext, LPCTSTR pSrcFile,
                                                     D3DX11_IMAGE_LOAD_INFO* pLoadInfo, ID3DX11ThreadPump* pPump,
                                                     ID3D11ShaderResourceView** ppOutputRV, bool bSRGB );
    HRESULT                 CreateTextureFromResource( LPDIRECT3DDEVICE9 pDevice, HMODULE hSrcModule,
                                                       LPCTSTR pSrcResource, LPDIRECT3DTEXTURE9* ppTexture );
    HRESULT                 CreateTextureFromResourceEx( LPDIRECT3DDEVICE9 pDevice, HMODULE hSrcModule,
                                                         LPCTSTR pSrcResource, UINT Width, UINT Height, UINT MipLevels,
                                                         DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, DWORD Filter,
                                                         DWORD MipFilter, D3DCOLOR ColorKey, D3DXIMAGE_INFO* pSrcInfo,
                                                         PALETTEENTRY* pPalette, LPDIRECT3DTEXTURE9* ppTexture );
    HRESULT                 CreateCubeTextureFromFile( LPDIRECT3DDEVICE9 pDevice, LPCTSTR pSrcFile,
                                                       LPDIRECT3DCUBETEXTURE9* ppCubeTexture );
    HRESULT                 CreateCubeTextureFromFileEx( LPDIRECT3DDEVICE9 pDevice, LPCTSTR pSrcFile, UINT Size,
                                                         UINT MipLevels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool,
                                                         DWORD Filter, DWORD MipFilter, D3DCOLOR ColorKey,
                                                         D3DXIMAGE_INFO* pSrcInfo, PALETTEENTRY* pPalette,
                                                         LPDIRECT3DCUBETEXTURE9* ppCubeTexture );
    HRESULT                 CreateCubeTextureFromResource( LPDIRECT3DDEVICE9 pDevice, HMODULE hSrcModule,
                                                           LPCTSTR pSrcResource,
                                                           LPDIRECT3DCUBETEXTURE9* ppCubeTexture );
    HRESULT                 CreateCubeTextureFromResourceEx( LPDIRECT3DDEVICE9 pDevice, HMODULE hSrcModule,
                                                             LPCTSTR pSrcResource, UINT Size, UINT MipLevels,
                                                             DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, DWORD Filter,
                                                             DWORD MipFilter, D3DCOLOR ColorKey,
                                                             D3DXIMAGE_INFO* pSrcInfo, PALETTEENTRY* pPalette,
                                                             LPDIRECT3DCUBETEXTURE9* ppCubeTexture );
    HRESULT                 CreateVolumeTextureFromFile( LPDIRECT3DDEVICE9 pDevice, LPCTSTR pSrcFile,
                                                         LPDIRECT3DVOLUMETEXTURE9* ppVolumeTexture );
    HRESULT                 CreateVolumeTextureFromFileEx( LPDIRECT3DDEVICE9 pDevice, LPCTSTR pSrcFile, UINT Width,
                                                           UINT Height, UINT Depth, UINT MipLevels, DWORD Usage,
                                                           D3DFORMAT Format, D3DPOOL Pool, DWORD Filter,
                                                           DWORD MipFilter, D3DCOLOR ColorKey,
                                                           D3DXIMAGE_INFO* pSrcInfo, PALETTEENTRY* pPalette,
                                                           LPDIRECT3DVOLUMETEXTURE9* ppTexture );
    HRESULT                 CreateVolumeTextureFromResource( LPDIRECT3DDEVICE9 pDevice, HMODULE hSrcModule,
                                                             LPCTSTR pSrcResource,
                                                             LPDIRECT3DVOLUMETEXTURE9* ppVolumeTexture );
    HRESULT                 CreateVolumeTextureFromResourceEx( LPDIRECT3DDEVICE9 pDevice, HMODULE hSrcModule,
                                                               LPCTSTR pSrcResource, UINT Width, UINT Height,
                                                               UINT Depth, UINT MipLevels, DWORD Usage,
                                                               D3DFORMAT Format, D3DPOOL Pool, DWORD Filter,
                                                               DWORD MipFilter, D3DCOLOR ColorKey,
                                                               D3DXIMAGE_INFO* pSrcInfo, PALETTEENTRY* pPalette,
                                                               LPDIRECT3DVOLUMETEXTURE9* ppVolumeTexture );
    HRESULT                 CreateFont( LPDIRECT3DDEVICE9 pDevice, UINT Height, UINT Width, UINT Weight,
                                        UINT MipLevels, BOOL Italic, DWORD CharSet, DWORD OutputPrecision,
                                        DWORD Quality, DWORD PitchAndFamily, LPCTSTR pFacename, LPD3DXFONT* ppFont );
    HRESULT CreateFontIndirect( LPDIRECT3DDEVICE9 pDevice, CONST D3DXFONT_DESC *pDesc, LPD3DXFONT *ppFont );
    HRESULT                 CreateEffectFromFile( LPDIRECT3DDEVICE9 pDevice, LPCTSTR pSrcFile,
                                                  const D3DXMACRO* pDefines, LPD3DXINCLUDE pInclude, DWORD Flags,
                                                  LPD3DXEFFECTPOOL pPool, LPD3DXEFFECT* ppEffect,
                                                  LPD3DXBUFFER* ppCompilationErrors );
    HRESULT                 CreateEffectFromResource( LPDIRECT3DDEVICE9 pDevice, HMODULE hSrcModule,
                                                      LPCTSTR pSrcResource, const D3DXMACRO* pDefines,
                                                      LPD3DXINCLUDE pInclude, DWORD Flags, LPD3DXEFFECTPOOL pPool,
                                                      LPD3DXEFFECT* ppEffect, LPD3DXBUFFER* ppCompilationErrors );

public:
    HRESULT                 OnCreateDevice( IDirect3DDevice9* pd3dDevice );
    HRESULT                 OnResetDevice( IDirect3DDevice9* pd3dDevice );
    HRESULT                 OnLostDevice();
    HRESULT                 OnDestroyDevice();

protected:
    friend CDXUTResourceCache& DXUTGetGlobalResourceCache();
    friend HRESULT WINAPI   DXUTInitialize3DEnvironment();
    friend HRESULT WINAPI   DXUTReset3DEnvironment();
    friend void WINAPI      DXUTCleanup3DEnvironment( bool bReleaseSettings );

                            CDXUTResourceCache()
                            {
                            }

	std::vector<DXUTCache_Texture> m_TextureCache;
    // std::vector<DXUTCache_Effect> m_EffectCache;
    // CGrowableArray <DXUTCache_Font> m_FontCache;
};

CDXUTResourceCache& DXUTGetGlobalResourceCache();

inline void DXUT_SetDebugName( ID3D11DeviceChild* pObj, const CHAR* pstrName )
{
    if ( pObj )
        pObj->SetPrivateData( WKPDID_D3DDebugObjectName, lstrlenA(pstrName), pstrName );
}