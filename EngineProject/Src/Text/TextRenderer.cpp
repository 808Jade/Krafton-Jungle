#include "EnginePCH.h"
#include "TextRenderer.h"

#include "Camera/CameraComponent.h"

#include "Render/RenderCommand.h"

namespace
{
	TArray<uint32> DecodeUTF8(const FString& Str)
	{
		TArray<uint32> Out;
		const unsigned char* P = reinterpret_cast<const unsigned char*>(Str.c_str());
		const unsigned char* End = P + Str.size();

		while (P < End)
		{
			uint32 CP = 0;
			int Len = 0;
			if (*P < 0x80) { CP = *P;        Len = 1; }
			else if ((*P & 0xE0) == 0xC0) { CP = *P & 0x1F; Len = 2; }
			else if ((*P & 0xF0) == 0xE0) { CP = *P & 0x0F; Len = 3; }
			else if ((*P & 0xF8) == 0xF0) { CP = *P & 0x07; Len = 4; }
			else { ++P; continue; }                         // 잘못된 바이트 스킵

			if (P + Len > End) break;
			for (int i = 1; i < Len; ++i) CP = (CP << 6) | (P[i] & 0x3F);

			Out.Add(CP);
			P += Len;
		}
		return Out;
	}
}

void FTextRenderer::Init(FRenderer* InRenderer, uint32 InMaxCharacters)
{
	Renderer = InRenderer;
	MaxCharacters = InMaxCharacters;
	MaxVertices = MaxCharacters * 4; // 글자당 쿼드 4버텍스
	MaxIndices = MaxCharacters * 6;  // 글자당 삼각형 2개(6인덱스)

	// 다이나믹 버텍스/인덱스 버퍼 생성 (D3D11_USAGE_DYNAMIC, CPU_ACCESS_WRITE)
	VertexBuffer = RenderCommand::CreateDynamicVertexBuffer(sizeof(FTextVertex) * MaxVertices, sizeof(FTextVertex));
	IndexBuffer = RenderCommand::CreateDynamicIndexBuffer(sizeof(uint32) * MaxIndices);

	MVP = RenderCommand::CreateConstantBuffer(sizeof(TextTransformData));
	ScreenPx = RenderCommand::CreateConstantBuffer(sizeof(MSDFData));

	TextShader = RenderCommand::CreateShader(L"Assets/Shader/TextShader.hlsl", FTextVertex::GetLayout());
}

void FTextRenderer::BuildTextMesh(const FString& Text, float TextSize, const UFontAtlas& Atlas,
	TArray<FTextVertex>& OutVertices, TArray<uint32>& OutIndices)
{
	TArray<uint32> CodePoints = DecodeUTF8(Text);

	float PenY = 0.0f;

	// 전체 텍스트 너비를 미리 계산해서 중앙 정렬 (UUID 라벨은 보통 중앙 정렬이 자연스러움)
	float TotalWidth = 0.0f;
	for (uint32 CP : CodePoints)
	{
		if (const FGlyphInfo* Glyph = Atlas.GlyphMap.FindOrNull(CP))
		{
			TotalWidth += Glyph->Advance * TextSize;
		}
	}
	float StartY = 0.0f; // -TotalWidth * 0.5f;
	PenY = StartY;

	for (uint32 CP : CodePoints)          // 메시 생성 루프
	{
		const FGlyphInfo* Glyph = Atlas.GlyphMap.FindOrNull(CP);
		if (!Glyph)
		{
			continue;
		}

		if (Glyph->bHasBounds)
		{
			float Left = PenY + Glyph->PlaneLeft * TextSize;
			float Bottom = Glyph->PlaneBottom * TextSize;
			float Right = PenY + Glyph->PlaneRight * TextSize;
			float Top = Glyph->PlaneTop * TextSize;

			float ULeft = Glyph->AtlasLeft / Atlas.Width;
			float VBottom = 1.0f - (Glyph->AtlasBottom / Atlas.Height);
			float URight = Glyph->AtlasRight / Atlas.Width;
			float VTop = 1.0f - (Glyph->AtlasTop / Atlas.Height);

			uint32 BaseIndex = static_cast<uint32>(OutVertices.Num());

			OutVertices.Add({ FVector(Bottom,Left, 0.0f), FVector2(ULeft, VBottom) });
			OutVertices.Add({ FVector(Top,Left, 0.0f), FVector2(ULeft, VTop) });
			OutVertices.Add({ FVector(Top, Right,  0.0f), FVector2(URight, VTop) });
			OutVertices.Add({ FVector(Bottom, Right, 0.0f), FVector2(URight,VBottom) });

			OutIndices.Add(BaseIndex + 0);
			OutIndices.Add(BaseIndex + 1);
			OutIndices.Add(BaseIndex + 2);
			OutIndices.Add(BaseIndex + 0);
			OutIndices.Add(BaseIndex + 2);
			OutIndices.Add(BaseIndex + 3);
		}

		PenY += Glyph->Advance * TextSize;
	}
}

