#include "EnginePCH.h"
#include "Render/SceneLightResources.h"
#include "Render/RenderCommand.h"
#include "Render/MeshPassProcessor.h"
#include "Render/Renderer.h"
#include "Components/LightComponentBase.h"
#include <algorithm>
#include <cstring>

namespace
{
	static_assert(offsetof(FDeferredLightConstants, ViewportSize) == 96);
	static_assert(offsetof(FDeferredLightConstants, Ambient) == 112);
	static_assert(offsetof(FDeferredLightConstants, LightCounts) == 128);
	static_assert(offsetof(FDeferredLightConstants, LightOffsets) == 144);
	static_assert(offsetof(FDeferredLightConstants, SkyColorIntensity) == 160);
	static_assert(sizeof(FDeferredLightConstants) == 176);

	// 용량이 모자랄 때만 두 배로 키우고 Map 한 번으로 채운다.
	bool UploadStructured(TUniquePtr<FStructuredBuffer>& Buffer, const void* Data, uint32 Count, uint32 Stride)
	{
		if (!Count) return true;
		if (!Buffer || Buffer->GetCapacity() < Count)
		{
			const uint32 PreviousCapacity = Buffer ? Buffer->GetCapacity() : 0;
			const uint32 Maximum = UINT32_MAX / Stride;
			const uint32 Doubled = PreviousCapacity <= Maximum / 2
				? std::max(1u, PreviousCapacity * 2) : Maximum;
			auto NewBuffer = RenderCommand::CreateDynamicStructuredBuffer(std::max(Count, Doubled), Stride);
			if (!NewBuffer || !NewBuffer->GetBuffer() || !NewBuffer->GetSRV()) return false;
			Buffer = std::move(NewBuffer);
		}
		void* Destination = RenderCommand::MapWriteDiscard(Buffer.get());
		if (!Destination) return false;
		std::memcpy(Destination, Data, static_cast<size_t>(Count) * Stride);
		RenderCommand::Unmap(Buffer.get());
		return true;
	}
}

bool FSceneLightResources::Init()
{
	if (bInitialized) return true;
	ForwardLightCB = RenderCommand::CreateConstantBuffer(sizeof(FForwardLightingConstants));
	DeferredLightCB = RenderCommand::CreateConstantBuffer(sizeof(FDeferredLightConstants));
	ShadowCB = RenderCommand::CreateConstantBuffer(sizeof(FShadowConstants));
	ShadowParameterBuffer = RenderCommand::CreateDynamicStructuredBuffer(1, sizeof(FShadowParameters));
	LightShadowRangeBuffer = RenderCommand::CreateDynamicStructuredBuffer(1, sizeof(FLightShadowRange));
	if (!ForwardTileGrid.Init())
		HTR_LOG(Warning, "Forward tile culling compute shader unavailable");
	bInitialized = ForwardLightCB && DeferredLightCB && ShadowCB &&
		ShadowParameterBuffer && ShadowParameterBuffer->GetBuffer() && ShadowParameterBuffer->GetSRV() &&
		LightShadowRangeBuffer && LightShadowRangeBuffer->GetBuffer() && LightShadowRangeBuffer->GetSRV();
	return bInitialized;
}

bool FSceneLightResources::UploadViewLights(const TArray<FLightRenderParameters>& Lights,
	uint32 (&Counts)[4], uint32 (&Offsets)[4])
{
	// ELightType 순서(방향광, 포인트, 스포트, 렉트)가 곧 버퍼 안의 구간 순서다.
	constexpr uint32 TypeCount = 4;
	uint32 TypeCounts[TypeCount] = {};
	for (const FLightRenderParameters& Light : Lights)
		if (Light.Type < TypeCount) ++TypeCounts[Light.Type];

	const uint32 DirectionalType = static_cast<uint32>(ELightType::Directional);
	const uint32 PointType = static_cast<uint32>(ELightType::Point);
	const uint32 SpotType = static_cast<uint32>(ELightType::Spot);
	const uint32 RectType = static_cast<uint32>(ELightType::Rect);
	Counts[0] = TypeCounts[PointType];
	Counts[1] = TypeCounts[SpotType];
	Counts[2] = TypeCounts[DirectionalType];
	Counts[3] = TypeCounts[RectType];
	Offsets[2] = 0;
	Offsets[0] = Counts[2];
	Offsets[1] = Counts[2] + Counts[0];
	Offsets[3] = Counts[2] + Counts[0] + Counts[1];

	const uint32 Total = Counts[0] + Counts[1] + Counts[2] + Counts[3];
	PackedLightCount = Total;
	PackedLightIndices.SetNum(Lights.Num());
	for (uint32& Index : PackedLightIndices) Index = UINT32_MAX;
	ShadowParameters.Reset();
	LightShadowRanges.Reset();
	if (!Total) return true;

	uint32 Next[TypeCount] = {};
	Next[DirectionalType] = Offsets[2];
	Next[PointType] = Offsets[0];
	Next[SpotType] = Offsets[1];
	Next[RectType] = Offsets[3];

	TArray<FLightRenderParameters> Packed;
	Packed.SetNum(static_cast<int32>(Total));
	for (int32 Index = 0; Index < Lights.Num(); ++Index)
	{
		const FLightRenderParameters& Light = Lights[Index];
		if (Light.Type >= TypeCount) continue;
		const uint32 PackedIndex = Next[Light.Type]++;
		Packed[PackedIndex] = Light;
		PackedLightIndices[Index] = PackedIndex;
	}
	return UploadStructured(SceneLightBuffer, Packed.GetData(), Total, sizeof(FLightRenderParameters));
}

