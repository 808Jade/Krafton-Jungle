#pragma once

#include "Render/RenderPacket.h"
#include "Render/ObjectConstants.h"
#include "Engine/PrimitiveSceneProxy.h"
#include "Engine/LightSceneProxy.h"
#include "Render/LightRenderData.h"
#include "Render/SceneLightResources.h"
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
	
	static constexpr uint32 DirectionalLightShadowMapSize = 1024;
	static constexpr uint32 PointLightShadowMapSize = 1024;
	static constexpr uint32 SpotLightShadowMapSize = 512;

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

	TArray<FRect> SceneLightShadowMapAreas; // 셰도우 아틀라스 내 라이트 텍스쳐 크기
	uint32 LightShadowMapOffsets[4];

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
		SceneLightShadowMapAreas.Reset();
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

	void Commit(bool& bLightUpaloadSucceeded)
	{
		RenderQueue.Sort();

		uint32 SuitableWidth = 0;
		uint32 SuitableHeight = 0;
		if (VisibleLights.Num())
		{
			constexpr uint32 TypeCount = 4;
			constexpr uint32 TypeToSlot[TypeCount] = {
				FSceneRenderData::DirectionalLightSlotIndex,
				FSceneRenderData::PointLightSlotIndex,
				FSceneRenderData::SpotLightSlotIndex,
				FSceneRenderData::RectLightSlotIndex,
			};

			std::sort(VisibleLights.GetData(), VisibleLights.GetData() + VisibleLights.Num(), [&TypeCount, &TypeToSlot](const FLightRenderParameters& A, const FLightRenderParameters& B) {
				const uint32 ASlot = TypeToSlot[A.Type];
				const uint32 BSlot = TypeToSlot[B.Type];
				return ASlot < BSlot;
			});

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

			// Light 아틀라스 영역 지정
			struct FAreaInfo
			{
				uint32 Size;
				uint32 RenctIndex;
			};

			TArray<FAreaInfo> AreaInfos;

			uint32 TotalAreaSize = 0;
			uint32 MaxAreaSize = 0;
			uint32 ShadowMapOffset = 0;

			LightShadowMapOffsets[FSceneRenderData::PointLightSlotIndex] = ShadowMapOffset;
			for (int32 i = 0; i < GetPointLightCount(); i++)
			{
				for (int32 j = 0; j < 6; j++)
				{
					uint32 RectIndex = SceneLightShadowMapAreas.Emplace();
					AreaInfos.Add({ PointLightShadowMapSize, RectIndex });
					TotalAreaSize += PointLightShadowMapSize * PointLightShadowMapSize;
					MaxAreaSize = FMath::Max(MaxAreaSize, PointLightShadowMapSize);
					ShadowMapOffset++;
				}
			}

			LightShadowMapOffsets[FSceneRenderData::SpotLightSlotIndex] = ShadowMapOffset;
			for (int32 i = 0; i < GetSpotLightCount(); i++)
			{
				uint32 RectIndex = SceneLightShadowMapAreas.Emplace();
				AreaInfos.Add({ SpotLightShadowMapSize, RectIndex });
				TotalAreaSize += SpotLightShadowMapSize * SpotLightShadowMapSize;
				MaxAreaSize = FMath::Max(MaxAreaSize, SpotLightShadowMapSize);
				ShadowMapOffset++;
			}

			LightShadowMapOffsets[FSceneRenderData::DirectionalLightSlotIndex] = ShadowMapOffset;
			for (int32 i = 0; i < GetDirectionalLightCount(); i++)
			{
				uint32 RectIndex = SceneLightShadowMapAreas.Emplace();
				AreaInfos.Add({ DirectionalLightShadowMapSize, RectIndex });
				TotalAreaSize += DirectionalLightShadowMapSize * DirectionalLightShadowMapSize;
				MaxAreaSize = FMath::Max(MaxAreaSize, DirectionalLightShadowMapSize);
				ShadowMapOffset++;
			}

			std::sort(AreaInfos.GetData(), AreaInfos.GetData() + AreaInfos.Num(), [](const FAreaInfo& A, const FAreaInfo& B) { return A.Size > B.Size; });

			SuitableWidth = std::ceil(sqrt(TotalAreaSize));
			SuitableWidth = FMath::Max(SuitableWidth, MaxAreaSize);

			uint32 AtlasCursorX = 0;
			uint32 AtlasCursorY = 0;
			uint32 MaxHeightInLine = 0;
			for (int32 i = 0; i < AreaInfos.Num(); i++)
			{
				FAreaInfo& AreaInfo = AreaInfos[i];

				if (AtlasCursorX + AreaInfo.Size > SuitableWidth)
				{
					AtlasCursorY += MaxHeightInLine;
					AtlasCursorX = 0;
					MaxHeightInLine = 0;
				}

				FRect& Area = SceneLightShadowMapAreas[AreaInfo.RenctIndex];
				Area.X = AtlasCursorX;
				Area.Y = AtlasCursorY;
				Area.Width = AreaInfo.Size;
				Area.Height = AreaInfo.Size;

				AtlasCursorX += AreaInfo.Size;

				MaxHeightInLine = std::max(MaxHeightInLine, AreaInfo.Size);
			}

			SuitableHeight = AtlasCursorY + MaxHeightInLine;
		}

		// NOTE: 라이트 데이터를 GPU에 올림.
		bLightUpaloadSucceeded = ViewLightResources.Init();
		if (bLightUpaloadSucceeded)
		{
			bLightUpaloadSucceeded = ViewLightResources.UploadViewLights(VisibleLights);
			if (SuitableWidth != 0 && SuitableHeight != 0)
			{
				ViewLightResources.CreateShadowAtlasIfNeed(SuitableWidth, SuitableHeight);
			}
		}
	}

	inline bool HasDirectionalLight() { return LightParameterCounts[DirectionalLightSlotIndex]; }
	inline uint32 GetDirectionalLightOffset() { return LightParameterOffsets[DirectionalLightSlotIndex]; }
	inline uint32 GetDirectionalLightCount() { return LightParameterCounts[DirectionalLightSlotIndex]; }
	inline uint32 GetDirectionalLightShadowOffset() { return LightShadowMapOffsets[DirectionalLightSlotIndex]; }
	
	inline bool HasPointLights() { return LightParameterCounts[PointLightSlotIndex]; }
	inline uint32 GetPointLightOffset() { return LightParameterOffsets[PointLightSlotIndex]; }
	inline uint32 GetPointLightCount() { return LightParameterCounts[PointLightSlotIndex]; }
	inline uint32 GetPointLightShadowOffset() { return LightShadowMapOffsets[PointLightSlotIndex]; }

	inline bool HasSpotLights() { return LightParameterCounts[SpotLightSlotIndex]; }
	inline uint32 GetSpotLightOffset() { return LightParameterOffsets[SpotLightSlotIndex]; }
	inline uint32 GetSpotLightCount() { return LightParameterCounts[SpotLightSlotIndex]; }
	inline uint32 GetSpotLightShadowOffset() { return LightShadowMapOffsets[SpotLightSlotIndex]; }

	inline bool HasRectLights() { return LightParameterCounts[RectLightSlotIndex]; }
	inline bool HasLights() { return HasDirectionalLight() || HasPointLights() || HasSpotLights() || HasRectLights(); }
};
