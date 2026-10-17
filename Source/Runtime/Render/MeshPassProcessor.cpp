#include "EnginePCH.h"
#include "Render/MeshPassProcessor.h"
#include "Render/SceneRenderData.h"
#include "Render/SceneView.h"
#include "Render/RenderResourceManager.h"
#include "Render/Mesh.h"
#include "Engine/PrimitiveSceneProxy.h"
#include "Core/EngineTimer.h"
#include "Engine/Scene.h"
#include "Components/StaticMeshComponent.h"
#include <algorithm>
#include <cstring>
#include <cmath>

namespace
{
	uint32 GetShadowResolution(const FVector& CameraPosition, const FVector& LightPosition)
	{
		const float DistanceSquared = (LightPosition - CameraPosition).SizeSquared();
		if (DistanceSquared < 10.0f * 10.0f) return 1024;
		if (DistanceSquared < 30.0f * 30.0f) return 512;
		return 256;
	}

	FMatrix MakeShadowView(const FVector& Position, const FVector& Forward, FVector Up)
	{
		if (Up.SizeSquared() < 1.0e-8f || std::abs(Up.Normalized().Dot(Forward)) > 0.99f)
			Up = std::abs(Forward.Z) < 0.99f ? FVector(0, 0, 1) : FVector(0, 1, 0);
		const FVector Right = Up.Cross(Forward).Normalized();
		const FVector ShadowUp = Forward.Cross(Right).Normalized();
		return FMatrix(
			Forward.X, Right.X, ShadowUp.X, 0.0f,
			Forward.Y, Right.Y, ShadowUp.Y, 0.0f,
			Forward.Z, Right.Z, ShadowUp.Z, 0.0f,
			-Position.Dot(Forward), -Position.Dot(Right), -Position.Dot(ShadowUp), 1.0f);
	}

	FMatrix MakeShadowPerspective(float HalfAngleDegrees, float NearClip, float FarClip)
	{
		const float Scale = 1.0f / std::tan(FMath::DegreesToRadians(HalfAngleDegrees));
		const float ZScale = FarClip / (FarClip - NearClip);
		return FMatrix(
			0.0f, 0.0f, ZScale, 1.0f,
			Scale, 0.0f, 0.0f, 0.0f,
			0.0f, Scale, 0.0f, 0.0f,
			0.0f, 0.0f, -NearClip * ZScale, 0.0f);
	}

	bool GetCameraFrustumBounds(const FSceneView& CameraView, const FMatrix& LightView,
		FVector& Min, FVector& Max)
	{
		// 방향광의 단일 shadow map은 카메라 전체 가시거리보다 가까운 영역에 맞춘다.
		constexpr float ShadowDistance = 50.0f;
		if (!std::isfinite(CameraView.NearClip) || !std::isfinite(CameraView.FarClip) ||
			CameraView.FarClip <= CameraView.NearClip) return false;
		const float FarFraction = std::min(ShadowDistance / (CameraView.FarClip - CameraView.NearClip), 1.0f);
		const FMatrix InverseVP = CameraView.ViewProjectionMatrix.Inverse();
		for (uint32 Index = 0; Index < 8; ++Index)
		{
			const float X = (Index & 1) ? 1.0f : -1.0f;
			const float Y = (Index & 2) ? 1.0f : -1.0f;
			const FVector4 Corner = FVector4(
				X, Y,
				(Index & 4) ? 1.0f : 0.0f, 1.0f) * InverseVP;
			if (!std::isfinite(Corner.W) || std::abs(Corner.W) < 1.0e-8f) return false;
			FVector WorldCorner(Corner.X / Corner.W, Corner.Y / Corner.W, Corner.Z / Corner.W);
			if (Index & 4)
			{
				const FVector4 NearCorner = FVector4(X, Y, 0.0f, 1.0f) * InverseVP;
				if (!std::isfinite(NearCorner.W) || std::abs(NearCorner.W) < 1.0e-8f) return false;
				const FVector NearWorld(NearCorner.X / NearCorner.W,
					NearCorner.Y / NearCorner.W, NearCorner.Z / NearCorner.W);
				WorldCorner = NearWorld + (WorldCorner - NearWorld) * FarFraction;
			}
			const FVector4 LightCorner = FVector4(
				WorldCorner.X, WorldCorner.Y, WorldCorner.Z, 1.0f) * LightView;
			if (!std::isfinite(LightCorner.X) || !std::isfinite(LightCorner.Y) ||
				!std::isfinite(LightCorner.Z)) return false;
			const FVector Point(LightCorner.X, LightCorner.Y, LightCorner.Z);
			if (Index == 0) Min = Max = Point;
			else
			{
				for (uint32 Axis = 0; Axis < 3; ++Axis)
				{
					Min.Component(Axis) = std::min(Min.Component(Axis), Point.Component(Axis));
					Max.Component(Axis) = std::max(Max.Component(Axis), Point.Component(Axis));
				}
			}
		}
		return true;
	}

