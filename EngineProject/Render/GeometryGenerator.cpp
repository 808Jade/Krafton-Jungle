#include "EnginePCH.h"
#include "GeometryGenerator.h"

UStaticMeshData FGeometryGenerator::CreatePlane(float Size, const FVector4& Color)
{
	UStaticMeshData PlaneMeshData;

	float HalfSize = Size / 2.0f;

	TArray<FVector> positions =
	{
		FVector(-HalfSize, HalfSize, 0.0f),
		FVector(HalfSize, HalfSize, 0.0f),
		FVector(HalfSize, -HalfSize, 0.0f),
		FVector(-HalfSize, -HalfSize, 0.0f),
	};

	for (int32 i = 0; i < positions.Num(); i++)
	{
		PlaneMeshData.Vertices.Add({ positions[i], Color });
	}

	PlaneMeshData.Indices.Add(0);
	PlaneMeshData.Indices.Add(2);
	PlaneMeshData.Indices.Add(1);

	PlaneMeshData.Indices.Add(0);
	PlaneMeshData.Indices.Add(3);
	PlaneMeshData.Indices.Add(2);

	return PlaneMeshData;
}

//UStaticMeshData FGeometryGenerator::CreatePlane(float Size, const FVector4& Color)
//{
//	UStaticMeshData PlaneMeshData;
//
//	float HalfSize = Size / 2.0f;
//
//	TArray<FVector> positions =
//	{
//		FVector(-HalfSize, HalfSize, 0.0f),
//		FVector(HalfSize, HalfSize, 0.0f),
//		FVector(HalfSize, -HalfSize, 0.0f),
//		FVector(-HalfSize, -HalfSize, 0.0f),
//	};
//
//	TArray<FVector2> uvs =
//	{
//		FVector2(0.0f, 0.0f), // top-left
//		FVector2(1.0f, 0.0f), // top-right
//		FVector2(1.0f, 1.0f), // bottom-right
//		FVector2(0.0f, 1.0f), // bottom-left
//	};
//
//	for (int32 i = 0; i < positions.Num(); i++)
//	{
//		FVertex Vertex;
//		Vertex.Position = positions[i];
//		Vertex.Color = Color;
//		Vertex.UV = uvs[i];
//		PlaneMeshData.Vertices.Add(Vertex);
//	}
//
//	PlaneMeshData.Indices.Add(0);
//	PlaneMeshData.Indices.Add(2);
//	PlaneMeshData.Indices.Add(1);
//
//	PlaneMeshData.Indices.Add(0);
//	PlaneMeshData.Indices.Add(3);
//	PlaneMeshData.Indices.Add(2);
//
//	return PlaneMeshData;
//}

UStaticMeshData FGeometryGenerator::CreateCone(float Radius, float Height, int Segments, const FVector4& Color)
{
	UStaticMeshData ConeMeshData;

	TArray<FVertex>& Vertices = ConeMeshData.Vertices;
	TArray<uint32>& Indices = ConeMeshData.Indices;

	Vertices.Add({ { 0.0f, 0.0f, Height }, Color });

	float Slice = PI * 2.0f / Segments;

	for (int32 i = 0; i <= Segments; i++)
	{
		float angle = i * Slice;
		float x = -Radius * cosf(angle);
		float y = -Radius * sinf(angle);

		Vertices.Add({ { x, y, 0.0f }, Color });
	}


	for (int32 i = 0; i < Segments; i++)
	{
		uint32 bottomA = 1 + i;
		uint32 bottomB = 1 + i + 1;

		Indices.Add(0); 
		Indices.Add(bottomA); 
		Indices.Add(bottomB);
	}

	uint32 baseIndex = static_cast<uint32>(Vertices.Num());

	uint32 bottomCenterIdx = baseIndex;
	Vertices.Add({ { 0.0f, 0.0f, 0.0f }, Color });

	uint32 bottomRingStart = static_cast<uint32>(Vertices.Num());
	for (int32 i = 0; i <= Segments; i++)
	{
		float angle = i * Slice;
		float x = Radius * cosf(angle);
		float y = Radius * sinf(angle);
		Vertices.Add({ { x, y, 0.0f }, Color });
	}

	for (int32 i = 0; i < Segments; i++)
	{
		Indices.Add(bottomCenterIdx);
		Indices.Add(bottomRingStart + i + 1);
		Indices.Add(bottomRingStart + i);
	}

	return ConeMeshData;
}


