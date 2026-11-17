#include "EnginePCH.h"
#include "Editor/Rendering/SkeletonRenderer.h"

#include "Editor/Gizmo/GizmoRenderer.h"
#include "Asset/AssetManager.h"
#include "Render/RenderCommand.h"
#include "Render/RenderResourceManager.h"
#include "Render/SkeletalMeshData.h"

#include <cmath>

namespace
{
	FVector GetMatrixLocation(const FMatrix& M)
	{
		return FVector(M.M[3][0], M.M[3][1], M.M[3][2]);
	}

	// 부모 관절(P0)에서 자식 관절(P1)로 뻗는 본 메시의 월드 행렬. 팔면체 메시의 Z축이 본 방향이다.
	bool MakeBoneWorld(const FVector& P0, const FVector& P1, float RadiusRatio, FMatrix& OutWorld)
	{
		const FVector Diff = P1 - P0;
		const float BoneLength = Diff.Length();
		if (BoneLength <= 0.001f)
			return false;

		const FVector Dir = Diff / BoneLength;
		const FVector RefAxis = (std::abs(Dir.Z) < 0.9f) ? FVector(0.0f, 0.0f, 1.0f) : FVector(1.0f, 0.0f, 0.0f);
		const FVector SideA = Dir.Cross(RefAxis).GetSafeNormal();
		const FVector SideB = SideA.Cross(Dir).GetSafeNormal();
		const float Radius = BoneLength * RadiusRatio;

		OutWorld = FMatrix::Identity;
		OutWorld.SetAxis(0, SideA * Radius);
		OutWorld.SetAxis(1, SideB * Radius);
		OutWorld.SetAxis(2, Dir * BoneLength);
		OutWorld.M[3][0] = P0.X;
		OutWorld.M[3][1] = P0.Y;
		OutWorld.M[3][2] = P0.Z;
		return true;
	}

	// 선택한 본으로 들어오는 본, 선택한 본에서 나가는 본, Hover한 본, 나머지 순으로 색을 고른다.
	FVector4 PickBoneColor(const FSkeletonDrawParams& Params, int32 Bone, int32 Parent,
		const FVector4& Incoming, const FVector4& Outgoing, const FVector4& Hovered, const FVector4& Normal)
	{
		if (Bone == Params.SelectedBone) return Incoming;
		if (Parent == Params.SelectedBone) return Outgoing;
		if (Bone == Params.HoveredBone || Parent == Params.HoveredBone) return Hovered;
		return Normal;
	}
}

FSkeletonRenderer::FSkeletonRenderer() = default;
FSkeletonRenderer::~FSkeletonRenderer() = default;

bool FSkeletonRenderer::Init()
{
	BoneMesh = UAssetManager::GetAssetByPath<UStaticMesh>("OctahedralBone");
	JointMesh = UAssetManager::GetAssetByPath<UStaticMesh>("GizmoSphere");
	ConstantBuffer = RenderCommand::CreateConstantBuffer(sizeof(FGizmoData));
	Shader = FRenderResourceManager::GetShaderProgram("Resources/Shader/GizmoShader.hlsl");
	PipelineState.Shader = Shader;
	PipelineState.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	return BoneMesh && ConstantBuffer && Shader;
}

void FSkeletonRenderer::DrawBone(const FMatrix& World, const FMatrix& ViewProjection, const FVector4& Color, const uint32 IndexCount)
{
	FGizmoData Data{};
	Data.World = World;
	Data.ViewProj = ViewProjection;
	Data.Color = Color;
	RenderCommand::UpdateBufferData(ConstantBuffer.get(), &Data, sizeof(FGizmoData));
	RenderCommand::BindConstantBuffer(0, ConstantBuffer.get(), EShaderBindFlagBits::Vertex);
	RenderCommand::DrawIndexed(IndexCount);
}

