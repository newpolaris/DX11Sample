#include "MeshUTWraper.h"
#include "d3dApp.h"

DXGI_FORMAT MAKE_SRGB( DXGI_FORMAT format )
{
	return format;
}

//--------------------------------------------------------------------------------------
// Search parent directories starting at strStartAt, and appending strLeafName
// at each parent directory.  It stops at the root directory.
//--------------------------------------------------------------------------------------
bool DXUTFindMediaSearchParentDirs( __in_ecount(cchSearch) WCHAR* strSearchPath, 
                                    int cchSearch, 
                                    __in WCHAR* strStartAt, 
                                    __in WCHAR* strLeafName )
{
    WCHAR strFullPath[MAX_PATH] =
    {
        0
    };
    WCHAR strFullFileName[MAX_PATH] =
    {
        0
    };
    WCHAR strSearch[MAX_PATH] =
    {
        0
    };
    WCHAR* strFilePart = NULL;

    GetFullPathName( strStartAt, MAX_PATH, strFullPath, &strFilePart );
    if( strFilePart == NULL )
        return false;

    while( strFilePart != NULL && *strFilePart != '\0' )
    {
        swprintf_s( strFullFileName, MAX_PATH, L"%s\\%s", strFullPath, strLeafName );
        if( GetFileAttributes( strFullFileName ) != 0xFFFFFFFF )
        {
            wcscpy_s( strSearchPath, cchSearch, strFullFileName );
            return true;
        }

        swprintf_s( strSearch, MAX_PATH, L"%s\\..", strFullPath );
        GetFullPathName( strSearch, MAX_PATH, strFullPath, &strFilePart );
    }

    return false;
}


//--------------------------------------------------------------------------------------
// Tries to find the location of a SDK media file
//       cchDest is the size in WCHARs of strDestPath.  Be careful not to 
//       pass in sizeof(strDest) on UNICODE builds.
//--------------------------------------------------------------------------------------
HRESULT WINAPI DXUTFindDXSDKMediaFileCch(__in_ecount(cchDest) WCHAR* strDestPath,
                                         int cchDest, 
                                         __in LPCWSTR strFilename )
{

	FILE* pFP = _wfopen(strFilename, L"r");
	assert(pFP);
	if (pFP == nullptr)
		return DXUTERR_MEDIANOTFOUND;
	fclose(pFP);

    wcscpy_s( strDestPath, cchDest, strFilename );
	return S_OK;
}

Microsoft::WRL::ComPtr<ID3D11DeviceContext> pContext;

ID3D11DeviceContext* DXUTGetD3D11DeviceContext()
{
	if (!pContext) {
		g_Device->GetImmediateContext(&pContext);
	}
	return pContext.Get();
}

CDXUTResourceCache & DXUTGetGlobalResourceCache()
{
    // Using an accessor function gives control of the construction order
    static CDXUTResourceCache cache;
    return cache;
}


CDXUTResourceCache::~CDXUTResourceCache()
{
	OnDestroyDevice();

	m_TextureCache.clear();
	// m_EffectCache.RemoveAll();
	// m_FontCache.RemoveAll();
}

