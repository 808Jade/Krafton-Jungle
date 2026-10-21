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
    if (Slice >= static_cast<uint32>(ShadowDepthView.Num())) return nullptr;

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
    for (int32 i = 0; i < ShadowDepthView.Num(); ++i)
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
        InNewNumAtlas == static_cast<uint32>(ShadowAtlasDepthView.Num()) && AtlasResolution == InNewAtlasResolution)
    {
        return true;
    }

    NumAtlas = InNewNumAtlas;
    NumAtlas = std::max(NumAtlas, 1u);
    AtlasResolution = InNewAtlasResolution;

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
    Desc.Width = AtlasResolution;
    Desc.Height = AtlasResolution;
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

    // Texture2D: Directional Light Shadow Atlas
    Context->PSSetShaderResources(3, 1, DirectionalShadowSRV.GetAddressOf());

    // StructuredBuffer
    Context->PSSetShaderResources(12, 1, ShadowInfoSRV.GetAddressOf());       
    // Directional Shadow Constants (b5)
    if (DirectionalShadowCB)
    {
        RenderCommand::BindConstantBuffer(5, DirectionalShadowCB.get(), EShaderBindFlagBits::Pixel);
    }

    // s0은 BindMaterial이 머티리얼마다 덮어쓰므로 섀도 샘플러는 s1
    RenderCommand::BindSamplerState(1, ESamplerState::Point, EShaderBindFlagBits::Pixel);
    RenderCommand::BindSamplerState(2, ESamplerState::Comparison_Linear, EShaderBindFlagBits::Pixel);
}