	float GetNormalEncoding(UTexture2D* Texture)
	{
		if (!Texture || !Texture->GetResource() || !Texture->GetResource()->GetSRV()) return 0.0f;
		switch (Texture->GetResource()->GetFormat())
		{
		case DXGI_FORMAT_BC5_UNORM: return 2.0f;
		case DXGI_FORMAT_BC5_SNORM: return 3.0f;
		default: return 1.0f;
		}
	}

	const FMatrix& PacketWorld(const FRenderPacket& Packet)
	{
		if (Packet.Proxy) return Packet.Proxy->GetLocalToWorld();
		return Packet.Model ? *Packet.Model : FMatrix::Identity;
	}

	uint64 GroupKey(const FStaticDrawGroup& Group)
	{
		return (static_cast<uint64>(Group.Material->SortID) << 47)
			| (static_cast<uint64>(Group.Mesh->SortID) << 31)
			| (static_cast<uint64>(Group.LODIndex & 0x3) << 29);
	}

	bool BuildBatch(FMeshDrawBatch& Batch, UStaticMesh* Mesh, uint8 LOD,
		UMaterial* Material, EMeshPassType Pass, const FSceneView& View, ELightingModel LightingModel,
		const void* OverrideParams = nullptr, uint32 OverrideSize = 0)
	{
		if (!Mesh || !Material) return false;
		Batch.VertexBuffer = Mesh->GetVertexBuffer(LOD);
		Batch.IndexBuffer = Mesh->GetIndexBuffer(LOD);
		if (!Batch.VertexBuffer || !Batch.IndexBuffer) return false;
		const bool bUsesUberShader = Pass == EMeshPassType::Forward || Pass == EMeshPassType::WorldNormal;
		if (bUsesUberShader)
		{
			const bool bWorldNormal = Pass == EMeshPassType::WorldNormal;
			FPipelineState* Base = FRenderResourceManager::GetPSO(bWorldNormal
				? EPSOType::StaticMesh_WorldNormal : EPSOType::StaticMesh_Forward);
			FShaderProgram* Shader = bWorldNormal ? (Base ? Base->Shader : nullptr)
				: FRenderResourceManager::GetForwardShaderProgram(LightingModel);
			if (!Base || !Shader || !Shader->VertexShader || !Shader->PixelShader) return false;
			Batch.Pipeline = *Base;
			Batch.Pipeline.Shader = Shader;
			Batch.bUseObjectSlots = false;
			Batch.bBindViewConstants = false;
			Batch.ObjectBindingSlot = 0;
			Batch.MaterialSlot = 2;
			Batch.MaterialStages = EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel;
			Batch.SamplerStages = Batch.MaterialStages;
			Batch.bForceWireframe = !bWorldNormal && View.bIsWireframe;

			FForwardMaterialConstants Params{};
			Params.BaseColor = Material->BaseColor;
			Params.SpecularColorShininess = FVector4(Material->SpecularColor.X,
				Material->SpecularColor.Y, Material->SpecularColor.Z, std::max(1.0f, Material->Shininess));
			UTexture2D* Color = Material->Textures.IsEmpty() ? nullptr : Material->Textures[0];
			UTexture2D* Normal = Material->Textures.Num() > 1 ? Material->Textures[1] : nullptr;
			const bool bHasColor = Color && Color->GetResource() && Color->GetResource()->GetSRV();
			Params.SurfaceParams = FVector4(Material->bFlipNormalY ? 1.0f : 0.0f, 0.0f, bHasColor ? 1.0f : 0.0f,
				GetNormalEncoding(Normal));
			const FVector2 UV = Material->UVScrollSpeed * EngineTimer::GetTotalTime();
			Params.UVTransform = FVector4(UV.X, UV.Y, 1.0f, 1.0f);
			std::memcpy(Batch.MaterialData.data(), &Params, sizeof(Params));
			Batch.MaterialDataSize = sizeof(Params);
		}
		else
		{
			EPSOType Type = Material->PSOType;
			if (Pass == EMeshPassType::GBuffer && Type == EPSOType::StaticMesh_Opaque)
				Type = EPSOType::StaticMesh_GBuffer;
			else if (View.bIsWireframe && Type == EPSOType::StaticMesh_Opaque)
				Type = EPSOType::StaticMesh_Wireframe;
			FPipelineState* Pipeline = FRenderResourceManager::GetPSO(Type);
			if (!Pipeline) return false;
			Batch.Pipeline = *Pipeline;
			Batch.bForceWireframe = Pass == EMeshPassType::GBuffer && View.bIsWireframe;
			if (Material->PSOType == EPSOType::StaticMesh_Opaque ||
				Material->PSOType == EPSOType::StaticMesh_Translucent ||
				Material->PSOType == EPSOType::StaticMesh_Wireframe)
			{
				FStaticMeshMaterialParams Params{};
				Params.BaseColor = Material->BaseColor;
				Params.UVOffset = Material->UVScrollSpeed * EngineTimer::GetTotalTime();
				Params.TextureFlags.X = GetNormalEncoding(Material->Textures.Num() > 1 ? Material->Textures[1] : nullptr);
				Params.TextureFlags.Y = Material->bFlipNormalY ? 1.0f : 0.0f;
				std::memcpy(Batch.MaterialData.data(), &Params, sizeof(Params));
				Batch.MaterialDataSize = sizeof(Params);
			}
			else if (OverrideParams && OverrideSize <= Batch.MaterialData.size())
			{
				std::memcpy(Batch.MaterialData.data(), OverrideParams, OverrideSize);
				Batch.MaterialDataSize = OverrideSize;
			}
		}
		Batch.Sampler = Material->SamplerState;
		Batch.Textures.Reserve(Material->Textures.Num());
		for (int32 Index = 0; Index < Material->Textures.Num(); ++Index)
		{
			// Forward 조명은 t8 이상을 전역 리소스에 사용한다.
			if (bUsesUberShader && Index >= 8) break;
			UTexture2D* Texture = Material->Textures[Index];
			EShaderBindFlagBits Stages = EShaderBindFlagBits::Pixel;
			if (bUsesUberShader && Index == 1)
				Stages = EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel;
			if (bUsesUberShader && Index == 0 &&
				(!Texture || !Texture->GetResource() || !Texture->GetResource()->GetSRV()))
				Texture = nullptr;
			if (bUsesUberShader && Index == 1 && GetNormalEncoding(Texture) <= 0.5f)
				Texture = nullptr;
			Batch.Textures.Add({ Texture, Stages });
		}
		return true;
	}