//--------------------------------------------------------------------------------------
HRESULT CDXUTResourceCache::CreateTextureFromFileEx( ID3D11Device* pDevice, ID3D11DeviceContext* pContext, LPCTSTR pSrcFile,
                                                     D3DX11_IMAGE_LOAD_INFO* pLoadInfo, ID3DX11ThreadPump* pPump,
                                                     ID3D11ShaderResourceView** ppOutputRV, bool bSRGB )
{

	bool is10L9 = false;
    HRESULT hr = S_OK;
    D3DX11_IMAGE_LOAD_INFO ZeroInfo;	//D3DX11_IMAGE_LOAD_INFO has a default constructor
    D3DX11_IMAGE_INFO SrcInfo;

    if( !pLoadInfo )
    {
        pLoadInfo = &ZeroInfo;
    }

    if( !pLoadInfo->pSrcInfo )
    {
        D3DX11GetImageInfoFromFile( pSrcFile, NULL, &SrcInfo, NULL );
        pLoadInfo->pSrcInfo = &SrcInfo;

        pLoadInfo->Format = pLoadInfo->pSrcInfo->Format;
    }

    // Search the cache for a matching entry.
    for( int i = 0; i < m_TextureCache.size(); ++i )
    {
        DXUTCache_Texture& Entry = m_TextureCache[i];
        if( Entry.Location == DXUTCACHE_LOCATION_FILE &&
            !lstrcmpW( Entry.wszSource, pSrcFile ) &&
            Entry.Width == pLoadInfo->Width &&
            Entry.Height == pLoadInfo->Height &&
            Entry.MipLevels == pLoadInfo->MipLevels &&
            Entry.Usage11 == pLoadInfo->Usage &&
            Entry.Format == pLoadInfo->Format &&
            Entry.CpuAccessFlags == pLoadInfo->CpuAccessFlags &&
            Entry.BindFlags == pLoadInfo->BindFlags &&
            Entry.MiscFlags == pLoadInfo->MiscFlags )
        {
            // A match is found. Obtain the IDirect3DTexture9 interface and return that.
            return Entry.pSRV11->QueryInterface( __uuidof( ID3D11ShaderResourceView ), ( LPVOID* )ppOutputRV );
        }
    }

#if defined(_PROFILE) || defined(_DEBUG)
    CHAR strFileA[MAX_PATH];
    WideCharToMultiByte( CP_ACP, 0, pSrcFile, -1, strFileA, MAX_PATH, NULL, FALSE );
    CHAR* pstrName = strrchr( strFileA, '\\' );
    if( pstrName == NULL )
        pstrName = strFileA;
    else
        pstrName++;
#endif

    //Ready a new entry to the texture cache
    //Do this before creating the texture since pLoadInfo may be volatile
    DXUTCache_Texture NewEntry;
    NewEntry.Location = DXUTCACHE_LOCATION_FILE;
    wcscpy_s( NewEntry.wszSource, MAX_PATH, pSrcFile );
    NewEntry.Width = pLoadInfo->Width;
    NewEntry.Height = pLoadInfo->Height;
    NewEntry.MipLevels = pLoadInfo->MipLevels;
    NewEntry.Usage11 = pLoadInfo->Usage;
    // 10L9 can't handle typesless, so we cant make a typesless format 
    if (is10L9 && bSRGB) {
        NewEntry.Format = MAKE_SRGB(pLoadInfo->Format);
    }else {
        NewEntry.Format = pLoadInfo->Format;
    }
    NewEntry.CpuAccessFlags = pLoadInfo->CpuAccessFlags;
    NewEntry.BindFlags = pLoadInfo->BindFlags;
    NewEntry.MiscFlags = pLoadInfo->MiscFlags;

    //Create the rexture
    ID3D11Texture2D* pRes = NULL;
    hr = D3DX11CreateTextureFromFile( pDevice, pSrcFile, pLoadInfo, pPump, ( ID3D11Resource** )&pRes, NULL );

    if( FAILED( hr ) )
        return hr;
    D3D11_TEXTURE2D_DESC tex_dsc;
    pRes->GetDesc(&tex_dsc);



    if (bSRGB ) {
        // This is a workaround so that we can load linearly, but sample in SRGB.  Right now, we can't load
        // as linear since D3DX will try to do conversion on load.  Loading as TYPELESS doesn't work either, and
        // loading as typed _UNORM doesn't allow us to create an SRGB view.

        // on d3d11 featuer levels this is just a copy, but on 10L9 we must use a cpu side copy with 2 staging resources.
        ID3D11Texture2D* unormStaging = NULL;
        ID3D11Texture2D* srgbStaging = NULL;

        D3D11_TEXTURE2D_DESC CopyDesc;
        pRes->GetDesc( &CopyDesc );

        pLoadInfo->BindFlags = 0;
        pLoadInfo->CpuAccessFlags = D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ;
        pLoadInfo->Depth = 0;
        pLoadInfo->Filter = D3DX11_FILTER_LINEAR;
        pLoadInfo->FirstMipLevel = 0;
        pLoadInfo->Format = CopyDesc.Format;
        pLoadInfo->Height = CopyDesc.Height;
        pLoadInfo->MipFilter = D3DX11_FILTER_LINEAR;
        pLoadInfo->MiscFlags = CopyDesc.MiscFlags;
        pLoadInfo->Usage = D3D11_USAGE_STAGING;
        pLoadInfo->Width = CopyDesc.Width;

        CopyDesc.BindFlags = 0;
        CopyDesc.Usage = D3D11_USAGE_STAGING;
        CopyDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ;
        CopyDesc.Format = MAKE_SRGB(CopyDesc.Format);

        hr = D3DX11CreateTextureFromFile( pDevice, pSrcFile, pLoadInfo, pPump, ( ID3D11Resource** )&unormStaging, NULL );
        DXUT_SetDebugName( unormStaging, "CDXUTResourceCache" );

        hr = pDevice->CreateTexture2D(&CopyDesc, NULL, &srgbStaging);
        DXUT_SetDebugName( srgbStaging, "CDXUTResourceCache" );
        pContext->CopyResource( srgbStaging, unormStaging );
        ID3D11Texture2D* srgbGPU;

        pRes->GetDesc( &CopyDesc );
        CopyDesc.Format = MAKE_SRGB(CopyDesc.Format);
        hr = pDevice->CreateTexture2D(&CopyDesc, NULL, &srgbGPU);
        pContext->CopyResource( srgbGPU, srgbStaging );

        SAFE_RELEASE(pRes);
        SAFE_RELEASE(srgbStaging);
        SAFE_RELEASE(unormStaging);
        pRes = srgbGPU;
    }

    DXUT_SetDebugName( pRes, pstrName );

    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    if( bSRGB )
        SRVDesc.Format = MAKE_SRGB( ZeroInfo.Format );
    else
        SRVDesc.Format = ZeroInfo.Format;
    if( pLoadInfo->pSrcInfo->ResourceDimension == D3D11_RESOURCE_DIMENSION_TEXTURE1D )
    {
        SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
        SRVDesc.Texture1D.MostDetailedMip = 0;
        SRVDesc.Texture1D.MipLevels = pLoadInfo->pSrcInfo->MipLevels;
    }
    else if( pLoadInfo->pSrcInfo->ResourceDimension == D3D11_RESOURCE_DIMENSION_TEXTURE2D )
    {
        SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        SRVDesc.Texture2D.MostDetailedMip = 0;
        SRVDesc.Texture2D.MipLevels = pLoadInfo->pSrcInfo->MipLevels;

        if( pLoadInfo->pSrcInfo->MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE )
        {
            SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
            SRVDesc.TextureCube.MostDetailedMip = 0;
            SRVDesc.TextureCube.MipLevels = pLoadInfo->pSrcInfo->MipLevels;
        }
    }
    else if( pLoadInfo->pSrcInfo->ResourceDimension == D3D11_RESOURCE_DIMENSION_TEXTURE3D )
    {
        SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
        SRVDesc.Texture3D.MostDetailedMip = 0;
        SRVDesc.Texture3D.MipLevels = pLoadInfo->pSrcInfo->MipLevels;
    }
    if (bSRGB) {
        SRVDesc.Format = MAKE_SRGB(tex_dsc.Format);
    }else {
        SRVDesc.Format = tex_dsc.Format;
    }
    SRVDesc.Texture2D.MipLevels = tex_dsc.MipLevels;
    SRVDesc.Texture2D.MostDetailedMip = 0;
    hr = pDevice->CreateShaderResourceView( pRes, &SRVDesc, ppOutputRV );
    pRes->Release();
    if( FAILED( hr ) )
        return hr;

    DXUT_SetDebugName( *ppOutputRV, pstrName );

    ( *ppOutputRV )->QueryInterface( __uuidof( ID3D11ShaderResourceView ), ( LPVOID* )&NewEntry.pSRV11 );

    m_TextureCache.push_back( NewEntry );

    return S_OK;
}