//UStaticMeshData FGeometryGenerator::CreateCone(float Radius, float Height, int Segments, const FVector4& Color)
//{
//	UStaticMeshData ConeMeshData;
//
//	TArray<FVertex>& Vertices = ConeMeshData.Vertices;
//	TArray<uint32>& Indices = ConeMeshData.Indices;
//
//	// 꼭짓점 (측면 팬의 중심) - u는 애매하므로 중앙값 사용
//	Vertices.Add({ { 0.0f, 0.0f, Height }, Color, FVector2(0.5f, 0.0f) });
//
//	float Slice = PI * 2.0f / Segments;
//
//	for (int32 i = 0; i <= Segments; i++)
//	{
//		float angle = i * Slice;
//		float x = -Radius * cosf(angle);
//		float y = -Radius * sinf(angle);
//
//		float u = (float)i / Segments;
//		Vertices.Add({ { x, y, 0.0f }, Color, FVector2(u, 1.0f) });
//	}
//
//	for (int32 i = 0; i < Segments; i++)
//	{
//		uint32 bottomA = 1 + i;
//		uint32 bottomB = 1 + i + 1;
//
//		Indices.Add(0);
//		Indices.Add(bottomA);
//		Indices.Add(bottomB);
//	}
//
//	uint32 baseIndex = static_cast<uint32>(Vertices.Num());
//
//	uint32 bottomCenterIdx = baseIndex;
//	Vertices.Add({ { 0.0f, 0.0f, 0.0f }, Color, FVector2(0.5f, 0.5f) });
//
//	uint32 bottomRingStart = static_cast<uint32>(Vertices.Num());
//	for (int32 i = 0; i <= Segments; i++)
//	{
//		float angle = i * Slice;
//		float x = Radius * cosf(angle);
//		float y = Radius * sinf(angle);
//
//		float u = 0.5f + 0.5f * cosf(angle);
//		float v = 0.5f + 0.5f * sinf(angle);
//		Vertices.Add({ { x, y, 0.0f }, Color, FVector2(u, v) });
//	}
//
//	for (int32 i = 0; i < Segments; i++)
//	{
//		Indices.Add(bottomCenterIdx);
//		Indices.Add(bottomRingStart + i + 1);
//		Indices.Add(bottomRingStart + i);
//	}
//
//	return ConeMeshData;
//}
UStaticMeshData FGeometryGenerator::CreateCylinder(float Radius, float Height, int32 Segments, const FVector4& Color)
{
	UStaticMeshData CylinderMeshData;

	TArray<FVertex>& Vertices = CylinderMeshData.Vertices;
	TArray<uint32>& Indices = CylinderMeshData.Indices;

	float Slice = PI * 2.0f / Segments;
	float HalfHeight = Height / 2.0f;

	for (int32 i = 0; i <= Segments; i++)
	{
		float angle = i * Slice;
		float x = -Radius * cosf(angle);
		float y = -Radius * sinf(angle);

		Vertices.Add({ { x, y, -HalfHeight }, Color });
		Vertices.Add({ { x, y,  HalfHeight }, Color });
	}

	for (uint32 i = 0; i < Segments; i++)
	{
		uint32 bottomA = i * 2;
		uint32 topA = i * 2 + 1;
		uint32 bottomB = (i + 1) * 2;
		uint32 topB = (i + 1) * 2 + 1;

		Indices.Add(bottomA); Indices.Add(bottomB); Indices.Add(topA);
		Indices.Add(topA);    Indices.Add(bottomB); Indices.Add(topB);
	}

	uint32 baseIndex = static_cast<uint32>(Vertices.Num());

	uint32 bottomCenterIdx = baseIndex;
	Vertices.Add({ { 0.0f, 0.0f, -HalfHeight }, Color });

	uint32 topCenterIdx = baseIndex + 1;
	Vertices.Add({ { 0.0f, 0.0f, HalfHeight }, Color });

	uint32 bottomRingStart = static_cast<uint32>(Vertices.Num());
	for (uint32 i = 0; i <= Segments; i++)
	{
		float angle = i * Slice;
		float x = Radius * cosf(angle);
		float y = Radius * sinf(angle);
		Vertices.Add({ { x, y, -HalfHeight }, Color });
	}

	uint32 topRingStart = static_cast<uint32>(Vertices.Num());
	for (uint32 i = 0; i <= Segments; i++)
	{
		float angle = i * Slice;
		float x = Radius * cosf(angle);
		float y = Radius * sinf(angle);
		Vertices.Add({ { x, y, HalfHeight }, Color });
	}

	for (uint32 i = 0; i < Segments; i++)
	{
		Indices.Add(bottomCenterIdx);
		Indices.Add(bottomRingStart + i);
		Indices.Add(bottomRingStart + i + 1);
	}

	for (uint32 i = 0; i < Segments; i++)
	{
		Indices.Add(topCenterIdx);
		Indices.Add(topRingStart + i + 1);
		Indices.Add(topRingStart + i);
	}

	return CylinderMeshData;
}