void FSkeletonRenderer::Render(const FSkeletonDrawParams& Params, const FMatrix& ViewProjection, FTexture2D* DepthStencil)
{
	if (!BoneMesh || !ConstantBuffer || !Shader || !Params.Bones)
		return;

	const TArray<FSkeletalBone>& Bones = *Params.Bones;
	const TArray<FMatrix>& Matrices = Params.BoneWorldMatrices;
	const int32 NumBones = Bones.Num();
	if (NumBones == 0 || Matrices.Num() != NumBones)
		return;

	// 뼈대가 캐릭터 메시 안쪽에 파묻히지 않도록 장면 깊이를 지우고, 뼈대들끼리만 깊이 판정한다.
	if (DepthStencil)
		RenderCommand::ClearDepthStencil(DepthStencil);
	RenderCommand::BindPipelineState(PipelineState);
	RenderCommand::SetDepthStencilState(EDepthStencilState::Default);
	RenderCommand::BindMesh(BoneMesh);
	const uint32 BoneIndexCount = BoneMesh->IndexBuffer->GetIndexCount();

	// 본 하나를 정해진 굵기·색으로 그린다. 부모가 없는 루트 본은 그릴 선분이 없다.
	const auto DrawAllBones = [&](float RadiusRatio, const FVector4& Incoming, const FVector4& Outgoing, const FVector4& Hovered, const FVector4& Normal)
	{
		for (int32 i = 0; i < NumBones; ++i)
		{
			const int32 Parent = Bones[i].ParentIndex;
			if (Parent < 0 || Parent >= NumBones)
				continue;
			FMatrix World;
			if (!MakeBoneWorld(GetMatrixLocation(Matrices[Parent]), GetMatrixLocation(Matrices[i]), RadiusRatio, World))
				continue;
			DrawBone(World, ViewProjection, PickBoneColor(Params, i, Parent, Incoming, Outgoing, Hovered, Normal), BoneIndexCount);
		}
	};

	const FVector4 WireIncoming(1.0f, 0.88f, 0.15f, 1.0f);
	const FVector4 WireOutgoing(1.0f, 0.76f, 0.12f, 1.0f);
	const FVector4 WireHovered(1.0f, 0.96f, 0.55f, 0.95f);
	const FVector4 WireNormal(0.37f, 0.85f, 1.0f, 0.90f);

	if (Params.bDrawCones)
	{
		// 1. 반투명으로 채운 팔면체
		RenderCommand::SetBlendState(EBlendState::AlphaBlend);
		RenderCommand::SetRasterizerState(ERasterizerState::SolidNone);
		DrawAllBones(Params.ConeWidthRatio,
			FVector4(1.0f, 0.57f, 0.0f, 0.55f), FVector4(1.0f, 0.45f, 0.08f, 0.50f),
			FVector4(1.0f, 0.95f, 0.45f, 0.40f), FVector4(0.25f, 0.75f, 0.98f, 0.30f));

		// 2. 같은 팔면체의 모서리 외곽선
		RenderCommand::SetRasterizerState(ERasterizerState::Wireframe);
		DrawAllBones(Params.ConeWidthRatio, WireIncoming, WireOutgoing, WireHovered, WireNormal);
	}
	else
	{
		// 선 모드: 아주 가는 팔면체의 외곽선만 그린다.
		RenderCommand::SetBlendState(EBlendState::Opaque);
		RenderCommand::SetRasterizerState(ERasterizerState::Wireframe);
		DrawAllBones(0.012f, WireIncoming, WireOutgoing, WireHovered, WireNormal);
	}

	// 3. 관절 구
	if (JointMesh && Params.JointRadius > 0.01f)
	{
		RenderCommand::SetBlendState(EBlendState::AlphaBlend);
		RenderCommand::SetRasterizerState(ERasterizerState::SolidNone);
		RenderCommand::BindMesh(JointMesh);
		const uint32 JointIndexCount = JointMesh->IndexBuffer->GetIndexCount();

		// 모델 크기에 비례하도록 정규화한다. (구 메시의 기본 반지름은 0.2)
		const float ModelScale = Params.MeshRadius > 0.001f ? Params.MeshRadius : 1.0f;
		const float ScaleFactor = ModelScale * 0.0020f * Params.JointRadius / 0.2f;

		for (int32 i = 0; i < NumBones; ++i)
		{
			float Scale = ScaleFactor;
			FVector4 Color(0.70f, 0.90f, 1.0f, 0.85f);
			if (i == Params.SelectedBone)
			{
				Scale *= 1.35f;
				Color = FVector4(1.0f, 0.55f, 0.0f, 0.95f);
			}
			else if (i == Params.HoveredBone)
			{
				Scale *= 1.20f;
				Color = FVector4(1.0f, 0.95f, 0.4f, 0.90f);
			}
			else if (Bones[i].ParentIndex == -1)
			{
				Color = FVector4(1.0f, 0.35f, 0.35f, 0.90f);
			}

			const FVector Pos = GetMatrixLocation(Matrices[i]);
			FMatrix World = FMatrix::Identity;
			World.M[0][0] = World.M[1][1] = World.M[2][2] = Scale;
			World.M[3][0] = Pos.X;
			World.M[3][1] = Pos.Y;
			World.M[3][2] = Pos.Z;
			DrawBone(World, ViewProjection, Color, JointIndexCount);
		}
	}

	// 다음 오버레이 그리기를 위해 기본 상태로 되돌린다.
	RenderCommand::SetBlendState(EBlendState::Opaque);
	RenderCommand::SetRasterizerState(ERasterizerState::SolidBack);
}