HRESULT CDXUTResourceCache::OnDestroyDevice()
{
	/*
    // Release all resources
    for( int i = m_EffectCache.GetSize() - 1; i >= 0; --i )
    {
        SAFE_RELEASE( m_EffectCache[i].pEffect );
        m_EffectCache.Remove( i );
    }
    for( int i = m_FontCache.GetSize() - 1; i >= 0; --i )
    {
        SAFE_RELEASE( m_FontCache[i].pFont );
        m_FontCache.Remove( i );
    }
	*/
    for( int i = m_TextureCache.size() - 1; i >= 0; --i )
    {
        SAFE_RELEASE( m_TextureCache[i].pTexture9 );
        SAFE_RELEASE( m_TextureCache[i].pSRV11 );
    }
	m_TextureCache.clear();

    return S_OK;
}


HRESULT CDXUTResourceCache::CreateTextureFromFile(LPDIRECT3DDEVICE9 pDevice, LPCTSTR pSrcFile, LPDIRECT3DTEXTURE9 * ppTexture)
{
	return E_NOTIMPL;
}

//--------------------------------------------------------------------------------------
HRESULT CDXUTResourceCache::CreateTextureFromFile( LPDIRECT3DDEVICE9 pDevice, LPCSTR pSrcFile,
                                                   LPDIRECT3DTEXTURE9* ppTexture )
{
    WCHAR szSrcFile[MAX_PATH];
    MultiByteToWideChar( CP_ACP, 0, pSrcFile, -1, szSrcFile, MAX_PATH );
    szSrcFile[MAX_PATH - 1] = 0;

    return CreateTextureFromFile( pDevice, szSrcFile, ppTexture );
}

//--------------------------------------------------------------------------------------
HRESULT CDXUTResourceCache::CreateTextureFromFile( ID3D11Device* pDevice, ID3D11DeviceContext *pContext, LPCTSTR pSrcFile,
                                                   ID3D11ShaderResourceView** ppOutputRV, bool bSRGB )
{
    return CreateTextureFromFileEx( pDevice, pContext, pSrcFile, NULL, NULL, ppOutputRV, bSRGB );
}

//--------------------------------------------------------------------------------------
HRESULT CDXUTResourceCache::CreateTextureFromFile( ID3D11Device* pDevice, ID3D11DeviceContext *pContext, LPCSTR pSrcFile,
                                                   ID3D11ShaderResourceView** ppOutputRV, bool bSRGB )
{
    WCHAR szSrcFile[MAX_PATH];
    MultiByteToWideChar( CP_ACP, 0, pSrcFile, -1, szSrcFile, MAX_PATH );
    szSrcFile[MAX_PATH - 1] = 0;

    return CreateTextureFromFile( pDevice, pContext, szSrcFile, ppOutputRV, bSRGB );
}