//UStaticMeshData FGeometryGenerator::CreateCylinder(float Radius, float Height, int32 Segments, const FVector4& Color)
//{
//	UStaticMeshData CylinderMeshData;
//
//	TArray<FVertex>& Vertices = CylinderMeshData.Vertices;
//	TArray<uint32>& Indices = CylinderMeshData.Indices;
//
//	float Slice = PI * 2.0f / Segments;
//	float HalfHeight = Height / 2.0f;
//
//	for (int32 i = 0; i <= Segments; i++)
//	{
//		float angle = i * Slice;
//		float x = -Radius * cosf(angle);
//		float y = -Radius * sinf(angle);
//		float u = (float)i / Segments;
//
//		Vertices.Add({ { x, y, -HalfHeight }, Color, FVector2(u, 1.0f) });
//		Vertices.Add({ { x, y,  HalfHeight }, Color, FVector2(u, 0.0f) });
//	}
//
//	for (uint32 i = 0; i < Segments; i++)
//	{
//		uint32 bottomA = i * 2;
//		uint32 topA = i * 2 + 1;
//		uint32 bottomB = (i + 1) * 2;
//		uint32 topB = (i + 1) * 2 + 1;
//
//		Indices.Add(bottomA); Indices.Add(bottomB); Indices.Add(topA);
//		Indices.Add(topA);    Indices.Add(bottomB); Indices.Add(topB);
//	}
//
//	uint32 baseIndex = static_cast<uint32>(Vertices.Num());
//
//	uint32 bottomCenterIdx = baseIndex;
//	Vertices.Add({ { 0.0f, 0.0f, -HalfHeight }, Color, FVector2(0.5f, 0.5f) });
//
//	uint32 topCenterIdx = baseIndex + 1;
//	Vertices.Add({ { 0.0f, 0.0f, HalfHeight }, Color, FVector2(0.5f, 0.5f) });
//
//	uint32 bottomRingStart = static_cast<uint32>(Vertices.Num());
//	for (uint32 i = 0; i <= Segments; i++)
//	{
//		float angle = i * Slice;
//		float x = Radius * cosf(angle);
//		float y = Radius * sinf(angle);
//		float u = 0.5f + 0.5f * cosf(angle);
//		float v = 0.5f + 0.5f * sinf(angle);
//		Vertices.Add({ { x, y, -HalfHeight }, Color, FVector2(u, v) });
//	}
//
//	uint32 topRingStart = static_cast<uint32>(Vertices.Num());
//	for (uint32 i = 0; i <= Segments; i++)
//	{
//		float angle = i * Slice;
//		float x = Radius * cosf(angle);
//		float y = Radius * sinf(angle);
//		float u = 0.5f + 0.5f * cosf(angle);
//		float v = 0.5f + 0.5f * sinf(angle);
//		Vertices.Add({ { x, y, HalfHeight }, Color, FVector2(u, v) });
//	}
//
//	for (uint32 i = 0; i < Segments; i++)
//	{
//		Indices.Add(bottomCenterIdx);
//		Indices.Add(bottomRingStart + i);
//		Indices.Add(bottomRingStart + i + 1);
//	}
//
//	for (uint32 i = 0; i < Segments; i++)
//	{
//		Indices.Add(topCenterIdx);
//		Indices.Add(topRingStart + i + 1);
//		Indices.Add(topRingStart + i);
//	}
//
//	return CylinderMeshData;
//}