	FMeshDrawBatch::FItem BuildItem(const FMatrix& World, const FSceneView& View,
		EMeshPassType Pass, uint32 Slot, uint32 StartIndex, uint32 IndexCount,
		const void* ObjectKey, bool bOccludedByGpu)
	{
		FMeshDrawBatch::FItem Item{};
		Item.ObjectSlot = Slot;
		Item.StartIndex = StartIndex;
		Item.IndexCount = IndexCount;
		Item.ObjectKey = ObjectKey;
		Item.bOccludedByGpu = bOccludedByGpu;
		if (Pass == EMeshPassType::Forward || Pass == EMeshPassType::WorldNormal)
		{
			FForwardObjectConstants Constants{};
			Constants.World = World;
			Constants.View = View.ViewMatrix;
			Constants.Projection = View.ProjectionMatrix;
			Constants.NormalMatrix = World.Inverse().GetTransposed();
			std::memcpy(Item.ObjectData.data(), &Constants, sizeof(Constants));
			Item.ObjectDataSize = sizeof(Constants);
		}
		else
		{
			const FPerObjectConstants Constants = MakePerObjectConstants(World);
			std::memcpy(Item.ObjectData.data(), &Constants, sizeof(Constants));
			Item.ObjectDataSize = sizeof(Constants);
		}
		return Item;
	}

