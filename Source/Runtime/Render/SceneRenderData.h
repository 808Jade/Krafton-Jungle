#pragma once

#include "Render/RenderPacket.h"
#include "Render/ObjectConstants.h"
#include "Engine/PrimitiveSceneProxy.h"
#include "Engine/LightSceneProxy.h"
#include "Render/LightRenderData.h"
#include "Render/SceneLightResources.h"
#include "Math/Matrix.h"
#include <algorithm>
#include <iterator>
#include <vector>

struct FRenderStats
{
	uint32 TotalPrimitives = 0;
	uint32 VisiblePrimitives = 0;
	uint32 DrawCalls = 0;
	uint64 Triangles = 0;
	uint32 LODCounts[4] = {};
	uint64 LODTriangles[4] = {};

	void Reset() { *this = FRenderStats(); }
};

// 뷰 하나의 데이터를 프레임 사이에도 보관한다. SceneRenderer는 현재 프레임 동안 이를 빌려 쓴다.
struct FSceneRenderData
{
	static constexpr int32 PointLightSlotIndex = 0;
	static constexpr int32 SpotLightSlotIndex = 1;
	static constexpr int32 DirectionalLightSlotIndex = 2;
	static constexpr int32 RectLightSlotIndex = 3;
	
	static constexpr uint32 ShadowMapSize = 4096;
	static constexpr uint32 DirectionalLightShadowMapSize = 4096;
	static constexpr uint32 MaxCascadeShadowCount = 4;
	static constexpr float MaxCascadeDistance = 1000.f;
	static constexpr uint32 PointLightShadowMapSize = 512;
	static constexpr uint32 SpotLightShadowMapSize = 4096;

	struct alignas(64) FGatherChunk
	{
		TArray<FRenderPacket> Packets;              // 스태틱 묶음에 못 들어가는 것 (반투명 섹션, Renderer 없는 호출)
		TArray<uint32> SlowPathIndices;
		std::vector<FStaticDrawGroup> Groups;       // 불투명 스태틱 메시: (머티리얼, 메시, LOD)별 묶음. 항목만 매 프레임 비운다
		uint32 LastGroup = 0;                       // 바로 전 물체가 들어간 묶음 (연속한 물체는 대개 같은 묶음)
		uint32 StaticDrawCount = 0;
		uint32 LODCounts[4] = {};
		uint64 LODTriangles[4] = {};
	};

	FRenderQueue RenderQueue;
	FRenderStats RenderStats;
	TArray<FPrimitiveSceneProxy*> VisibleProxies;
	TArray<FLODSelectionInput> LODInputs;
	TArray<uint8> SelectedLODs;
	TArray<FGatherChunk> GatherChunks;

	TArray<FPerObjectConstants> ObjectConstants;
	FSceneLightResources ViewLightResources;

	FAmbientLightRenderData AmbientLight{};
	FSkyLightRenderData SkyLight{};
	bool bHasSkyLight = false;

	TArray<FLightRenderParameters> VisibleLights; // 모든 종류의 광원. 업로드할 때 종류별 구간으로 정렬한다
	uint32 LightParameterOffsets[4]; // 라이트 리스트 내 구간의 offset
	uint32 LightParameterCounts[4]; // 라이트 리스트 내 구간의 count

	TArray<FLightShadowRenderData> LightShadows;
	uint32 LightShadowOffsets[4]; // 라이트 그림자 리스트 내 구간의 offset

	float CascadeShadowDistances[MaxCascadeShadowCount]{};
	uint32 CascadeShadowCount = 0;
	// CPU-only preview state for this view. Never uploaded with the light parameters.
	const ULightComponent* PreviewLight = nullptr;
	int32 PreviewLightIndex = INDEX_NONE;
	FLightShadowRange PreviewShadowRange{};