UStaticMeshData FGeometryGenerator::CreateArrow(float BodyRadius, float BodyHeight, float HeadRadius, float HeadHeight, int Segments, const FVector4& Color)
{

	UStaticMeshData Arrow = CreateCylinder(BodyRadius, BodyHeight, Segments, Color);
	Arrow.Translate(FVector(0.0f, 0.0f, BodyHeight * 0.5f));   // 밑면을 0으로

	UStaticMeshData Head = CreateCone(HeadRadius, HeadHeight, Segments, Color);
	Head.Translate(FVector(0.0f, 0.0f, BodyHeight));

	Arrow.Append(Head);
	return Arrow;
}

UStaticMeshData FGeometryGenerator::CreateScaleBar(float BodyRadius, float BodyLength, float HeadSize, float Segments, const FVector4& Color)
{
	UStaticMeshData Arrow = CreateCylinder(BodyRadius, BodyLength, Segments, Color);
	Arrow.Translate(FVector(0.0f, 0.0f, BodyLength * 0.5f));   // 밑면을 0으로

	UStaticMeshData Head = CreateCube(HeadSize, Color);
	Head.Translate(FVector(0.0f, 0.0f, BodyLength + HeadSize * 0.5f));

	Arrow.Append(Head);
	return Arrow;
}

UStaticMeshData FGeometryGenerator::CreateRing(float Radius, float TubeRadius, int Segments, int TubeSegments, const FVector4& Color)
{
	UStaticMeshData Data;

	// 정점 생성
	for (int i = 0; i <= Segments; ++i)
	{
		float theta = (float)i * 2.0f * PI / Segments;   // 큰 원을 도는 각도
		float cosTheta = cosf(theta);
		float sinTheta = sinf(theta);

		for (int j = 0; j <= TubeSegments; ++j)
		{
			float phi = (float)j * 2.0f * PI / TubeSegments;   // 단면 원을 도는 각도
			float cosPhi = cosf(phi);
			float sinPhi = sinf(phi);

			FVertex v;
			v.Position.X = (Radius + TubeRadius * cosPhi) * cosTheta;
			v.Position.Y = (Radius + TubeRadius * cosPhi) * sinTheta;
			v.Position.Z = TubeRadius * sinPhi;
			v.Color = Color;
			Data.Vertices.Add(v);
		}
	}

	int stride = TubeSegments + 1;
	for (int i = 0; i < Segments; ++i)
	{
		for (int j = 0; j < TubeSegments; ++j)
		{
			int a = i * stride + j;
			int b = a + 1;
			int c = (i + 1) * stride + j;
			int d = c + 1;

			Data.Indices.Add(a);
			Data.Indices.Add(c);
			Data.Indices.Add(b);

			Data.Indices.Add(b);
			Data.Indices.Add(c);
			Data.Indices.Add(d);
		}
	}

	return Data;
}