void FTextRenderer::OnRender(const FString& Text, const FVector& WorldPos, float TextSize,
	const UFontAtlas& Atlas, UCameraComponent* CameraComponent)
{
	if (Text.empty() || !Atlas.AtlasTexture)
	{
		return;
	}

	TArray<FTextVertex> Vertices;
	TArray<uint32> Indices;
	BuildTextMesh(Text, TextSize, Atlas, Vertices, Indices);

	if (Vertices.IsEmpty())
	{
		return;
	}

	// 버퍼 용량 초과 체크 (UUID 정도면 안 넘겠지만 방어적으로)
	if (Vertices.Num() > (int32)MaxVertices || Indices.Num() > (int32)MaxIndices)
	{
		// 필요시 여기서 버퍼 재생성 로직 추가 가능
		return;
	}

	RenderCommand::UpdateBufferData(VertexBuffer.get(), Vertices.GetData(), sizeof(FTextVertex) * Vertices.Num());
	RenderCommand::UpdateBufferData(IndexBuffer.get(), Indices.GetData(), sizeof(uint32) * Indices.Num());

	const FTransform &Transform = CameraComponent->GetTransform();

	//// 빌보드 Worl d 행렬: CameraRight/Up을 축으로, WorldPos에 위치
	FVector Right = Transform.GetRight().Normalized();
	FVector Up = Transform.GetUp().Normalized();
	FVector Forward = Transform.GetForward().Normalized();   // 또는 Cross로 계산

	FVector BbUp = Transform.GetUp().Normalized();                         // Z-up 월드
	FVector BbRight = FVector::Cross(BbUp, Forward).Normalized();
	FVector BbFwd = FVector::Cross(BbUp, BbRight);          // 자기모순 없게 외적으로 유도

	// 카메라가 수직으로 내려다볼 때 폴백
	if (BbRight.Length() <= 1e-6f)
	{
		BbRight = Right;
		BbFwd = FVector::Cross(BbUp, BbRight);
	}

	FMatrix World;
	World.SetIdentity();
	// row0 = 로컬 +X = up
	World.M[0][0] = BbUp.X;    World.M[0][1] = BbUp.Y;    World.M[0][2] = BbUp.Z;    World.M[0][3] = 0;
	// row1 = 로컬 +Y = right
	World.M[1][0] = BbRight.X; World.M[1][1] = BbRight.Y; World.M[1][2] = BbRight.Z; World.M[1][3] = 0;
	// row2 = 로컬 +Z = forward
	World.M[2][0] = BbFwd.X;         World.M[2][1] = BbFwd.Y;         World.M[2][2] = BbFwd.Z;         World.M[2][3] = 0;
	World.M[3][0] = WorldPos.X;      World.M[3][1] = WorldPos.Y;      World.M[3][2] = WorldPos.Z;      World.M[3][3] = 1;

	//World.M[0][0] = Right.X;   World.M[0][1] = Right.Y;   World.M[0][2] = Right.Z;   World.M[0][3] = 0.0f;
	//World.M[1][0] = Up.X;      World.M[1][1] = Up.Y;      World.M[1][2] = Up.Z;      World.M[1][3] = 0.0f;
	//World.M[2][0] = Forward.X; World.M[2][1] = Forward.Y; World.M[2][2] = Forward.Z; World.M[2][3] = 0.0f;
	//World.M[3][0] = WorldPos.X; World.M[3][1] = WorldPos.Y; World.M[3][2] = WorldPos.Z; World.M[3][3] = 1.0f;

	TextTransformData TransData;
	TransData.World = World.GetTransposed();
	TransData.ViewProj = CameraComponent->GetViewProjectionMatrix().GetTransposed();

	MSDFData MSDFData;
	MSDFData.ScreenPx = Atlas.DistanceRange;

	RenderCommand::SetRasterizerState(ERasterizerState::SolidBack);
	RenderCommand::BindShader(TextShader.get());
	RenderCommand::SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	RenderCommand::BindShaderResource(0, Atlas.AtlasTexture, EShaderBindFlagBits::Pixel);
	RenderCommand::UpdateBufferData(MVP.get(), &TransData, sizeof(TextTransformData));
	RenderCommand::UpdateBufferData(ScreenPx.get(), &MSDFData, sizeof(MSDFData));

	RenderCommand::BindConstantBuffer(0, MVP.get(), EShaderBindFlagBits::Vertex);
	RenderCommand::BindConstantBuffer(1, ScreenPx.get(), EShaderBindFlagBits::Pixel);

	RenderCommand::BindVertexBuffer(VertexBuffer.get());
	RenderCommand::BindIndexBuffer(IndexBuffer.get());
	RenderCommand::DrawIndexed(Indices.Num());
}