	bool GetShadowPreview(const ULightComponent* Light, FTexture2DArray*& OutAtlas,  // Shadow range를 바탕으로 lightshadow 인덱스에서 선택한 조명이 몇번째 index에서 viewcount가 몇개인지 알 수 있음
		TArray<FLightShadowRenderData>& OutShadows) const
	{
		OutAtlas = nullptr;
		OutShadows.Reset();
		const uint32 Start = PreviewShadowRange.StartIndex;
		const uint32 Count = PreviewShadowRange.ViewCount;
		if (!Light || Light != PreviewLight || !Count || Start > uint32(LightShadows.Num()) ||
			Count > uint32(LightShadows.Num()) - Start)
			return false;
		FTexture2DArray* Atlas = ViewLightResources.GetShadowMapAtlas();
		if (!Atlas || !Atlas->GetSRV()) return false;
		for (uint32 i = 0; i < Count; ++i)
		{
			const auto& Shadow = LightShadows[Start + i];
			if (Shadow.AtlasIndex >= Atlas->GetArraySize() || Shadow.AtlasRect.Width <= 0 || Shadow.AtlasRect.Height <= 0)
			{
				OutShadows.Reset();
				return false;
			}
			OutShadows.Add(Shadow);
		}
		OutAtlas = Atlas;
		return true;
	}

	void ResetScene()
	{
		ResetFrame();
		for (FGatherChunk& Chunk : GatherChunks)
		{
			Chunk.Groups.clear();
			Chunk.LastGroup = 0;
		}
	}

	void ResetFrame()
	{
		RenderQueue.Reset();
		RenderStats.Reset();
		VisibleProxies.Reset();
		LODInputs.Reset();
		SelectedLODs.Reset();
		ObjectConstants.Reset();
		AmbientLight = {};
		SkyLight = {};
		bHasSkyLight = false;
		VisibleLights.Reset();
		LightShadows.Reset();
		PreviewLightIndex = INDEX_NONE;
		PreviewShadowRange = {};
		CascadeShadowCount = 0;
		std::fill(std::begin(CascadeShadowDistances), std::end(CascadeShadowDistances), 0.0f);
		std::memset(LightParameterOffsets, 0, sizeof(LightParameterOffsets));
		std::memset(LightParameterCounts, 0, sizeof(LightParameterCounts));

		for (FGatherChunk& Chunk : GatherChunks)
		{
			Chunk.Packets.Reset();
			Chunk.SlowPathIndices.Reset();
			Chunk.StaticDrawCount = 0;
			std::fill(std::begin(Chunk.LODCounts), std::end(Chunk.LODCounts), 0u);
			std::fill(std::begin(Chunk.LODTriangles), std::end(Chunk.LODTriangles), 0ull);
			for (FStaticDrawGroup& Group : Chunk.Groups) Group.Items.clear();
		}
	}

