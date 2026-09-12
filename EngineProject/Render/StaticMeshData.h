#pragma once

struct FBox
{
	FVector Min;
	FVector Max;

	FBox GetWorldAABB(const FMatrix& M)
	{
		FVector Center = (Min + Max) * 0.5f;
		FVector Extent = (Max - Min) * 0.5f;

		// 중심은 그냥 변환
		FVector4 C = FVector4(Center, 1.0f) * M;

		// 범위는 회전 부분의 절댓값으로 변환
		FVector E;
		E.X = Extent.X * fabsf(M.M[0][0]) + Extent.Y * fabsf(M.M[1][0]) + Extent.Z * fabsf(M.M[2][0]);
		E.Y = Extent.X * fabsf(M.M[0][1]) + Extent.Y * fabsf(M.M[1][1]) + Extent.Z * fabsf(M.M[2][1]);
		E.Z = Extent.X * fabsf(M.M[0][2]) + Extent.Y * fabsf(M.M[1][2]) + Extent.Z * fabsf(M.M[2][2]);

		return FBox{ FVector(C.X, C.Y, C.Z) - E, FVector(C.X, C.Y, C.Z) + E };
	}
};

struct FStaticMeshData
{
	TArray<FVertex> Vertices;
	TArray<uint32> Indices;

	FBox AABB;

	FBox GetAABB() const
	{
		FBox Box;

		if (Vertices.Num() == 0)
		{
			Box.Min = { 0.0f, 0.0f, 0.0f };
			Box.Max = { 0.0f, 0.0f, 0.0f };

			return Box;
		}

		FVector Min{ FLT_MAX, FLT_MAX, FLT_MAX };
		FVector Max{ -FLT_MAX, -FLT_MAX, -FLT_MAX };
		for (const FVertex& Vertex : Vertices)
		{
			Min.X = fmin(Min.X, Vertex.Position.X);
			Min.Y = fmin(Min.Y, Vertex.Position.Y);
			Min.Z = fmin(Min.Z, Vertex.Position.Z);

			Max.X = fmax(Max.X, Vertex.Position.X);
			Max.Y = fmax(Max.Y, Vertex.Position.Y);
			Max.Z = fmax(Max.Z, Vertex.Position.Z);
		}
		Box.Min = Min;
		Box.Max = Max;

		return Box;
	}


	void Append(const FStaticMeshData& Other)
	{
		uint32 Base = (uint32)Vertices.Num();

		Vertices.Append(Other.Vertices);

		for (uint32 i : Other.Indices)
			Indices.Add(Base + i);
	}

	void Translate(const FVector& Offset)
	{
		for (auto& V : Vertices)
			V.Position += Offset;
	}
};