bool FShadowMapManager::EnsureDirectionalShadowMap(uint32 InResolution)
{
    if (DirectionalShadowTexture && DirectionalShadowDSV && DirectionalShadowSRV && DirectionalResolution == InResolution)
    {
        return true;
    }

    DirectionalResolution = InResolution;
    DirectionalShadowTexture.Reset();
    DirectionalShadowDSV.Reset();
    DirectionalShadowSRV.Reset();

    ID3D11Device* Device = RenderCommand::GetDevice();

    D3D11_TEXTURE2D_DESC Desc{};
    Desc.Width = DirectionalResolution;
    Desc.Height = DirectionalResolution;
    Desc.MipLevels = 1;
    Desc.ArraySize = 1;
    Desc.Format = DXGI_FORMAT_R32_TYPELESS;
    Desc.SampleDesc.Count = 1;
    Desc.Usage = D3D11_USAGE_DEFAULT;
    Desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = Device->CreateTexture2D(&Desc, nullptr, DirectionalShadowTexture.GetAddressOf());
    if (FAILED(hr))
    {
        assert(false);
        return false;
    }

    D3D11_DEPTH_STENCIL_VIEW_DESC DsvDesc{};
    DsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    DsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    DsvDesc.Texture2D.MipSlice = 0;

    hr = Device->CreateDepthStencilView(DirectionalShadowTexture.Get(), &DsvDesc, DirectionalShadowDSV.GetAddressOf());
    if (FAILED(hr))
    {
        assert(false);
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC SrvDesc{};
    SrvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    SrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    SrvDesc.Texture2D.MostDetailedMip = 0;
    SrvDesc.Texture2D.MipLevels = 1;

    hr = Device->CreateShaderResourceView(DirectionalShadowTexture.Get(), &SrvDesc, DirectionalShadowSRV.GetAddressOf());
    if (FAILED(hr))
    {
        assert(false);
        return false;
    }

    return true;
}

void FShadowMapManager::SetupDirectionalShadow(const FShadowInfo& DirectionalShadow)
{
    EnsureDirectionalShadowMap(DirectionalResolution);

    if (!DirectionalShadowCB)
    {
        DirectionalShadowCB = RenderCommand::CreateConstantBuffer(sizeof(FDirectionalShadowConstants));
    }

    FDirectionalShadowConstants Constants{};

    // [0번 캐스케이드] 현재 Directional Light의 뷰-프로젝션 행렬
    // @todo 4분할 알고리즘을 넣을 때, 0~3번 슬롯을 채워준다. 그리고 NumCascades = 4 해주면 됨
    Constants.CascadeViewProjection[0] = DirectionalShadow.LightViewProjection;

    // [분할 거리] 모든 픽셀이 0번 캐스케이드에 걸리도록 큰 값 설정
    Constants.CascadeSplits = FVector4(10000.0f, 10000.0f, 10000.0f, 10000.0f);

    // [아틀라스 UV] 현재는 2048x2048 텍스처 1장을 통째로 쓰므로 (Scale: 1.0, Offset: 0.0)
    Constants.CascadeOffsets[0] = FVector4(1.0f, 1.0f, 0.0f, 0.0f);

    // [바이어스]
    Constants.CascadeBiases = FVector4(DirectionalShadow.Bias, 0.002f, 0.004f, 0.008f);
    Constants.NormalBias = DirectionalShadow.NormalBias;

    // [캐스케이드 개수] 현재는 1개
    Constants.NumCascades = 1;

    // 2. GPU 상수 버퍼(DirectionalShadowCB)로 업로드
    RenderCommand::UpdateBufferData(DirectionalShadowCB.get(), &Constants);
}

void FShadowMapManager::BeginDirectionalShadowRender()
{
    ID3D11DeviceContext* Context = RenderCommand::GetContext();
    if (!DirectionalShadowDSV) return;

    Context->OMSetRenderTargets(0, nullptr, DirectionalShadowDSV.Get());
    Context->ClearDepthStencilView(DirectionalShadowDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

    RenderCommand::SetViewport(0, 0, DirectionalResolution, DirectionalResolution);
}

void FShadowMapManager::EndDirectionalShadowRender()
{
    RenderCommand::GetContext()->OMSetRenderTargets(0, nullptr, nullptr);
}

void FShadowMapManager::UpdatePreview(FLightSceneProxy* PreviewShadowMapProxy, const TArray<FShadowInfo>& ShadowInfos)
{
    if (!PreviewShadowMapProxy)
        return;

    TArray<FShadowPreview>& Previews = PreviewShadowMapProxy->GetShadowPreview();

    // Directional Light의 경우 이미 단일 Texture2D 형태이므로 SRV를 직접 전달
    if (PreviewShadowMapProxy->GetLightType() == ELightType::Directional)
    {
        if (Previews.IsValidIndex(0) == false)
        {
            Previews.Add(FShadowPreview());
        }
        Previews[0].ShadowSRV = DirectionalShadowSRV.Get();
        Previews[0].Resolution = DirectionalResolution;
        Previews[0].PreviewUV = { {0.0f, 0.0f}, {1.0f, 1.0f} };

        return;
    }

    const int32 BaseIndex = PreviewShadowMapProxy->GetShadowIndex();
    const int32 ShadowInfoNum = PreviewShadowMapProxy->GetShadowInfoNum();
    if (BaseIndex < 0 || ShadowInfoNum <= 0 || BaseIndex + ShadowInfoNum > ShadowInfos.Num())
        return;

    // Point Light 6장이어도 왜 아틀라스 개수만큼 Num을 set하는가??
    // 0번 1번 아틀라스에 저장되면 상관 없음
    // 3번 4번 아틀라스에 저장되면 ShadowInfo 가 갖고있는 인덱스는 3혹은 4인데 
    // PrevicewShadowTextures 배열에 접근할 때는 추가적인 인덱스 계산이 필요함.
    // 그래서 번호 유지를 위해서 실제로 사용하는 인덱스에만 저장.
    if (PreviewShadowTextures.Num() < NumAtlas)
    {
        PreviewShadowTextures.SetNum(NumAtlas);
        PreviewShadowSRVs.SetNum(NumAtlas);
    }

    TArray<uint32> CopiedPages;

    const float InvAtlas = 1.0f / static_cast<float>(AtlasResolution);

    // ShadowInfo 개수만큼 
    for (int32 i = 0; i < ShadowInfoNum; ++i)
    {
        if (Previews.IsValidIndex(i) == false)
        {
            Previews.Add(FShadowPreview());
        }

        const FShadowInfo& Info = ShadowInfos[BaseIndex + i];
        const uint32 Page = Info.AtlasIndex;

        if (PreviewShadowTextures[Page] == nullptr)
        {
            ID3D11Device* Device = RenderCommand::GetDevice();

            D3D11_TEXTURE2D_DESC PreviewDesc{};
            // 정사각형
            PreviewDesc.Width = AtlasResolution;
            PreviewDesc.Height = AtlasResolution;
            PreviewDesc.MipLevels = 1;
            // 층 수 (Spot 수 + Point 수 × 6 ...)
            PreviewDesc.Format = DXGI_FORMAT_R32_TYPELESS;
            PreviewDesc.SampleDesc.Count = 1;
            PreviewDesc.Usage = D3D11_USAGE_DEFAULT;
            PreviewDesc.ArraySize = 1;
            PreviewDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            HRESULT hr = Device->CreateTexture2D(&PreviewDesc, nullptr, PreviewShadowTextures[Page].ReleaseAndGetAddressOf());
            if (FAILED(hr))
            {
                assert(false);
                return;
            }

            PreviewShadowMapProxy->GetShadowPreview()[i].Resolution = Resolution;

            D3D11_SHADER_RESOURCE_VIEW_DESC PreviewSrv{};
            PreviewSrv.Format = DXGI_FORMAT_R32_FLOAT;
            // ImGui 셰이더가 Texture2D라서
            PreviewSrv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            PreviewSrv.Texture2D.MipLevels = 1;
            hr = Device->CreateShaderResourceView(PreviewShadowTextures[Page].Get(), &PreviewSrv, PreviewShadowSRVs[Page].ReleaseAndGetAddressOf());
            if (FAILED(hr))
            {
                assert(false);
            }
        }

        if (CopiedPages.Find(Page) == -1)
        {
            const UINT SrcSubresource = D3D11CalcSubresource(0, Info.AtlasIndex, 1);

            //해당 Subresource를 복사합니다.
            //    ID3D11Resource * pDstResource, UINT DstSubresource,   // 어디로 (PreviewTexture, 0번)
            //    UINT DstX, UINT DstY, UINT DstZ,                     // 대상 안의 시작 좌표 (0,0,0)
            //    ID3D11Resource * pSrcResource, UINT SrcSubresource,   // 어디서 (ShadowMapArray, 층 k)
            //    const D3D11_BOX * pSrcBox                            // 복사할 영역. nullptr = 서브리소스 전체

            RenderCommand::GetContext()->CopySubresourceRegion(
                PreviewShadowTextures[Page].Get(), 0, 0, 0, 0,
                ShadowMapAtlasBuffer.Get(), SrcSubresource, nullptr);
            CopiedPages.Add(Page);
        }


        Previews[i].ShadowSRV = PreviewShadowSRVs[Page].Get();
        Previews[i].Resolution = Info.AtlasSize;
        Previews[i].PreviewUV.StartUV = { Info.AtlasStartX * InvAtlas, Info.AtlasStartY * InvAtlas };
        Previews[i].PreviewUV.EndUV = { (Info.AtlasStartX + Info.AtlasSize) * InvAtlas, (Info.AtlasStartY + Info.AtlasSize) * InvAtlas };      
    }
   
}

void FShadowMapManager::BuildAtlas(TArray<FShadowInfo>& ShadowInfos)
{
    uint32 CurrentAtlasIndex = 0;
    uint32 CurrentX = 0;
    uint32 CurrentY = 0;
    
    // TODO: Resolution 값 라이트별로 다르게 되는 상황 해결필요
    for (FShadowInfo& Info : ShadowInfos)
    {
        // 아틀라스 계산
        if (CurrentX + Resolution > AtlasResolution)
        {
            CurrentX = 0;
            CurrentY += Resolution;
        }

        if (CurrentY + Resolution > AtlasResolution)
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
    ID3D11ShaderResourceView* NullSRVs[2] = { nullptr, nullptr };
    ID3D11DeviceContext* Context = RenderCommand::GetContext();

    // t2: ShadowMapArray, t3: DirectionalLightShadowmapAtlas
    Context->PSSetShaderResources(2, 2, NullSRVs);
    // t12: StructuredShadowData
    Context->PSSetShaderResources(12, 1, &NullSRVs[0]);
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
