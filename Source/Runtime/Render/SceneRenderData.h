#pragma once

#include "Render/RenderPacket.h"
#include "Render/ObjectConstants.h"
#include "Engine/PrimitiveSceneProxy.h"
#include "Engine/LightSceneProxy.h"
#include "Render/LightRenderData.h"
#include "Render/SceneLightResources.h"
#include "Math/Matrix.h"
#include <algorithm>
#include <cmath>
#include <iterator>
#include <vector>
#include "Math/QuadTree.h"

struct FRenderStats
{
	uint32 TotalPrimitives = 0;
	uint32 VisiblePrimitives = 0;
	uint32 DrawCalls = 0;
	uint64 Triangles = 0;
	uint32 LODCounts[4] = {};
	uint64 LODTriangles[4] = {};

	uint32 DirectionalCount = 0;
	uint32 PointCount = 0;
	uint32 SpotCount = 0;
	uint32 RectCount = 0;
	float AtlasCapacity = 0.0f;
	float ShadowTiles = 0.0f;
	uint32 ShadowViewsRedrawn = 0; // 이번 프레임에 다시 그린 그림자 뷰(큐브 면, cascade) 수
	uint32 ShadowViewsCached = 0;  // 캐시를 재사용해 건너뛴 그림자 뷰 수

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
	static constexpr uint32 MaxCascadeShadowCount = 4;
	static constexpr float MaxCascadeDistance = 1000.f;

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
	TArray<FPrimitiveSceneProxy*> ShadowCasterProxies;
	TArray<FLODSelectionInput> LODInputs;
	TArray<uint8> SelectedLODs;
	TArray<FGatherChunk> GatherChunks;

	TArray<FPerObjectConstants> ObjectConstants;
	FSceneLightResources ViewLightResources;

	FAmbientLightRenderData AmbientLight{};
	FSkyLightRenderData SkyLight{};
	bool bHasSkyLight = false;

	TArray<FLightRenderParameters> VisibleLights; // 모든 종류의 광원. 업로드할 때 종류별 구간으로 정렬한다
	TArray<FLightShadowSettings> VisibleLightShadowSettings; // VisibleLights와 같은 순서의 그림자 설정
	uint32 LightParameterOffsets[4]; // 라이트 리스트 내 구간의 offset
	uint32 LightParameterCounts[4]; // 라이트 리스트 내 구간의 count

	uint32 DirectionalLightShadowMapSize = 4096;
	uint32 PointLightShadowMapSize = 512;
	uint32 SpotLightShadowMapSize = 512;

	TArray<FLightShadowRenderData> LightShadows;
	uint32 LightShadowOffsets[4]; // 라이트 그림자 리스트 내 구간의 offset

	// ---------- 그림자 캐시 (r.Shadow.CacheWholeSceneShadows) ----------
	// 그림자 뷰 하나(큐브 면, cascade)를 프레임 사이에 같은 것으로 보는 키. 순서까지 같으면 아틀라스 배치를 그대로 재사용한다.
	struct FShadowLayoutKey
	{
		const ULightComponent* Light = nullptr;
		uint32 Size = 0;
		bool operator==(const FShadowLayoutKey&) const = default;
	};
	// 이번 프레임 값. 모두 LightShadows와 같은 순서다.
	TArray<FShadowLayoutKey> ShadowLayout;
	TArray<FVector4> ShadowInfluences;  // 광원 영향 구 (xyz 중심, w 반지름). w < 0이면 씬 전체(방향광)
	TArray<uint8> ShadowNeedsRedraw;    // 1이면 이번 프레임에 이 타일을 지우고 다시 그린다
	bool bClearWholeShadowAtlas = true; // 배치를 새로 했으면 타일별로 지우지 않고 아틀라스 전체를 지운다
	// 이전 프레임 값. ResetFrame에서 지우지 않는다.
	TArray<FShadowLayoutKey> CachedShadowLayout;
	TArray<FLightShadowRenderData> CachedShadows;
	uint32 CachedAtlasNeedCount = 0;
	const FTexture2DArray* CachedShadowAtlas = nullptr;
	uint64 ShadowInvalidationVersion = 0; // FScene 무효화 기록을 어디까지 처리했는지
	bool bHasShadowInvalidationVersion = false;

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
		ShadowCasterProxies.Reset();
		LODInputs.Reset();
		SelectedLODs.Reset();
		ObjectConstants.Reset();
		AmbientLight = {};
		SkyLight = {};
		bHasSkyLight = false;
		VisibleLights.Reset();
		VisibleLightShadowSettings.Reset();
		LightShadows.Reset();
		ShadowLayout.Reset();
		ShadowInfluences.Reset();
		ShadowNeedsRedraw.Reset();
		bClearWholeShadowAtlas = true;
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