bool FSceneLightResources::BuildViewShadowParameters(const TArray<FShadowDrawPass>& Passes)
{
	bShadowsUploaded = false;
	ShadowParameters.Reset();
	ShadowParameters.Reserve(Passes.Num());
	LightShadowRanges.Reset();
	LightShadowRanges.SetNum(static_cast<int32>(PackedLightCount));

	for (const FShadowDrawPass& Pass : Passes)
	{
		if (Pass.LightIndex >= static_cast<uint32>(PackedLightIndices.Num())) return false;
		const uint32 PackedIndex = PackedLightIndices[Pass.LightIndex];
		if (PackedIndex >= PackedLightCount || Pass.Parameters.AtlasIndex == UINT32_MAX) return false;

		FLightShadowRange& Range = LightShadowRanges[PackedIndex];
		if (Range.ViewCount == 0)
			Range.StartIndex = static_cast<uint32>(ShadowParameters.Num());

		// 같은 조명의 면들을 FaceIndex 순서대로 연속 저장한다.
		if (Pass.FaceIndex != Range.ViewCount ||
			Range.StartIndex + Range.ViewCount != static_cast<uint32>(ShadowParameters.Num())) return false;
		ShadowParameters.Add(Pass.Parameters);
		++Range.ViewCount;
	}
	return true;
}

bool FSceneLightResources::UploadViewShadows(bool bEnabled, uint32 AtlasCount)
{
	if (!bInitialized) return false;
	bShadowsUploaded = false;
	FShadowConstants Constants{};
	// 비활성 프레임과 업로드 실패 시에도 이전 프레임의 활성 상태가 남지 않는다.
	RenderCommand::UpdateBufferData(ShadowCB.get(), &Constants, sizeof(Constants));
	if (!bEnabled || ShadowParameters.IsEmpty()) return true;
	if (AtlasCount == 0 || AtlasCount > MaxShadowAtlases)
	{
		HTR_LOG(Error, "[Shadow] Deferred lighting supports up to {} atlases; requested {}", MaxShadowAtlases, AtlasCount);
		return false;
	}
	if (LightShadowRanges.Num() != static_cast<int32>(PackedLightCount)) return false;
	for (const FShadowParameters& Parameters : ShadowParameters)
		if (Parameters.AtlasIndex >= AtlasCount) return false;

	if (!UploadStructured(ShadowParameterBuffer, ShadowParameters.GetData(),
		static_cast<uint32>(ShadowParameters.Num()), sizeof(FShadowParameters)) ||
		!UploadStructured(LightShadowRangeBuffer, LightShadowRanges.GetData(),
		static_cast<uint32>(LightShadowRanges.Num()), sizeof(FLightShadowRange))) return false;

	Constants.ShadowsEnabled = 1;
	RenderCommand::UpdateBufferData(ShadowCB.get(), &Constants, sizeof(Constants));
	bShadowsUploaded = true;
	return true;
}

void FSceneLightResources::BindDeferredShadows(FRenderer* Renderer) const
{
	Renderer->BindStructured(5, bShadowsUploaded ? ShadowParameterBuffer.get() : nullptr, EShaderBindFlagBits::Pixel);
	Renderer->BindStructured(6, bShadowsUploaded ? LightShadowRangeBuffer.get() : nullptr, EShaderBindFlagBits::Pixel);
	Renderer->BindConstant(1, ShadowCB.get(), EShaderBindFlagBits::Pixel);
}