	void Commit(const FFrustumCorners& CameraFrustumCorners, float CameraNear, float CameraFar, uint32 ExpectCascadeCount, bool& bLightUpaloadSucceeded)
	{
		RenderQueue.Sort();

		uint32 AtlasNeedCount = 1;
		if (VisibleLights.Num())
		{
			constexpr uint32 TypeCount = 4;
			constexpr uint32 TypeToSlot[TypeCount] = {
				FSceneRenderData::DirectionalLightSlotIndex,
				FSceneRenderData::PointLightSlotIndex,
				FSceneRenderData::SpotLightSlotIndex,
				FSceneRenderData::RectLightSlotIndex,
			};

			// Sort indices so the selected light's identity survives sorting, including identical lights.
			TArray<int32> LightOrder;
			LightOrder.SetNum(VisibleLights.Num());
			for (int32 i = 0; i < VisibleLights.Num(); ++i) LightOrder[i] = i;
			std::sort(LightOrder.begin(), LightOrder.end(), [&](int32 A, int32 B) {
				const uint32 ASlot = TypeToSlot[VisibleLights[A].Type];
				const uint32 BSlot = TypeToSlot[VisibleLights[B].Type];
				return ASlot < BSlot;
			});
			TArray<FLightRenderParameters> SortedLights;
			int32 SortedPreviewIndex = INDEX_NONE;
			for (int32 Index : LightOrder)
			{
				if (Index == PreviewLightIndex) SortedPreviewIndex = SortedLights.Num();
				SortedLights.Add(VisibleLights[Index]);
			}
			VisibleLights = std::move(SortedLights);
			PreviewLightIndex = SortedPreviewIndex;

			// Light 배열 정리 및 구간 별 Offset, Count 기록
			int32 Offset = 0;
			int32 Count = 1;
			for (int32 i = 1; i < VisibleLights.Num(); i++)
			{
				const FLightRenderParameters& LightParams = VisibleLights[i];
				const FLightRenderParameters& PrevLightParams = VisibleLights[i - 1];

				if (LightParams.Type != PrevLightParams.Type)
				{
					int32 Slot = TypeToSlot[PrevLightParams.Type];
					LightParameterOffsets[Slot] = Offset;
					LightParameterCounts[Slot] = Count;

					Offset += Count;
					Count = 1;
				}
				else
				{
					Count++;
				}
			}

			const FLightRenderParameters& LightParams = VisibleLights.Last();
			int32 Slot = TypeToSlot[LightParams.Type];
			LightParameterOffsets[Slot] = Offset;
			LightParameterCounts[Slot] = Count;

			// Light의 View, Projection 아틀라스 영역의 데이터를 저장한다.
			struct FAreaInfo
			{
				uint32 Size;
				uint32 Index;
			};

			TArray<FAreaInfo> AreaInfos;

			uint32 ShadowOffset = 0;

			LightShadowOffsets[FSceneRenderData::PointLightSlotIndex] = ShadowOffset;
			for (int32 i = 0; i < GetPointLightCount(); i++)
			{
				int32 LightIndex = GetPointLightOffset() + i;
				if (LightIndex == PreviewLightIndex) PreviewShadowRange = { uint32(LightShadows.Num()), 6 };

				const FLightRenderParameters& PointLight = VisibleLights[LightIndex];

				FMatrix PerspectiveMatrix = Perspective(FMath::DegreesToRadians(90.f), 1.0, 0.1f, PointLight.Radius);
				FMatrix Views[] = {
					LookAt(PointLight.Position, PointLight.Position + ForwardVector, UpVector),
					LookAt(PointLight.Position, PointLight.Position + BackwardVector, UpVector),
					LookAt(PointLight.Position, PointLight.Position + UpVector, BackwardVector),
					LookAt(PointLight.Position, PointLight.Position + DownVector, ForwardVector),
					LookAt(PointLight.Position, PointLight.Position + RightVector, UpVector),
					LookAt(PointLight.Position, PointLight.Position + LeftVector, UpVector),
				};

				for (int32 j = 0; j < 6; j++)
				{
					FLightShadowRenderData Shadow{};
					Shadow.ViewProjection = Views[j] * PerspectiveMatrix;
					Shadow.Near = 0.1f;
					Shadow.Far = PointLight.Radius;
					
					uint32 Index = LightShadows.Add(Shadow);

					AreaInfos.Add({ PointLightShadowMapSize, Index });

					ShadowOffset++;
				}
			}

			LightShadowOffsets[FSceneRenderData::SpotLightSlotIndex] = ShadowOffset;
			for (int32 i = 0; i < GetSpotLightCount(); i++)
			{
				int32 LightIndex = GetSpotLightOffset() + i;
				if (LightIndex == PreviewLightIndex) PreviewShadowRange = { uint32(LightShadows.Num()), 1 };

				const FLightRenderParameters& SpotLight = VisibleLights[LightIndex];

				FMatrix ViewMatrix = LookAt(SpotLight.Position, SpotLight.Position + SpotLight.Direction, UpVector);
				FMatrix ProjectionMatrix = Perspective(std::acos(SpotLight.OuterConeCos) * 2.0f, 1.0, 0.1f, SpotLight.Radius);

				FLightShadowRenderData Shadow{};
				Shadow.ViewProjection = ViewMatrix * ProjectionMatrix;
				Shadow.Near = 0.1f;
				Shadow.Far = SpotLight.Radius;
				
				uint32 Index = LightShadows.Add(Shadow);

				AreaInfos.Add({ SpotLightShadowMapSize, Index });

				ShadowOffset++;
			}

			float CascadeShadowFar = FMath::Min(CameraFar, MaxCascadeDistance);
			float LogRatio = 0.8;

			CascadeShadowCount = FMath::Clamp(ExpectCascadeCount, 1u, MaxCascadeShadowCount);

			LightShadowOffsets[FSceneRenderData::DirectionalLightSlotIndex] = ShadowOffset;
			for (int32 i = 0; i < GetDirectionalLightCount(); i++)
			{
				int32 LightIndex = GetDirectionalLightOffset() + i;
				if (LightIndex == PreviewLightIndex) PreviewShadowRange = { uint32(LightShadows.Num()), CascadeShadowCount };

				const FLightRenderParameters& DirectionalLight = VisibleLights[LightIndex];
				FMatrix ViewMatrix = LookAt(FVector(0.f), DirectionalLight.Direction, UpVector);

				FVector ToTL = CameraFrustumCorners.FTL() - CameraFrustumCorners.NTL();
				FVector ToTR = CameraFrustumCorners.FTR() - CameraFrustumCorners.NTR();
				FVector ToBL = CameraFrustumCorners.FBL() - CameraFrustumCorners.NBL();
				FVector ToBR = CameraFrustumCorners.FBR() - CameraFrustumCorners.NBR();

				FVector PrevTL = CameraFrustumCorners.NTL();
				FVector PrevTR = CameraFrustumCorners.NTR();
				FVector PrevBL = CameraFrustumCorners.NBL();
				FVector PrevBR = CameraFrustumCorners.NBR();

				uint32 CascadeShadowMapSize = DirectionalLightShadowMapSize;
				for (int32 j = 0; j < CascadeShadowCount; j++)
				{
					float Linear = CameraNear + (CascadeShadowFar - CameraNear) * ((j + 1) / (float)CascadeShadowCount);
					float Log = CameraNear * std::pow(CascadeShadowFar / CameraNear, (j + 1) / (float)CascadeShadowCount);
					float Dist = (1.f - LogRatio) * Linear + LogRatio * Log;

					float T = (Dist - CameraNear) / (CameraFar - CameraNear);

					FVector LT = CameraFrustumCorners.NTL() + ToTL * T;
					FVector RT = CameraFrustumCorners.NTR() + ToTR * T;
					FVector LB = CameraFrustumCorners.NBL() + ToBL * T;
					FVector RB = CameraFrustumCorners.NBR() + ToBR * T;

					FVector* WorldCorners[] = {
						&PrevTL,
						&PrevTR,
						&PrevBL,
						&PrevBR,
						&LT,
						&RT,
						&LB,
						&RB
					};

					FVector MinInCascadeLight = FVector(FLT_MAX);
					FVector MaxInCascadeLight = FVector(-FLT_MAX);
					for (int32 k = 0; k < 8; k++)
					{
						FVector4 InCascadeLightSpace = FVector4(*WorldCorners[k], 1.0f) * ViewMatrix;
						MinInCascadeLight = FMath::Min(MinInCascadeLight, FVector(InCascadeLightSpace.X, InCascadeLightSpace.Y, InCascadeLightSpace.Z));
						MaxInCascadeLight = FMath::Max(MaxInCascadeLight, FVector(InCascadeLightSpace.X, InCascadeLightSpace.Y, InCascadeLightSpace.Z));
					}

					FMatrix CascadeProjectionMatrix = Ortho(MinInCascadeLight.Y, MaxInCascadeLight.Y, MaxInCascadeLight.Z, MinInCascadeLight.Z, MinInCascadeLight.X, MaxInCascadeLight.X);

					FLightShadowRenderData Shadow{};
					Shadow.ViewProjection = ViewMatrix * CascadeProjectionMatrix;
					Shadow.Near = MinInCascadeLight.X;
					Shadow.Far = MaxInCascadeLight.X;

					uint32 Index = LightShadows.Add(Shadow);

					AreaInfos.Add({ CascadeShadowMapSize, Index});

					ShadowOffset++;

					PrevTL = LT;
					PrevTR = RT;
					PrevBL = LB;
					PrevBR = RB;

					CascadeShadowDistances[j] = Dist;
					CascadeShadowMapSize /= 2;
				}
			}

			std::sort(AreaInfos.GetData(), AreaInfos.GetData() + AreaInfos.Num(), [](const FAreaInfo& A, const FAreaInfo& B) { return A.Size > B.Size; });

			// TODO: 패킹 알고리즘 필요
			uint32 AtlasCursorX = 0;
			uint32 AtlasCursorY = 0;
			uint32 MaxHeightInLine = 0;
			for (int32 i = 0; i < AreaInfos.Num(); i++)
			{
				FAreaInfo& AreaInfo = AreaInfos[i];

				if (AtlasCursorX + AreaInfo.Size > ShadowMapSize)
				{
					AtlasCursorY += MaxHeightInLine;
					AtlasCursorX = 0;
					MaxHeightInLine = 0;

					if (AtlasCursorY + AreaInfo.Size > ShadowMapSize)
					{
						AtlasNeedCount++;
						AtlasCursorY = 0;
						MaxHeightInLine = 0;
					}
				}
				if (AtlasCursorY + AreaInfo.Size > ShadowMapSize)
				{
					AtlasNeedCount++;
					AtlasCursorX = 0;
					AtlasCursorY = 0;
					MaxHeightInLine = 0;
				}

				FLightShadowRenderData& RenderData = LightShadows[AreaInfo.Index];

				RenderData.AtlasIndex = AtlasNeedCount - 1;
				RenderData.AtlasRect.X = AtlasCursorX;
				RenderData.AtlasRect.Y = AtlasCursorY;
				RenderData.AtlasRect.Width = AreaInfo.Size;
				RenderData.AtlasRect.Height = AreaInfo.Size;

				AtlasCursorX += AreaInfo.Size;

				MaxHeightInLine = std::max(MaxHeightInLine, AreaInfo.Size);
			}
		}

		// NOTE: 라이트 데이터를 GPU에 올림.
		bLightUpaloadSucceeded = ViewLightResources.Init();
		if (bLightUpaloadSucceeded)
		{
			bLightUpaloadSucceeded &= ViewLightResources.UploadViewLights(VisibleLights);
			ViewLightResources.CreateShadowAtlasIfNeed(ShadowMapSize, ShadowMapSize, AtlasNeedCount);
			bLightUpaloadSucceeded &= ViewLightResources.UploadLightShadows(LightShadows);
		}
		if (!bLightUpaloadSucceeded) PreviewShadowRange = {};
	}