	// 아틀라스 QuadTree는 2의 거듭제곱 정사각형만 받으므로 배율 적용 후 가까운 2의 거듭제곱으로 맞춘다.
	static uint32 ScaleShadowMapSize(uint32 BaseSize, float Scale)
	{
		constexpr uint32 MinShadowMapSize = 32;
		const float Scaled = std::max(float(BaseSize) * Scale, 1.0f);
		const uint32 Rounded = 1u << uint32(std::lround(std::log2(Scaled)));
		return std::clamp(Rounded, MinShadowMapSize, ShadowMapSize);
	}

	static void ApplyShadowSettings(FLightShadowRenderData& Shadow, const FLightShadowSettings& Settings)
	{
		Shadow.Bias = Settings.Bias;
		Shadow.SlopeBias = Settings.SlopeBias;
		Shadow.Sharpen = Settings.Sharpen;
	}

	static bool IntersectsInfluence(const FVector4& Influence, const FAABB& Bounds)
	{
		if (Influence.W < 0.0f) return true; // 방향광은 씬 전체에 영향을 준다
		// 구 중심에서 AABB까지의 최단 거리로 판정한다.
		const float DX = std::max(std::abs(Influence.X - Bounds.Center.X) - Bounds.Extent.X, 0.0f);
		const float DY = std::max(std::abs(Influence.Y - Bounds.Center.Y) - Bounds.Extent.Y, 0.0f);
		const float DZ = std::max(std::abs(Influence.Z - Bounds.Center.Z) - Bounds.Extent.Z, 0.0f);
		return DX * DX + DY * DY + DZ * DZ <= Influence.W * Influence.W;
	}

	void AddShadowView(const FLightShadowRenderData& Shadow, const FLightShadowSettings& Settings, uint32 Size, const FVector4& Influence)
	{
		LightShadows.Add(Shadow);
		ShadowLayout.Add({ Settings.Light, Size });
		ShadowInfluences.Add(Influence);
	}

	// 이전 프레임 결과를 재사용할 수 있는 그림자 뷰는 ShadowNeedsRedraw를 0으로 둔다.
	void UpdateShadowCache(bool bCacheShadows, bool bReuseLayout, const FShadowInvalidationList& Invalidations)
	{
		const int32 ShadowCount = LightShadows.Num();
		ShadowNeedsRedraw.SetNum(ShadowCount);

		// 무효화 기록을 놓쳤거나(처음, 너무 오래 안 그림) 캐시가 꺼져 있으면 전부 다시 그린다.
		const bool bMissedInvalidations = !bHasShadowInvalidationVersion || ShadowInvalidationVersion < Invalidations.OldestVersion;
		const bool bRedrawAll = !bCacheShadows || !bReuseLayout || bMissedInvalidations;
		// 전부 다시 그릴 때는 타일마다 지우는 것보다 패스 시작 시 한 번에 Clear하는 편이 싸다.
		bClearWholeShadowAtlas = bRedrawAll;

		for (int32 i = 0; i < ShadowCount; ++i)
		{
			bool bRedraw = bRedrawAll;
			// 광원이 움직이거나 반지름·cascade 범위가 바뀌면 행렬이 달라진다.
			if (!bRedraw)
				bRedraw = std::memcmp(&LightShadows[i].ViewProjection, &CachedShadows[i].ViewProjection, sizeof(FMatrix)) != 0;
			// 마지막으로 처리한 이후에 영향 범위 안에서 Caster가 바뀌었는지 본다.
			if (!bRedraw && Invalidations.Records)
			{
				for (const FShadowInvalidation& Record : *Invalidations.Records)
				{
					if (Record.Version <= ShadowInvalidationVersion) continue;
					if (IntersectsInfluence(ShadowInfluences[i], Record.Bounds)) { bRedraw = true; break; }
				}
			}
			ShadowNeedsRedraw[i] = bRedraw ? 1 : 0;
			if (bRedraw) ++RenderStats.ShadowViewsRedrawn;
			else ++RenderStats.ShadowViewsCached;
		}

		ShadowInvalidationVersion = Invalidations.CurrentVersion;
		bHasShadowInvalidationVersion = true;
		CachedShadowLayout = ShadowLayout;
		CachedShadows = LightShadows;
	}