	FMeshDrawBatch::FItem BuildBatchItem(const FMatrix& World, const FMatrix& View, const FMatrix& Projection,
		uint32 Slot, uint32 StartIndex, uint32 IndexCount,
		const void* ObjectKey, bool bOccludedByGpu)
	{
		FMeshDrawBatch::FItem Item{};
		Item.ObjectSlot = Slot;
		Item.StartIndex = StartIndex;
		Item.IndexCount = IndexCount;
		Item.ObjectKey = ObjectKey;
		Item.bOccludedByGpu = bOccludedByGpu;

		FForwardObjectConstants Constants{};
		Constants.World = World;
		Constants.View = View;
		Constants.Projection = Projection;
		Constants.NormalMatrix = World.Inverse().GetTransposed();
		std::memcpy(Item.ObjectData.data(), &Constants, sizeof(Constants));
		Item.ObjectDataSize = sizeof(Constants);
		
		return Item;
	}



	bool BuildShadowBatches(
		const TArray<FPrimitiveSceneProxy*>& ShadowMeshes,
		TArray<FMeshDrawBatch>& OutBatches)
	{
		OutBatches.Reset();

		const FPipelineState* Pipeline =
			FRenderResourceManager::GetPSO(EPSOType::StaticMesh_DepthOnly);
		FShaderProgram* ShadowProgram = FRenderResourceManager::GetShadowDepthShaderProgram();

		if (!Pipeline || !ShadowProgram || !ShadowProgram->VertexShader)
			return false;

		for (FPrimitiveSceneProxy* Proxy : ShadowMeshes)
		{
			if (!Proxy || !Proxy->GetRenderState()) continue;
			// 조명 방향 화살표 등 에디터 표시용 메시는 그림자를 만들지 않는다.
			const auto* MeshComponent = Cast<UStaticMeshComponent>(Proxy->GetComponent());
			if (MeshComponent && MeshComponent->IsEditorIcon()) continue;

			UStaticMesh* Mesh = Proxy->GetMesh();
			if (!Mesh || Proxy->GetLODCount() == 0) continue;

			// 우선 LOD 0 사용
			constexpr uint8 LOD = 0;
			const FCachedMeshLOD& CachedLOD = Proxy->GetLOD(LOD);

			FMeshDrawBatch Batch{};
			Batch.Pipeline = *Pipeline;
			Batch.Pipeline.Shader = ShadowProgram;
			Batch.VertexBuffer = Mesh->GetVertexBuffer(LOD);
			Batch.IndexBuffer = Mesh->GetIndexBuffer(LOD);
			Batch.bUseObjectSlots = false;
			Batch.bBindViewConstants = true;
			Batch.ObjectBindingSlot = 2;
			Batch.ObjectStages = EShaderBindFlagBits::Vertex;

			if (!Batch.VertexBuffer || !Batch.IndexBuffer)
				return false;

			for (uint32 Index = 0; Index < CachedLOD.NumSections; ++Index)
			{
				const FCachedMeshSection& Section =
					Proxy->GetSection(CachedLOD.FirstSection + Index);

				if (Section.IndexCount == 0) continue;

				FMeshDrawBatch::FItem Item{};
				Item.StartIndex = Section.StartIndex;
				Item.IndexCount = Section.IndexCount;
				Item.ObjectKey = Proxy;

				const FMatrix& World = Proxy->GetLocalToWorld();
				std::memcpy(Item.ObjectData.data(), &World, sizeof(World));
				Item.ObjectDataSize = sizeof(World);

				Batch.Items.Add(std::move(Item));
			}

			if (!Batch.Items.IsEmpty())
				OutBatches.Add(std::move(Batch));
		}

		return true;
	}

}