//UStaticMeshData FGeometryGenerator::CreateRing(float Radius, float TubeRadius, int Segments, int TubeSegments, const FVector4& Color)
//{
//	UStaticMeshData Data;
//
//	for (int i = 0; i <= Segments; ++i)
//	{
//		float theta = (float)i * 2.0f * PI / Segments;
//		float cosTheta = cosf(theta);
//		float sinTheta = sinf(theta);
//		float u = (float)i / Segments;
//
//		for (int j = 0; j <= TubeSegments; ++j)
//		{
//			float phi = (float)j * 2.0f * PI / TubeSegments;
//			float cosPhi = cosf(phi);
//			float sinPhi = sinf(phi);
//			float v = (float)j / TubeSegments;
//
//			FVertex v_;
//			v_.Position.X = (Radius + TubeRadius * cosPhi) * cosTheta;
//			v_.Position.Y = (Radius + TubeRadius * cosPhi) * sinTheta;
//			v_.Position.Z = TubeRadius * sinPhi;
//			v_.Color = Color;
//			v_.UV = FVector2(u, v);
//			Data.Vertices.Add(v_);
//		}
//	}
//
//	int stride = TubeSegments + 1;
//	for (int i = 0; i < Segments; ++i)
//	{
//		for (int j = 0; j < TubeSegments; ++j)
//		{
//			int a = i * stride + j;
//			int b = a + 1;
//			int c = (i + 1) * stride + j;
//			int d = c + 1;
//
//			Data.Indices.Add(a);
//			Data.Indices.Add(c);
//			Data.Indices.Add(b);
//
//			Data.Indices.Add(b);
//			Data.Indices.Add(c);
//			Data.Indices.Add(d);
//		}
//	}
//
//	return Data;
//}

UStaticMeshData FGeometryGenerator::CreateCube(float Size, const FVector4& Color)
{
	float HalfWidth = Size / 2.0f;
	float HalfHeight = Size / 2.0f;
	float HalfDepth = Size / 2.0f;

	TArray<FVector> positions =
	{
		//Front
		FVector(-HalfWidth, -HalfHeight, -HalfDepth),
		FVector(-HalfWidth, HalfHeight, -HalfDepth),
		FVector(HalfWidth, HalfHeight, -HalfDepth),
		FVector(HalfWidth, -HalfHeight, -HalfDepth),
		//Back
		FVector(-HalfWidth, -HalfHeight, HalfDepth),
		FVector(-HalfWidth, HalfHeight, HalfDepth),
		FVector(HalfWidth, HalfHeight, HalfDepth),
		FVector(HalfWidth, -HalfHeight, HalfDepth)
	};

	TArray<FVector> normals =
	{
		FVector(0.0f,0.0f,-1.0f),//Front
		FVector(0.0f,0.0f,1.0f), //Back
		FVector(0.0f,1.0f,0.0f), //Top
		FVector(0.0f,-1.0f,0.0f),//Bottom
		FVector(-1.0f,0.0f,0.0f),//Left
		FVector(1.0f,0.0f,0.0f), //Right
	};

	TArray<FVector> colors =
	{
		FVector(1.0f,0.0f,0.6f),//Front
		FVector(1.0f,0.0f,0.6f), //Back
		FVector(0.6f,1.0f,0.0f), //Top
		FVector(0.6f,1.0f,0.0f),//Bottom
		FVector(0.0f,0.6f,1.0f),//Left
		FVector(0.0f,0.6f,1.0f), //Right
	};

	TArray<TArray<uint32>> cubeFaces =
	{
		{ 0,1,2,3 }, //Front
		{ 7,6,5,4 }, //Back
		{ 1,5,6,2 }, //Top
		{ 4,0,3,7 }, //Bottom
		{ 4,5,1,0 }, //Left
		{ 3,2,6,7 }, //Right
	};

	UStaticMeshData CubeMeshData;

	for (int i = 0; i < 6; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			FVertex Vertex;
			Vertex.Position = positions[cubeFaces[i][j]];
			Vertex.Color = FVector4(colors[i], 1.0f);
			CubeMeshData.Vertices.Add(Vertex);
		}

		uint32 BaseVertexIndex = i * 4;

		CubeMeshData.Indices.Add(BaseVertexIndex + 0);
		CubeMeshData.Indices.Add(BaseVertexIndex + 1);
		CubeMeshData.Indices.Add(BaseVertexIndex + 2);

		CubeMeshData.Indices.Add(BaseVertexIndex + 0);
		CubeMeshData.Indices.Add(BaseVertexIndex + 2);
		CubeMeshData.Indices.Add(BaseVertexIndex + 3);
	}
	return CubeMeshData;
}

