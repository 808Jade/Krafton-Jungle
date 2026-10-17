#pragma once

#include "Render/RenderPacket.h"
#include "Render/ObjectConstants.h"
#include "Engine/PrimitiveSceneProxy.h"
#include "Engine/LightSceneProxy.h"
#include "Render/LightRenderData.h"
#include "Render/SceneLightResources.h"
#include "Render/SceneView.h"
#include "Math/Matrix.h"
#include <algorithm>
#include <cmath>
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

	TArray<FLightShadowRenderData> LightShadows;
	uint32 LightShadowOffsets[4];
	uint32 DirectionalCascadeCount = 1;
	FVector4 DirectionalCascadeFars[2];

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
		DirectionalCascadeCount = 1;
		DirectionalCascadeFars[0] = FVector4(0.0f);
		DirectionalCascadeFars[1] = FVector4(0.0f);
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

	void Commit(bool& bLightUpaloadSucceeded, const FSceneView& CameraView)
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

			// Light의 View, Projection 아틀라스 영역의 데이터를 저장한다.
			struct FAreaInfo
			{
				uint32 Size;
				uint32 Index;
			};

			TArray<FAreaInfo> AreaInfos;

			uint32 TotalAreaSize = 0;
			uint32 MaxAreaSize = 0;
			uint32 ShadowMapOffset = 0;

			LightShadowOffsets[FSceneRenderData::PointLightSlotIndex] = ShadowMapOffset;
			for (int32 i = 0; i < GetPointLightCount(); i++)
			{
				int32 LightIndex = GetPointLightOffset() + i;

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
					
					uint32 Index = LightShadows.Add(Shadow);

					AreaInfos.Add({ PointLightShadowMapSize, Index });

					TotalAreaSize += PointLightShadowMapSize * PointLightShadowMapSize;
					MaxAreaSize = FMath::Max(MaxAreaSize, PointLightShadowMapSize);
					ShadowMapOffset++;
				}
			}

			LightShadowOffsets[FSceneRenderData::SpotLightSlotIndex] = ShadowMapOffset;
			for (int32 i = 0; i < GetSpotLightCount(); i++)
			{
				int32 LightIndex = GetSpotLightOffset() + i;

				const FLightRenderParameters& SpotLight = VisibleLights[LightIndex];

				FMatrix ViewMatrix = LookAt(SpotLight.Position, SpotLight.Position + SpotLight.Direction, UpVector);
				FMatrix ProjectionMatrix = Perspective(std::acos(SpotLight.OuterConeCos) * 2.0f, 1.0, 0.1f, SpotLight.Radius);

				FLightShadowRenderData Shadow{};
				Shadow.ViewProjection = ViewMatrix * ProjectionMatrix;
				
				uint32 Index = LightShadows.Add(Shadow);

				AreaInfos.Add({ SpotLightShadowMapSize, Index });

				TotalAreaSize += SpotLightShadowMapSize * SpotLightShadowMapSize;
				MaxAreaSize = FMath::Max(MaxAreaSize, SpotLightShadowMapSize);
				ShadowMapOffset++;
			}

			LightShadowOffsets[FSceneRenderData::DirectionalLightSlotIndex] = ShadowMapOffset;
			constexpr float TargetCascadeDepthRatio = 5.0f;
			constexpr uint32 MaxDirectionalCascades = 8;
			constexpr float CasterDepthPadding = 20.0f;
			const float CameraNear = CameraView.NearClip;
			const float CameraFar = CameraView.FarClip;
			const bool bValidCameraRange = std::isfinite(CameraNear) && std::isfinite(CameraFar)
				&& CameraNear > 0.0f && CameraFar > CameraNear;
			if (bValidCameraRange)
			{
				const double DesiredCount = std::ceil(
					std::log(static_cast<double>(CameraFar) / CameraNear) / std::log(TargetCascadeDepthRatio));
				DirectionalCascadeCount = static_cast<uint32>(std::clamp(
					DesiredCount, 1.0, static_cast<double>(MaxDirectionalCascades)));
				for (uint32 CascadeIndex = 0; CascadeIndex < DirectionalCascadeCount; ++CascadeIndex)
				{
					DirectionalCascadeFars[CascadeIndex / 4].Component(CascadeIndex % 4) = CameraNear * std::pow(CameraFar / CameraNear, static_cast<float>(CascadeIndex + 1) / DirectionalCascadeCount);
				}
			}

			FVector CameraCorners[2][4]{}; // [near/far][NDC의 네 모서리]
			if (bValidCameraRange && GetDirectionalLightCount() > 0)
			{
				const FMatrix InverseVP = CameraView.ViewProjectionMatrix.Inverse();
				struct FNdcCorner { float X, Y; };
				constexpr FNdcCorner NdcCorners[4] = {
					{ -1.0f, -1.0f }, // 왼쪽 아래
					{  1.0f, -1.0f }, // 오른쪽 아래
					{ -1.0f,  1.0f }, // 왼쪽 위
					{  1.0f,  1.0f }  // 오른쪽 위
				};
				for (int32 CornerIndex = 0; CornerIndex < 4; ++CornerIndex)
				{
					const FNdcCorner& NdcCorner = NdcCorners[CornerIndex];
					for (int32 PlaneIndex = 0; PlaneIndex < 2; ++PlaneIndex)
					{
						const FVector4 Corner = FVector4(NdcCorner.X, NdcCorner.Y, static_cast<float>(PlaneIndex), 1.0f) * InverseVP;
						CameraCorners[PlaneIndex][CornerIndex] = FVector(Corner.X / Corner.W, Corner.Y / Corner.W, Corner.Z / Corner.W);
					}
				}
			}
			for (int32 i = 0; i < GetDirectionalLightCount(); i++)
			{
				int32 LightIndex = GetDirectionalLightOffset() + i;

				const FLightRenderParameters& DirectionalLight = VisibleLights[LightIndex];
				const FVector LightDirection = DirectionalLight.Direction.GetSafeNormal();
				const FVector LightUp = std::abs(LightDirection.Z) > 0.99f
					? FVector(0, 1, 0) : FVector(0, 0, 1);
				const FMatrix LightOrientation = LookAt(FVector(0, 0, 0), LightDirection, LightUp);
				const FMatrix InverseLightOrientation = LightOrientation.Inverse();
				for (uint32 CascadeIndex = 0; CascadeIndex < DirectionalCascadeCount; ++CascadeIndex)
				{
					FMatrix ViewMatrix;
					FMatrix ProjectionMatrix;
					if (bValidCameraRange)
					{
						const float NearDistance = CascadeIndex == 0 ? CameraNear
							: DirectionalCascadeFars[(CascadeIndex - 1) / 4].Component((CascadeIndex - 1) % 4);
						const float FarDistance = DirectionalCascadeFars[CascadeIndex / 4].Component(CascadeIndex % 4);
						const float NearFraction = (NearDistance - CameraNear) / (CameraFar - CameraNear);
						const float FarFraction = (FarDistance - CameraNear) / (CameraFar - CameraNear);
						FVector Min, Max;
						for (int32 CornerIndex = 0; CornerIndex < 4; ++CornerIndex)
						{
							const FVector Ray = CameraCorners[1][CornerIndex] - CameraCorners[0][CornerIndex];
							for (int32 PlaneIndex = 0; PlaneIndex < 2; ++PlaneIndex)
							{
								const float Fraction = PlaneIndex ? FarFraction : NearFraction;
								const FVector WorldCorner = CameraCorners[0][CornerIndex] + Ray * Fraction;
								const FVector4 LightCorner = FVector4(WorldCorner, 1.0f) * LightOrientation;
								const FVector Point(LightCorner.X, LightCorner.Y, LightCorner.Z);
								if (CornerIndex == 0 && PlaneIndex == 0) Min = Max = Point;
								else
								{
									for (int32 Axis = 0; Axis < 3; ++Axis)
									{
										Min.Component(Axis) = std::min(Min.Component(Axis), Point.Component(Axis));
										Max.Component(Axis) = std::max(Max.Component(Axis), Point.Component(Axis));
									}
								}
							}
						}
						const float Width = std::max(Max.Y - Min.Y, 0.01f);
						const float Height = std::max(Max.Z - Min.Z, 0.01f);
						const float Depth = std::max(Max.X - Min.X + CasterDepthPadding, 0.01f);
						const FVector4 Eye = FVector4(Min.X - CasterDepthPadding - 0.1f,
							(Min.Y + Max.Y) * 0.5f, (Min.Z + Max.Z) * 0.5f, 1.0f) * InverseLightOrientation;
						const FVector EyePosition(Eye.X, Eye.Y, Eye.Z);
						ViewMatrix = LookAt(EyePosition, EyePosition + LightDirection, LightUp);
						ProjectionMatrix = Ortho(Width, Width / Height, 0.1f, Depth + 0.1f);
					}
					else
					{
						ViewMatrix = LookAt(DirectionalLight.Position,
							DirectionalLight.Position + LightDirection, LightUp);
						ProjectionMatrix = Ortho(20, 1.0f, 0.1f, 1000.f);
					}

					FLightShadowRenderData Shadow{};
					Shadow.ViewProjection = ViewMatrix * ProjectionMatrix;
					uint32 Index = LightShadows.Add(Shadow);
					AreaInfos.Add({ DirectionalLightShadowMapSize, Index });
					TotalAreaSize += DirectionalLightShadowMapSize * DirectionalLightShadowMapSize;
					MaxAreaSize = FMath::Max(MaxAreaSize, DirectionalLightShadowMapSize);
					ShadowMapOffset++;
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