	inline bool HasDirectionalLight() { return LightParameterCounts[DirectionalLightSlotIndex]; }
	inline uint32 GetDirectionalLightOffset() { return LightParameterOffsets[DirectionalLightSlotIndex]; }
	inline uint32 GetDirectionalLightCount() { return LightParameterCounts[DirectionalLightSlotIndex]; }
	inline uint32 GetDirectionalLightInfoOffset() { return LightShadowOffsets[DirectionalLightSlotIndex]; }
	
	inline bool HasPointLights() { return LightParameterCounts[PointLightSlotIndex]; }
	inline uint32 GetPointLightOffset() { return LightParameterOffsets[PointLightSlotIndex]; }
	inline uint32 GetPointLightCount() { return LightParameterCounts[PointLightSlotIndex]; }
	inline uint32 GetPointLightShadowOffset() { return LightShadowOffsets[PointLightSlotIndex]; }

	inline bool HasSpotLights() { return LightParameterCounts[SpotLightSlotIndex]; }
	inline uint32 GetSpotLightOffset() { return LightParameterOffsets[SpotLightSlotIndex]; }
	inline uint32 GetSpotLightCount() { return LightParameterCounts[SpotLightSlotIndex]; }
	inline uint32 GetSpotLightInfoOffset() { return LightShadowOffsets[SpotLightSlotIndex]; }

	inline bool HasRectLights() { return LightParameterCounts[RectLightSlotIndex]; }
	inline bool HasLights() { return HasDirectionalLight() || HasPointLights() || HasSpotLights() || HasRectLights(); }
};