// 그림자 시점과 해상도를 준비한다. Atlas 영역은 FSceneRenderer에서 배정한다.
bool FMeshPassProcessor::Build(const FSceneView& CameraView, FScene& Scene,
	const TArray<FLightRenderParameters>& Lights,
	TArray<FShadowDrawPass>& OutPasses) const
{
	OutPasses.Reset();
	const FVector PointDirections[6] = {
		{ 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 },
		{ 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
	const FVector PointUpVectors[6] = {
		{ 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 },
		{ 0, 0, 1 }, { 0, -1, 0 }, { 0, 1, 0 } };

	for (uint32 LightIndex = 0; LightIndex < static_cast<uint32>(Lights.Num()); ++LightIndex)
	{
		const FLightRenderParameters& Light = Lights[LightIndex];
		if (!std::isfinite(Light.Intensity) || Light.Intensity <= 0.0f) continue;
		const ELightType Type = static_cast<ELightType>(Light.Type);
		const bool bDirectional = Type == ELightType::Directional;
		if (!bDirectional && Type != ELightType::Point && Type != ELightType::Spot &&
			Type != ELightType::Rect) continue;
		if (!bDirectional && (!std::isfinite(Light.Radius) || Light.Radius <= 0.0f)) continue;

		const uint32 Resolution = bDirectional ? 2048 : GetShadowResolution(CameraView.ViewLocation, Light.Position); // 카메라와 광원 위치에 따른 texture 해상도 결정
		const uint32 FaceCount = Type == ELightType::Point ? 6u : 1u; // 포인트 라이트면 6개

		for (uint32 FaceIndex = 0; FaceIndex < FaceCount; ++FaceIndex)
		{
			FShadowDrawPass Pass{};
			Pass.LightIndex = LightIndex;
			Pass.FaceIndex = FaceIndex;
			Pass.Resolution = Resolution;

			FSceneView ShadowView{};
			ShadowView.ViewRect = { 0, 0, static_cast<float>(Resolution), static_cast<float>(Resolution) };
			ShadowView.AspectRatio = 1.0f;
			ShadowView.bIsPerspective = !bDirectional;
			ShadowView.ViewForward = Type == ELightType::Point ? PointDirections[FaceIndex] : Light.Direction.GetSafeNormal();

			if (ShadowView.ViewForward.SizeSquared() < 1.0e-8f) return false; // 조명 방향 벡터가 거의 (0, 0, 0)인 상황

			FVector Up = Type == ELightType::Point ? PointUpVectors[FaceIndex] : FVector(0, 0, 1);

			if (Type == ELightType::Rect) Up = ShadowView.ViewForward.Cross(Light.Right);

			if (bDirectional)
			{
				const FMatrix LightView = MakeShadowView(CameraView.ViewLocation, ShadowView.ViewForward, Up);
				FVector Min, Max;

				if (!GetCameraFrustumBounds(CameraView, LightView, Min, Max)) return false; // 카메라 frustum 꼭짓점 가지고 AABB 만들기
				Min.X -= 20.0f; // 카메라 밖에서 가시 영역으로 그림자를 드리우는 물체의 공간

				const float Width = std::max(Max.Y - Min.Y, 0.01f);
				const float Height = std::max(Max.Z - Min.Z, 0.01f);
				const float Depth = std::max(Max.X - Min.X, 0.01f);

				ShadowView.NearClip = 0.1f;

				const FVector4 Origin = FVector4(Min.X - ShadowView.NearClip,
					(Min.Y + Max.Y) * 0.5f, (Min.Z + Max.Z) * 0.5f, 1.0f) * LightView.Inverse(); // Directional Camera 위치

				ShadowView.ViewLocation = FVector(Origin.X, Origin.Y, Origin.Z);
				ShadowView.FarClip = Depth + ShadowView.NearClip;

				ShadowView.OrthoWidth = Width;
				ShadowView.AspectRatio = Width / Height;
				const float ZScale = 1.0f / (ShadowView.FarClip - ShadowView.NearClip);
				ShadowView.ProjectionMatrix = FMatrix(
					0.0f, 0.0f, ZScale, 0.0f,
					2.0f / Width, 0.0f, 0.0f, 0.0f,
					0.0f, 2.0f / Height, 0.0f, 0.0f,
					0.0f, 0.0f, -ShadowView.NearClip * ZScale, 1.0f);
			}
			else
			{
				ShadowView.ViewLocation = Light.Position;
				ShadowView.NearClip = std::min(0.1f, Light.Radius * 0.01f);
				ShadowView.FarClip = Light.Radius;
				float HalfAngle = 45.0f;
				if (Type == ELightType::Spot)
				{
					HalfAngle = FMath::Clamp(std::acos(FMath::Clamp(Light.OuterConeCos, -1.0f, 1.0f))
						* (180.0f / PI), 0.5f, 89.0f);
				}
				// Rect는 면광원 중심에서 전방을 보는 단일 투영으로 근사한다.
				if (Type == ELightType::Rect) HalfAngle = 89.0f;
				ShadowView.FieldOfView = HalfAngle * 2.0f;
				ShadowView.ProjectionMatrix = MakeShadowPerspective(HalfAngle, ShadowView.NearClip, ShadowView.FarClip);
			}
			ShadowView.ViewMatrix = MakeShadowView(ShadowView.ViewLocation, ShadowView.ViewForward, Up);
			ShadowView.ViewRotation = MatrixToRotator(ShadowView.ViewMatrix.GetTransposed());
			ShadowView.ViewProjectionMatrix = ShadowView.ViewMatrix * ShadowView.ProjectionMatrix;
			ShadowView.Frustum = ExtractFrustumPlanes(ShadowView.ViewProjectionMatrix);

			TArray<FPrimitiveSceneProxy*> ShadowMeshes;

			Scene.BVH.QueryCull(
				FrustumAllPlanesMask,
				[&ShadowView](const FBox& Bounds, uint32& Mask)
				{
					// 메시 경계 상자와 조명 절두체의 교차 검사
					return static_cast<EBVHCullResult>(
						ClassifyBoxInFrustum(
							Bounds, ShadowView.Frustum, Mask));
				},
				[&ShadowMeshes, &CameraView](FPrimitiveSceneProxy* Proxy)
				{
					// 절두체를 통과한 메시 중 표시 가능한 것만 저장
					if (Proxy->IsVisible() && Proxy->IsShown(CameraView.bGameView))
						ShadowMeshes.Add(Proxy);
				});

			
			if (!BuildShadowBatches(ShadowMeshes, Pass.Batches)) return false;

			Pass.Parameters.LightVP = ShadowView.ViewProjectionMatrix;
			OutPasses.Add(std::move(Pass));
		}
	}

	return true;
}

bool FMeshPassProcessor::Build(const FSceneView& View, const FRenderQueue& Queue,
	const FSceneRenderData* SceneData, EMeshPassType Pass,
	TArray<FMeshDrawBatch>& Out, ELightingModel LightingModel) const
{
	Out.Reset();
	if (Pass != EMeshPassType::Translucent && SceneData)
	{
		TArray<const FStaticDrawGroup*> Groups;
		for (const FSceneRenderData::FGatherChunk& Chunk : SceneData->GatherChunks)
			for (const FStaticDrawGroup& Group : Chunk.Groups)
				if (!Group.Items.empty()) Groups.Add(&Group);

		std::sort(Groups.begin(), Groups.end(),
			[](const FStaticDrawGroup* A, const FStaticDrawGroup* B) { return GroupKey(*A) < GroupKey(*B); }); // 재질, 메시, LOD 순으로 정렬

		for (const FStaticDrawGroup* Group : Groups)
		{
			FMeshDrawBatch Batch; // Render에게 전달하는 최종 구조체
			if (!BuildBatch(Batch, Group->Mesh, Group->LODIndex, Group->Material, Pass, View, LightingModel))
				return false;
			Batch.Items.Reserve(static_cast<uint32>(Group->Items.size()));
			for (const FStaticDrawItem& Item : Group->Items)
				Batch.Items.Add(BuildItem(Item.Proxy->GetLocalToWorld(), View, Pass, Item.Slot,
					Item.StartIndex, Item.IndexCount, Item.Proxy, Item.bOccludedByGpu != 0));
			Out.Add(std::move(Batch));
		}
	}
	const uint32 Begin = Pass == EMeshPassType::Translucent ? Queue.GetFirstTranslucentIndex() : 0;
	const uint32 End = Pass == EMeshPassType::Translucent ? Queue.Num() : Queue.GetFirstTranslucentIndex();
	Out.Reserve(Out.Num() + End - Begin);
	for (uint32 Index = Begin; Index < End; ++Index)
	{
		const FRenderPacket& Packet = Queue[Index];
		if (Pass == EMeshPassType::WorldNormal &&
			(!Packet.Material || Packet.Material->PSOType != EPSOType::StaticMesh_Opaque)) continue;
		FMeshDrawBatch Batch;
		if (!BuildBatch(Batch, Packet.Mesh, Packet.LODIndex, Packet.Material, Pass, View,
			LightingModel, Packet.MaterialParamData, Packet.MaterialParamDataSize)) return false;

		const uint32 IndexCount = Packet.IndexCount ? Packet.IndexCount : Batch.IndexBuffer->GetIndexCount();
		const void* ObjectKey = Packet.Proxy ? static_cast<const void*>(Packet.Proxy) : static_cast<const void*>(&Packet);

		Batch.Items.Add(BuildItem(PacketWorld(Packet), View, Pass, Packet.Slot,
			Packet.StartIndex, IndexCount, ObjectKey, Packet.bOccludedByGpu));
		Out.Add(std::move(Batch));
	}
	return true;
}
