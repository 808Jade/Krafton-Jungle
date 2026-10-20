#include "EnginePCH.h"
#include "ShadowMapManager.h"
#include "RenderCommand.h"
#include "RenderUtil.h"
#include "Shader.h"
#include "Render/RenderResourceManager.h"
#include "Engine/LightSceneProxy.h"

void FShadowMapManager::UploadShadowInfos(const TArray<FShadowInfo>& ShadowInfos)
{
    NumShadowInfos = ShadowInfos.Num();

    if (!EnsureShadowInfoCapacity(NumShadowInfos))
    {
        assert(false);
        return;
    }

    // 일단 아틀라스 해상도 4096으로 하드코딩
    EnsureShadowMapAtlas(4096, NumAtlas);

    ID3D11DeviceContext* Context = RenderCommand::GetContext();
    D3D11_MAPPED_SUBRESOURCE Mapped{};

    if (NumShadowInfos > 0 && SUCCEEDED(Context->Map(ShadowInfoBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
    {
        std::memcpy(Mapped.pData, ShadowInfos.GetData(), NumShadowInfos * sizeof(FShadowInfo));
        Context->Unmap(ShadowInfoBuffer.Get(), 0);
    }
}

ID3D11DepthStencilView* FShadowMapManager::GetSliceDSV(uint32 Slice) const
{
    if (ShadowDepthView.Num() - 1 < Slice) return nullptr;

    return ShadowDepthView[Slice].Get();
}

ID3D11ShaderResourceView* FShadowMapManager::GetShadowMapArraySRV() const
{
    return ShadowMapArraySRV.Get();
}

ID3D11ShaderResourceView* FShadowMapManager::GetShadowInfoSRV() const
{
    return ShadowInfoSRV.Get();
}

bool FShadowMapManager::EnsureShadowMaps(uint32 InResolution, uint32 InNumSlices)
{
    //개수가 바뀐다면, 해상도가 바뀐다면
    if (ShadowMapArrayBuffer && ShadowMapArraySRV && 
        InNumSlices == static_cast<uint32>(ShadowDepthView.Num()) && Resolution == InResolution)
    {
        return true;
    }

    NumSlices = InNumSlices;
    NumSlices = std::max(NumSlices, 1u);
    Resolution = InResolution;

    ShadowMapArrayBuffer.Reset();
    for (uint32 i = 0; i < ShadowDepthView.Num(); i++)
    {
        ShadowDepthView[i].Reset();
    }
    ShadowDepthView.Empty();
    ShadowDepthView.SetNum(NumSlices);

    ShadowMapArraySRV.Reset();


    ID3D11Device* Device = RenderCommand::GetDevice();

    

    // 리소스: 같은 크기의 깊이 이미지 N장
    D3D11_TEXTURE2D_DESC Desc{};
    // 정사각형
    Desc.Width = Resolution;          
    Desc.Height = Resolution;          
    Desc.MipLevels = 1;
    // 층 수 (Spot 수 + Point 수 × 6 ...)
    Desc.ArraySize = NumSlices;                    
    Desc.Format = DXGI_FORMAT_R32_TYPELESS;     
    Desc.SampleDesc.Count = 1;
    Desc.Usage = D3D11_USAGE_DEFAULT;
    Desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    HRESULT hr = Device->CreateTexture2D(&Desc, nullptr, ShadowMapArrayBuffer.GetAddressOf());
    if (FAILED(hr))
    {
        assert(false);
    }

    


    // DSV: 층마다 하나씩 (그릴 때는 한 층만 대상)
    for (uint32 i = 0; i < NumSlices; ++i)
    {
        D3D11_DEPTH_STENCIL_VIEW_DESC Dsv{};
        Dsv.Format = DXGI_FORMAT_D32_FLOAT;
        Dsv.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
        Dsv.Texture2DArray.MipSlice = 0;
        Dsv.Texture2DArray.FirstArraySlice = i;
        Dsv.Texture2DArray.ArraySize = 1;
        hr = Device->CreateDepthStencilView(ShadowMapArrayBuffer.Get(), &Dsv, ShadowDepthView[i].GetAddressOf());
        if (FAILED(hr))
        {
            assert(false);
        }
    }

    // SRV: 전체 층을 한 번에 (셰이더는 이걸 Texture2DArray로 본다)
    D3D11_SHADER_RESOURCE_VIEW_DESC Srv{};
    Srv.Format = DXGI_FORMAT_R32_FLOAT;
    Srv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    Srv.Texture2DArray.MostDetailedMip = 0;
    Srv.Texture2DArray.MipLevels = 1;
    Srv.Texture2DArray.FirstArraySlice = 0;
    Srv.Texture2DArray.ArraySize = NumSlices;
    hr = Device->CreateShaderResourceView(ShadowMapArrayBuffer.Get(), &Srv, ShadowMapArraySRV.GetAddressOf());
    if (FAILED(hr))
    {
        assert(false);
    }

    return true;
}

bool FShadowMapManager::EnsureShadowMapAtlas(uint32 InNewAtlasResolution, uint32 InNewNumAtlas)
{
    //개수가 바뀐다면, 해상도가 바뀐다면
    if (ShadowMapAtlasBuffer && ShadowMapAtlasArraySRV &&
        InNewNumAtlas == static_cast<uint32>(ShadowAtlasDepthView.Num()) && AtlasResoltion == InNewAtlasResolution)
    {
        return true;
    }

    NumAtlas = InNewNumAtlas;
    NumAtlas = std::max(NumAtlas, 1u);
    AtlasResoltion = InNewAtlasResolution;

    ShadowMapAtlasBuffer.Reset();
    for (uint32 i = 0; i < ShadowAtlasDepthView.Num(); i++)
    {
        ShadowAtlasDepthView[i].Reset();
    }
    ShadowAtlasDepthView.Empty();
    ShadowAtlasDepthView.SetNum(NumAtlas);

    ShadowMapAtlasArraySRV.Reset();


    ID3D11Device* Device = RenderCommand::GetDevice();

    D3D11_TEXTURE2D_DESC Desc{};
    // 정사각형
    Desc.Width = AtlasResoltion;
    Desc.Height = AtlasResoltion;
    Desc.MipLevels = 1;
    // 층 수 (Spot 수 + Point 수 × 6 ...)
    Desc.ArraySize = NumAtlas;   // TODO : 아틀라스 배열로 수정
    Desc.Format = DXGI_FORMAT_R32_TYPELESS;
    Desc.SampleDesc.Count = 1;
    Desc.Usage = D3D11_USAGE_DEFAULT;
    Desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = Device->CreateTexture2D(&Desc, nullptr, ShadowMapAtlasBuffer.GetAddressOf());
    if (FAILED(hr))
    {
        assert(false);
    }


    // DSV: 층마다 하나씩 (그릴 때는 한 층만 대상)
    for (uint32 i = 0; i < NumAtlas; ++i)
    {
        D3D11_DEPTH_STENCIL_VIEW_DESC Dsv{};
        Dsv.Format = DXGI_FORMAT_D32_FLOAT;
        Dsv.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
        Dsv.Texture2DArray.MipSlice = 0;
        Dsv.Texture2DArray.FirstArraySlice = i;
        Dsv.Texture2DArray.ArraySize = 1;
        // TODO: RenderCommand 에 DSV 생성 함수 만들기
        hr = Device->CreateDepthStencilView(ShadowMapAtlasBuffer.Get(), &Dsv, ShadowAtlasDepthView[i].GetAddressOf());
        if (FAILED(hr))
        {
            assert(false);
        }
    }

    // SRV: 전체 층을 한 번에 (셰이더는 이걸 Texture2DArray로 본다)
    D3D11_SHADER_RESOURCE_VIEW_DESC Srv{};
    Srv.Format = DXGI_FORMAT_R32_FLOAT;
    Srv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    Srv.Texture2DArray.MostDetailedMip = 0;
    Srv.Texture2DArray.MipLevels = 1;
    Srv.Texture2DArray.FirstArraySlice = 0;
    Srv.Texture2DArray.ArraySize = NumAtlas;  // TODO : 아틀라스 배열로 수정
    // TODO: RenderCommand 에 SRV 생성 함수 만들기
    hr = Device->CreateShaderResourceView(ShadowMapAtlasBuffer.Get(), &Srv, ShadowMapAtlasArraySRV.GetAddressOf());
    if (FAILED(hr))
    {
        assert(false);
    }

    return true;
}

void FShadowMapManager::BeginShadowRender(uint32 Slice)
{
    //DSV, Viewport 바인딩
    //Viewport는 나중에 다른 해상도로 바뀔 것이다.

    ID3D11DeviceContext* Context = RenderCommand::GetContext();

    if (Slice >= static_cast<uint32>(ShadowDepthView.Num())) return;

    //해당하는 DSV 있다면 초기화
    ID3D11DepthStencilView* DSV = ShadowDepthView[Slice].Get();

    // RTV 없이 DSV만 : Depth-only
    Context->OMSetRenderTargets(0, nullptr, DSV);
    Context->ClearDepthStencilView(DSV, D3D11_CLEAR_DEPTH, 1.0f, 0);


    // 섀도맵 해상도로 Viewport (뷰포트 크기와 무관)
    //나중에 맵마다 해상도를 다르게 한다면 좀 더 구조적으로 잡혀야한다.
    //지금은 모든 텍스처가 같다.
    RenderCommand::SetViewport(0, 0, Resolution, Resolution);
}

void FShadowMapManager::ClearAtlas()
{
    ID3D11DeviceContext* Context = RenderCommand::GetContext();
    for (uint32 i = 0; i < NumAtlas; ++i)
        Context->ClearDepthStencilView(ShadowAtlasDepthView[i].Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
}

void FShadowMapManager::BeginShadowRenderWithAtlas(const FShadowInfo& ShadowInfo)
{
    //DSV, Viewport 바인딩
    //Viewport는 나중에 다른 해상도로 바뀔 것이다.

    ID3D11DeviceContext* Context = RenderCommand::GetContext();

    //해당하는 DSV 있다면 초기화
    ID3D11DepthStencilView* DSV = ShadowAtlasDepthView[ShadowInfo.AtlasIndex].Get();

    // RTV 없이 DSV만 : Depth-only
    Context->OMSetRenderTargets(0, nullptr, DSV);

    // 섀도맵 해상도로 Viewport (뷰포트 크기와 무관)
    //나중에 맵마다 해상도를 다르게 한다면 좀 더 구조적으로 잡혀야한다.
    //지금은 모든 텍스처가 같다.
    RenderCommand::SetViewport(ShadowInfo.AtlasStartX, ShadowInfo.AtlasStartY, ShadowInfo.AtlasSize, ShadowInfo.AtlasSize);
}

void FShadowMapManager::EndShadowPass()
{
    // DSV를 풀어야 같은 리소스를 SRV로 읽을 수 있다
    RenderCommand::GetContext()->OMSetRenderTargets(0, nullptr, nullptr);
}

void FShadowMapManager::BindForShading()
{
    ID3D11DeviceContext* Context = RenderCommand::GetContext();
    // Texture2DArray
    Context->PSSetShaderResources(2, 1, ShadowMapAtlasArraySRV.GetAddressOf());
    // StructuredBuffer
    Context->PSSetShaderResources(12, 1, ShadowInfoSRV.GetAddressOf());       
    // s0은 BindMaterial이 머티리얼마다 덮어쓰므로 섀도 샘플러는 s1
    RenderCommand::BindSamplerState(1, ESamplerState::Point, EShaderBindFlagBits::Pixel);
}

void FShadowMapManager::UpdatePreview(FLightSceneProxy* PreviewShadowMapProxy)
{
    //Imgui쪽에 보여줘야하는 건 Texture2D
    //Shadow Map은 Texture2DArray
    //SRV도 Texture3DArray
    //다른 차원의 SRV를 넘기면 안된다.
    //배열 리소스에서 Texture2D 차원 SRV를 만들 수 없어서 보고 싶은 층 하나를 그냥 Texture2D로 복사해서 보내야한다.

    //if (!PreviewTexture || Slice >= NumSlices)
    if (!PreviewShadowMapProxy || PreviewShadowMapProxy->GetShadowIndex() < 0 || 
        static_cast<uint32>(PreviewShadowMapProxy->GetShadowIndex()) >= NumSlices)
        return;

    //텍스처가 없거나 해상도가 바꼈다면
    if (PreviewShadowMapProxy->GetShadowPreview().ShadowTexture == nullptr || PreviewShadowMapProxy->GetShadowPreview().Resolution != Resolution)
    {
        ID3D11Device* Device = RenderCommand::GetDevice();

        //초기화되어 있을 버퍼를 생성해준다.

        // 미리보기 텍스처: 배열과 같은 TYPELESS 포맷이어야 CopySubresourceRegion이 된다
        D3D11_TEXTURE2D_DESC PreviewDesc{};
        // 정사각형
        PreviewDesc.Width = Resolution;
        PreviewDesc.Height = Resolution;
        PreviewDesc.MipLevels = 1;
        // 층 수 (Spot 수 + Point 수 × 6 ...)
        PreviewDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        PreviewDesc.SampleDesc.Count = 1;
        PreviewDesc.Usage = D3D11_USAGE_DEFAULT;
        PreviewDesc.ArraySize = 1;
        PreviewDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        //HRESULT hr = Device->CreateTexture2D(&PreviewDesc, nullptr, PreviewShadowTexture.GetAddressOf());
        HRESULT hr = Device->CreateTexture2D(&PreviewDesc, nullptr, PreviewShadowMapProxy->GetShadowPreview().ShadowTexture.ReleaseAndGetAddressOf());
        if (FAILED(hr))
        {
            assert(false);
        }

        PreviewShadowMapProxy->GetShadowPreview().Resolution = Resolution;

        D3D11_SHADER_RESOURCE_VIEW_DESC PreviewSrv{};
        PreviewSrv.Format = DXGI_FORMAT_R32_FLOAT;
        // ImGui 셰이더가 Texture2D라서
        PreviewSrv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        PreviewSrv.Texture2D.MipLevels = 1;
        //hr = Device->CreateShaderResourceView(PreviewTexture.Get(), &PreviewSrv, PreviewShadowSRV.GetAddressOf());
        hr = Device->CreateShaderResourceView(PreviewShadowMapProxy->GetShadowPreview().ShadowTexture.Get(), &PreviewSrv, PreviewShadowMapProxy->GetShadowPreview().ShadowSRV.ReleaseAndGetAddressOf());
        if (FAILED(hr))
        {
            assert(false);
        }
    }
    

    // 텍스처 하나는 (밉 레벨 × 배열 층) 개수만큼의 개별 이미지로 이루어지고, 이 각각을 서브리소스라고 부릅니다. 
    // D3D 함수들은 서브리소스를 정수 번호 하나로 가리킵니다.
    // MipSlice, ArraySlice, MipLevels 
    /*번호 = MipSlice + ArraySlice × MipLevels

        예 : MipLevels = 3, ArraySize = 2
              Mip0  Mip1  Mip2
        층 0 : 0     1     2
        층 1 : 3     4     5*/
    const UINT SrcSubresource = D3D11CalcSubresource(0, PreviewShadowMapProxy->GetShadowIndex(), 1);

    //해당 Subresource를 복사합니다.
    //    ID3D11Resource * pDstResource, UINT DstSubresource,   // 어디로 (PreviewTexture, 0번)
    //    UINT DstX, UINT DstY, UINT DstZ,                     // 대상 안의 시작 좌표 (0,0,0)
    //    ID3D11Resource * pSrcResource, UINT SrcSubresource,   // 어디서 (ShadowMapArray, 층 k)
    //    const D3D11_BOX * pSrcBox                            // 복사할 영역. nullptr = 서브리소스 전체
    RenderCommand::GetContext()->CopySubresourceRegion(
        PreviewShadowMapProxy->GetShadowPreview().ShadowTexture.Get(), 0, 0, 0, 0,
        ShadowMapArrayBuffer.Get(), SrcSubresource, nullptr);
}

void FShadowMapManager::BuildAtlas(TArray<FShadowInfo>& ShadowInfos)
{
    uint32 CurrentAtlasIndex = 0;
    uint32 CurrentX = 0;
    uint32 CurrentY = 0;
    
    for (FShadowInfo& Info : ShadowInfos)
    {
        // 아틀라스 계산
        if (CurrentX + Resolution > AtlasResoltion)
        {
            CurrentX = 0;
            CurrentY += Resolution;
        }

        if (CurrentY + Resolution > AtlasResoltion)
        {   // 다음줄
            CurrentX = CurrentY = 0;
            ++CurrentAtlasIndex;
        }

        Info.AtlasIndex = CurrentAtlasIndex;
        // TODO: ShadowManager가 갖고있는 값이 아니라 컴포넌트가 들고있기 혹은 라이트 타입 별 전역옵션으로 수정필요
        Info.AtlasSize = Resolution;    
        Info.AtlasStartX = CurrentX;
        Info.AtlasStartY = CurrentY;

        CurrentX += Resolution;
    }
    NumAtlas = CurrentAtlasIndex + 1;
}

void FShadowMapManager::UnbindForShading()
{
    ID3D11ShaderResourceView* NullSRV = nullptr;
    ID3D11DeviceContext* Context = RenderCommand::GetContext();

    //t12 StructuredShadowData, ShadowMap
    Context->PSSetShaderResources(2, 1, &NullSRV);
    Context->PSSetShaderResources(12, 1, &NullSRV);
}

bool FShadowMapManager::EnsureShadowInfoCapacity(uint32 RequiredCount)
{
    RequiredCount = std::max(RequiredCount, 1u);

    if (ShadowInfoBuffer && ShadowInfoSRV && RequiredCount <= ShadowInfoCapacity)
    {
        return true;
    }

    uint32 NewCapacity = std::max(ShadowInfoCapacity * 2, 64u);
    while (NewCapacity < RequiredCount)
    {
        NewCapacity *= 2;
    }

    ShadowInfoBuffer.Reset();
    ShadowInfoSRV.Reset();
    ShadowInfoCapacity = 0;

    D3D11_BUFFER_DESC BufferDesc{};
    BufferDesc.ByteWidth = NewCapacity * sizeof(FShadowInfo);
    BufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    BufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    BufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    BufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    BufferDesc.StructureByteStride = sizeof(FShadowInfo);

    ID3D11Device* Device = RenderCommand::GetDevice();

    if (FAILED(Device->CreateBuffer(&BufferDesc, nullptr, ShadowInfoBuffer.GetAddressOf())))
    {
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc{};
    SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
    SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    SRVDesc.Buffer.FirstElement = 0;
    SRVDesc.Buffer.NumElements = NewCapacity;

    if (FAILED(Device->CreateShaderResourceView(ShadowInfoBuffer.Get(), &SRVDesc, ShadowInfoSRV.GetAddressOf())))
    {
        ShadowInfoBuffer.Reset();
        return false;
    }

    ShadowInfoCapacity = NewCapacity;
    return true;
}