//UStaticMeshData FGeometryGenerator::CreateCube(float Size, const FVector4& Color)
//{
//	float HalfWidth = Size / 2.0f;
//	float HalfHeight = Size / 2.0f;
//	float HalfDepth = Size / 2.0f;
//
//	TArray<FVector> positions =
//	{
//		FVector(-HalfWidth, -HalfHeight, -HalfDepth),
//		FVector(-HalfWidth, HalfHeight, -HalfDepth),
//		FVector(HalfWidth, HalfHeight, -HalfDepth),
//		FVector(HalfWidth, -HalfHeight, -HalfDepth),
//		FVector(-HalfWidth, -HalfHeight, HalfDepth),
//		FVector(-HalfWidth, HalfHeight, HalfDepth),
//		FVector(HalfWidth, HalfHeight, HalfDepth),
//		FVector(HalfWidth, -HalfHeight, HalfDepth)
//	};
//
//	TArray<FVector> colors =
//	{
//		FVector(1.0f,0.0f,0.6f),
//		FVector(1.0f,0.0f,0.6f),
//		FVector(0.6f,1.0f,0.0f),
//		FVector(0.6f,1.0f,0.0f),
//		FVector(0.0f,0.6f,1.0f),
//		FVector(0.0f,0.6f,1.0f),
//	};
//
//	TArray<TArray<uint32>> cubeFaces =
//	{
//		{ 0,1,2,3 },
//		{ 7,6,5,4 },
//		{ 1,5,6,2 },
//		{ 4,0,3,7 },
//		{ 4,5,1,0 },
//		{ 3,2,6,7 },
//	};
//
//	// 각 면 공통 UV (0,0)-(1,0)-(1,1)-(0,1) 순서로 4버텍스에 매핑
//	TArray<FVector2> faceUVs =
//	{
//		FVector2(0.0f, 1.0f),
//		FVector2(0.0f, 0.0f),
//		FVector2(1.0f, 0.0f),
//		FVector2(1.0f, 1.0f),
//	};
//
//	UStaticMeshData CubeMeshData;
//
//	for (int i = 0; i < 6; i++)
//	{
//		for (int j = 0; j < 4; j++)
//		{
//			FVertex Vertex;
//			Vertex.Position = positions[cubeFaces[i][j]];
//			Vertex.Color = FVector4(colors[i], 1.0f);
//			Vertex.UV = faceUVs[j];
//			CubeMeshData.Vertices.Add(Vertex);
//		}
//
//		uint32 BaseVertexIndex = i * 4;
//
//		CubeMeshData.Indices.Add(BaseVertexIndex + 0);
//		CubeMeshData.Indices.Add(BaseVertexIndex + 1);
//		CubeMeshData.Indices.Add(BaseVertexIndex + 2);
//
//		CubeMeshData.Indices.Add(BaseVertexIndex + 0);
//		CubeMeshData.Indices.Add(BaseVertexIndex + 2);
//		CubeMeshData.Indices.Add(BaseVertexIndex + 3);
//	}
//	return CubeMeshData;
//}