	void Commit(const FFrustumCorners& CameraFrustumCorners, float CameraNear, float CameraFar, uint32 ExpectCascadeCount,
		bool bCacheShadows, const FShadowInvalidationList& ShadowInvalidations, bool& bLightUpaloadSucceeded)
	{
		RenderQueue.Sort();

		uint32 AtlasNeedCount = 1;
		float UsedShadowTexels = 0.0f;
		bool bReuseLayout = false;
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
			// stable_sort: 같은 종류끼리의 순서가 프레임마다 같아야 그림자 캐시가 같은 배치를 재사용한다.
			std::stable_sort(LightOrder.begin(), LightOrder.end(), [&](int32 A, int32 B) {
				const uint32 ASlot = TypeToSlot[VisibleLights[A].Type];
				const uint32 BSlot = TypeToSlot[VisibleLights[B].Type];
				return ASlot < BSlot;
			});
			TArray<FLightRenderParameters> SortedLights;
			TArray<FLightShadowSettings> SortedShadowSettings;
			int32 SortedPreviewIndex = INDEX_NONE;
			for (int32 Index : LightOrder)
			{
				if (Index == PreviewLightIndex) SortedPreviewIndex = SortedLights.Num();
				SortedLights.Add(VisibleLights[Index]);
				SortedShadowSettings.Add(Index < VisibleLightShadowSettings.Num() ? VisibleLightShadowSettings[Index] : FLightShadowSettings{});
			}
			VisibleLights = std::move(SortedLights);
			VisibleLightShadowSettings = std::move(SortedShadowSettings);
			PreviewLightIndex = SortedPreviewIndex;

			// 그림자를 만든 광원만 아래에서 ShadowIndex를 채운다. 셰이더는 이 값으로 LightShadows를 찾는다.
			for (FLightRenderParameters& Light : VisibleLights) Light.ShadowIndex = FLightRenderParameters::InvalidShadowIndex;

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
				const FLightShadowSettings& ShadowSettings = VisibleLightShadowSettings[LightIndex];
				if (!ShadowSettings.bCastShadows) continue;
				if (LightIndex == PreviewLightIndex) PreviewShadowRange = { uint32(LightShadows.Num()), 6 };

				FLightRenderParameters& PointLight = VisibleLights[LightIndex];
				PointLight.ShadowIndex = uint32(LightShadows.Num());
				const uint32 PointShadowMapSize = ScaleShadowMapSize(PointLightShadowMapSize, ShadowSettings.ResolutionScale);

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
					ApplyShadowSettings(Shadow, ShadowSettings);

					const uint32 Index = uint32(LightShadows.Num());
					AddShadowView(Shadow, ShadowSettings, PointShadowMapSize, FVector4(PointLight.Position, PointLight.Radius));

					AreaInfos.Add({ PointShadowMapSize, Index });

					ShadowOffset++;
				}
			}

			LightShadowOffsets[FSceneRenderData::SpotLightSlotIndex] = ShadowOffset;
			for (int32 i = 0; i < GetSpotLightCount(); i++)
			{
				int32 LightIndex = GetSpotLightOffset() + i;
				const FLightShadowSettings& ShadowSettings = VisibleLightShadowSettings[LightIndex];
				if (!ShadowSettings.bCastShadows) continue;
				if (LightIndex == PreviewLightIndex) PreviewShadowRange = { uint32(LightShadows.Num()), 1 };

				FLightRenderParameters& SpotLight = VisibleLights[LightIndex];
				SpotLight.ShadowIndex = uint32(LightShadows.Num());

				FMatrix ViewMatrix = LookAt(SpotLight.Position, SpotLight.Position + SpotLight.Direction, UpVector);
				FMatrix ProjectionMatrix = Perspective(std::acos(SpotLight.OuterConeCos) * 2.0f, 1.0, 0.1f, SpotLight.Radius);

				FLightShadowRenderData Shadow{};
				Shadow.ViewProjection = ViewMatrix * ProjectionMatrix;
				Shadow.Near = 0.1f;
				Shadow.Far = SpotLight.Radius;
				ApplyShadowSettings(Shadow, ShadowSettings);

				const uint32 Index = uint32(LightShadows.Num());
				const uint32 SpotShadowMapSize = ScaleShadowMapSize(SpotLightShadowMapSize, ShadowSettings.ResolutionScale);
				// 원뿔 대신 같은 반지름의 구로 보수적으로 판정한다.
				AddShadowView(Shadow, ShadowSettings, SpotShadowMapSize, FVector4(SpotLight.Position, SpotLight.Radius));

				AreaInfos.Add({ SpotShadowMapSize, Index });

				ShadowOffset++;
			}

			float CascadeShadowFar = FMath::Min(CameraFar, MaxCascadeDistance);

			CascadeShadowCount = FMath::Clamp(ExpectCascadeCount, 1u, MaxCascadeShadowCount);

			LightShadowOffsets[FSceneRenderData::DirectionalLightSlotIndex] = ShadowOffset;
			for (int32 i = 0; i < GetDirectionalLightCount(); i++)
			{
				int32 LightIndex = GetDirectionalLightOffset() + i;
				const FLightShadowSettings& ShadowSettings = VisibleLightShadowSettings[LightIndex];
				if (!ShadowSettings.bCastShadows) continue;
				if (LightIndex == PreviewLightIndex) PreviewShadowRange = { uint32(LightShadows.Num()), CascadeShadowCount };

				FLightRenderParameters& DirectionalLight = VisibleLights[LightIndex];
				DirectionalLight.ShadowIndex = uint32(LightShadows.Num());
				FMatrix ViewMatrix = LookAt(FVector(0.f), DirectionalLight.Direction, UpVector);

				FVector ToTL = CameraFrustumCorners.FTL() - CameraFrustumCorners.NTL();
				FVector ToTR = CameraFrustumCorners.FTR() - CameraFrustumCorners.NTR();
				FVector ToBL = CameraFrustumCorners.FBL() - CameraFrustumCorners.NBL();
				FVector ToBR = CameraFrustumCorners.FBR() - CameraFrustumCorners.NBR();

				FVector PrevTL = CameraFrustumCorners.NTL();
				FVector PrevTR = CameraFrustumCorners.NTR();
				FVector PrevBL = CameraFrustumCorners.NBL();
				FVector PrevBR = CameraFrustumCorners.NBR();

				uint32 CascadeShadowMapSize = ScaleShadowMapSize(DirectionalLightShadowMapSize, ShadowSettings.ResolutionScale);
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

					FVector CascadeCenter(0.f);
					for (int32 k = 0; k < 8; k++)
						CascadeCenter += *WorldCorners[k];
					CascadeCenter *= 1.f / 8.f;
					const FVector LightCameraPosition = CascadeCenter - DirectionalLight.Direction.Normalized() * 100.f;
					const FMatrix ViewMatrix = LookAt(LightCameraPosition, CascadeCenter, UpVector);

					float RadiusSquared = 0.f;
					float MinLightX = FLT_MAX;
					float MaxLightX = -FLT_MAX;
					for (int32 k = 0; k < 8; k++)
					{
						RadiusSquared = std::max(RadiusSquared, (*WorldCorners[k] - CascadeCenter).SizeSquared());
						FVector4 InCascadeLightSpace = FVector4(*WorldCorners[k], 1.0f) * ViewMatrix;
						MinLightX = std::min(MinLightX, InCascadeLightSpace.X);
						MaxLightX = std::max(MaxLightX, InCascadeLightSpace.X);
					}

					// 회전해도 투영 크기가 변하지 않도록 cascade를 구로 감싼다. 부동 소수점 오류 방지 위해 격자점에 snap
					const float Radius = std::ceil(std::sqrt(RadiusSquared) * 16.f) / 16.f;
					const float Extent = Radius * CascadeShadowMapSize / (CascadeShadowMapSize - 1.f); // 패딩 공간 주기 위해 n / n-1 배 해주기
					const float TexelSize = 2.f * Extent / CascadeShadowMapSize;

					FVector4 CenterInLight = FVector4(CascadeCenter, 1.f) * ViewMatrix;

					const float CenterY = std::round(CenterInLight.Y / TexelSize) * TexelSize; // 중심을 가까운 격자점에 스냅함
					const float CenterZ = std::round(CenterInLight.Z / TexelSize) * TexelSize;
					float NearX = MinLightX;

					for (const FPrimitiveSceneProxy* Caster : ShadowCasterProxies)
					{
						const FAABB& Bounds = Caster->GetBounds();
						const FVector4 LightCenter = FVector4(Bounds.Center, 1.f) * ViewMatrix;
						const FVector LightExtent( // lightview 공간에서의 물체의 새로운 aabb 박스
							std::abs(ViewMatrix.M[0][0]) * Bounds.Extent.X + std::abs(ViewMatrix.M[1][0]) * Bounds.Extent.Y + std::abs(ViewMatrix.M[2][0]) * Bounds.Extent.Z,
							std::abs(ViewMatrix.M[0][1]) * Bounds.Extent.X + std::abs(ViewMatrix.M[1][1]) * Bounds.Extent.Y + std::abs(ViewMatrix.M[2][1]) * Bounds.Extent.Z,
							std::abs(ViewMatrix.M[0][2]) * Bounds.Extent.X + std::abs(ViewMatrix.M[1][2]) * Bounds.Extent.Y + std::abs(ViewMatrix.M[2][2]) * Bounds.Extent.Z);

						if (LightCenter.Y + LightExtent.Y < CenterY - Extent || LightCenter.Y - LightExtent.Y > CenterY + Extent ||
							LightCenter.Z + LightExtent.Z < CenterZ - Extent || LightCenter.Z - LightExtent.Z > CenterZ + Extent ||
							LightCenter.X - LightExtent.X > MaxLightX)
							continue;

						NearX = std::min(NearX, LightCenter.X - LightExtent.X);
					}
					NearX -= 1.f;
					FMatrix CascadeProjectionMatrix = Ortho(CenterY - Extent, CenterY + Extent, CenterZ + Extent, CenterZ - Extent, NearX, MaxLightX);

					FLightShadowRenderData Shadow{};
					Shadow.ViewProjection = ViewMatrix * CascadeProjectionMatrix;
					Shadow.Near = NearX;
					Shadow.Far = MaxLightX;
					ApplyShadowSettings(Shadow, ShadowSettings);

					const uint32 Index = uint32(LightShadows.Num());
					AddShadowView(Shadow, ShadowSettings, CascadeShadowMapSize, FVector4(0.0f, 0.0f, 0.0f, -1.0f));

					AreaInfos.Add({ CascadeShadowMapSize, Index});

					ShadowOffset++;

					PrevTL = LT;
					PrevTR = RT;
					PrevBL = LB;
					PrevBR = RB;

					CascadeShadowDistances[j] = Dist;
					//CascadeShadowMapSize /= 2;
				}
			}

			// 그림자 뷰 구성(광원·해상도·순서)이 지난 프레임과 같으면 아틀라스 위치를 그대로 쓴다.
			// 위치가 같아야 지난 프레임에 그린 깊이를 다시 그리지 않고 쓸 수 있다.
			bReuseLayout = bCacheShadows && ShadowLayout.Num() == CachedShadowLayout.Num() &&
				CachedShadows.Num() == LightShadows.Num() &&
				std::equal(ShadowLayout.begin(), ShadowLayout.end(), CachedShadowLayout.begin());
			if (bReuseLayout)
			{
				for (int32 i = 0; i < LightShadows.Num(); ++i)
				{
					LightShadows[i].AtlasIndex = CachedShadows[i].AtlasIndex;
					LightShadows[i].AtlasRect = CachedShadows[i].AtlasRect;
					UsedShadowTexels += float(ShadowLayout[i].Size) * float(ShadowLayout[i].Size);
				}
				AtlasNeedCount = CachedAtlasNeedCount;
			}
			else
			{
				std::sort(AreaInfos.GetData(), AreaInfos.GetData() + AreaInfos.Num(), [](const FAreaInfo& A, const FAreaInfo& B) { return A.Size > B.Size; });

				TArray<FQuadTree> AtlasTrees;
				FQuadTree Root(ShadowMapSize);
				AtlasTrees.Add(Root);

				for (int32 i = 0; i < AreaInfos.Num(); ++i)
				{
					const FAreaInfo& AreaInfo = AreaInfos[i];
					
					uint32 PlacedAtlasIndex = 0;
					FRect PlacedRect{};
					bool bPlaced = false;

					for (size_t AtlasIndex = 0; AtlasIndex < AtlasTrees.Num(); ++AtlasIndex)
					{
						if (AtlasTrees[AtlasIndex].InsertSquare(AreaInfo.Size, PlacedRect))
						{
							PlacedAtlasIndex = static_cast<uint32>(AtlasIndex);
							bPlaced = true;
							break;
						}
					}

					if (!bPlaced)
					{
						AtlasTrees.Add(FQuadTree(ShadowMapSize));
						PlacedAtlasIndex = static_cast<uint32>(AtlasTrees.Num() - 1);
						bPlaced = AtlasTrees.Last().InsertSquare(AreaInfo.Size, PlacedRect);
						if (!bPlaced)
						{
							CachedShadowLayout.Reset();
							bLightUpaloadSucceeded = false;
							return;
						}
					}

					FLightShadowRenderData& Shadow = LightShadows[AreaInfo.Index];
					Shadow.AtlasIndex = PlacedAtlasIndex;
					Shadow.AtlasRect = PlacedRect;
					UsedShadowTexels += float(AreaInfo.Size) * float(AreaInfo.Size);
				}
				AtlasNeedCount = static_cast<uint32>(AtlasTrees.Num());
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

		// 아틀라스를 새로 만들었으면 이전 깊이가 사라졌으므로 재사용할 수 없다.
		const FTexture2DArray* ShadowAtlas = ViewLightResources.GetShadowMapAtlas();
		if (ShadowAtlas != CachedShadowAtlas || !bLightUpaloadSucceeded) bReuseLayout = false;
		CachedShadowAtlas = bLightUpaloadSucceeded ? ShadowAtlas : nullptr;
		CachedAtlasNeedCount = AtlasNeedCount;
		UpdateShadowCache(bCacheShadows, bReuseLayout, ShadowInvalidations);
		RenderStats.DirectionalCount = GetDirectionalLightCount();
		RenderStats.PointCount = GetPointLightCount();
		RenderStats.SpotCount = GetSpotLightCount();
		RenderStats.AtlasCapacity = ShadowMapSize * ShadowMapSize * AtlasNeedCount * 4;
		// 그림자를 끈 광원과 해상도 배율이 반영되도록 실제로 배치한 타일 면적(바이트)으로 센다.
		RenderStats.ShadowTiles = UsedShadowTexels * 4;
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