UStaticMeshData FGeometryGenerator::CreateSphere(float Radius, uint32 NumSlices, uint32 NumStacks, const FVector4& Color)
{
	UStaticMeshData SphereMeshData;

	const float SliceStep = PI * 2.0f / NumSlices;
	const float StackStep = PI / NumStacks;

	// i = 0 → +Z 극, i = NumStacks → -Z 극
	for (uint32 i = 0; i <= NumStacks; i++)
	{
		const float Phi = i * StackStep;        // 0 ~ PI, +Z에서 내려감
		const float z = Radius * cosf(Phi);   // Z-up
		const float r = Radius * sinf(Phi);   // 해당 높이에서의 원 반지름

		for (uint32 j = 0; j <= NumSlices; j++)
		{
			const float Theta = j * SliceStep;

			FVertex vertex;
			vertex.Position = FVector(r * cosf(Theta), r * sinf(Theta), z);
			vertex.Color = Color;
			// vertex.Normal = Normalized(vertex.Position);
			// vertex.UV     = FVector2(float(j) / NumSlices, float(i) / NumStacks);

			SphereMeshData.Vertices.Add(vertex);
		}
	}

	for (uint32 i = 0; i < NumStacks; i++)
	{
		const uint32 offset = (NumSlices + 1) * i;
		for (uint32 j = 0; j < NumSlices; j++)
		{
			const uint32 TopL = offset + j;
			const uint32 TopR = offset + j + 1;
			const uint32 BotL = offset + NumSlices + 1 + j;
			const uint32 BotR = offset + NumSlices + 1 + j + 1;

			// CW (바깥에서 봤을 때)
			SphereMeshData.Indices.Add(TopL);
			SphereMeshData.Indices.Add(BotL);
			SphereMeshData.Indices.Add(BotR);

			SphereMeshData.Indices.Add(TopL);
			SphereMeshData.Indices.Add(BotR);
			SphereMeshData.Indices.Add(TopR);
		}
	}
	return SphereMeshData;
}

//UStaticMeshData FGeometryGenerator::CreateSphere(float Radius, uint32 NumSlices, uint32 NumStacks, const FVector4& Color)
//{
//	UStaticMeshData SphereMeshData;
//
//	const float SliceStep = PI * 2.0f / NumSlices;
//	const float StackStep = PI / NumStacks;
//
//	for (uint32 i = 0; i <= NumStacks; i++)
//	{
//		const float Phi = i * StackStep;
//		const float z = Radius * cosf(Phi);
//		const float r = Radius * sinf(Phi);
//
//		for (uint32 j = 0; j <= NumSlices; j++)
//		{
//			const float Theta = j * SliceStep;
//
//			FVertex vertex;
//			vertex.Position = FVector(r * cosf(Theta), r * sinf(Theta), z);
//			vertex.Color = Color;
//			vertex.UV = FVector2(float(j) / NumSlices, float(i) / NumStacks);
//
//			SphereMeshData.Vertices.Add(vertex);
//		}
//	}
//
//	for (uint32 i = 0; i < NumStacks; i++)
//	{
//		const uint32 offset = (NumSlices + 1) * i;
//		for (uint32 j = 0; j < NumSlices; j++)
//		{
//			const uint32 TopL = offset + j;
//			const uint32 TopR = offset + j + 1;
//			const uint32 BotL = offset + NumSlices + 1 + j;
//			const uint32 BotR = offset + NumSlices + 1 + j + 1;
//
//			SphereMeshData.Indices.Add(TopL);
//			SphereMeshData.Indices.Add(BotL);
//			SphereMeshData.Indices.Add(BotR);
//
//			SphereMeshData.Indices.Add(TopL);
//			SphereMeshData.Indices.Add(BotR);
//			SphereMeshData.Indices.Add(TopR);
//		}
//	}
//	return SphereMeshData;
//}

UStaticMeshData FGeometryGenerator::GetMeshData(const FString& InName)
{
	if (InName == FString("Cone"))
	{
		return CreateCone(1.0f, 1.0f, 20, FVector4(1.0f, 1.0f, 1.0f, 1.0f));
	}
	else if (InName == FString("Cube"))
	{
		return CreateCube(1.0f);
	}
	else if (InName == FString("Cylinder"))
	{
		return CreateCylinder(1.0f, 1.0f, 20, FVector4(1.0f, 1.0f, 1.0f, 1.0f));
	}
	else if (InName == FString("Sphere"))
	{
		return CreateSphere(1.0f, 20, 10, FVector4(1.0f, 1.0f, 1.0f, 1.0f));
	}
	else if (InName == FString("Plane"))
	{
		return CreatePlane(1.0f, FVector4(1.0f, 1.0f, 1.0f, 1.0f));
	}


	return UStaticMeshData();
}
