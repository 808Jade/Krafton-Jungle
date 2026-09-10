#pragma once

#include "Render/Vertex.h"

struct FMeshData
{
	TArray<FVertex> Vertices;
	TArray<uint32> Indices;

	void GetAABB(FVector& OutBoxMin, FVector& OutBoxMax) const
	{
		if (Vertices.size() == 0)
		{
			OutBoxMax = { 0.0f, 0.0f, 0.0f };
			OutBoxMin = { 0.0f, 0.0f, 0.0f };

			return;
		}

		FVector Min{ FLT_MAX, FLT_MAX, FLT_MAX };
		FVector Max{ -FLT_MAX, -FLT_MAX, -FLT_MAX };
		for (const FVertex& vertex : Vertices)
		{
			Min.X = fmin(Min.X, vertex.Position.X);
			Min.Y = fmin(Min.Y, vertex.Position.Y);
			Min.Z = fmin(Min.Z, vertex.Position.Z);

			Max.X = fmax(Max.X, vertex.Position.X);
			Max.Y = fmax(Max.Y, vertex.Position.Y);
			Max.Z = fmax(Max.Z, vertex.Position.Z);
		}
		OutBoxMin = Min;
		OutBoxMax = Max;
	}

	void GetWorldAABB(FVector& OutBoxMin, FVector& OutBoxMax, const FMatrix& WorldMatrix) const
	{
		FVector BoxMin, BoxMax;
		GetAABB(BoxMin, BoxMax);

		TArray<FVector> Corners = 
		{
			{BoxMin.X, BoxMin.Y, BoxMin.Z},
			{BoxMin.X, BoxMin.Y, BoxMax.Z},
			{BoxMin.X, BoxMax.Y, BoxMin.Z},
			{BoxMin.X, BoxMax.Y, BoxMax.Z},
			{BoxMax.X, BoxMin.Y, BoxMin.Z},
			{BoxMax.X, BoxMin.Y, BoxMax.Z},
			{BoxMax.X, BoxMax.Y, BoxMin.Z},
			{BoxMax.X, BoxMax.Y, BoxMax.Z}
		};

		FVector Min{ FLT_MAX, FLT_MAX, FLT_MAX };
		FVector Max{ -FLT_MAX, -FLT_MAX, -FLT_MAX };
		for (FVector& vertex : Corners)
		{
			FVector4 WorldBoxVec4 = FVector4(vertex, 1.0f) * WorldMatrix;
			vertex.X = WorldBoxVec4.X;
			vertex.Y = WorldBoxVec4.Y;
			vertex.Z = WorldBoxVec4.Z;

			Min.X = fmin(Min.X, vertex.X);
			Min.Y = fmin(Min.Y, vertex.Y);
			Min.Z = fmin(Min.Z, vertex.Z);

			Max.X = fmax(Max.X, vertex.X);
			Max.Y = fmax(Max.Y, vertex.Y);
			Max.Z = fmax(Max.Z, vertex.Z);
		}

		OutBoxMin = Min;
		OutBoxMax = Max;
	}

	void Append(const FMeshData& Other)
	{
		uint32 Base = (uint32)Vertices.size();
		Vertices.insert(Vertices.end(), Other.Vertices.begin(), Other.Vertices.end());
		for (uint32 i : Other.Indices)
			Indices.push_back(Base + i);
	}

	void Translate(const FVector& Offset)
	{
		for (auto& V : Vertices)
			V.Position += Offset;
	}
};

class FGeometryGenerator
{
public:
	static FMeshData CreateLine(const FVector& Start, const FVector& End, const FVector4& Color);
	static FMeshData CreatePlane(float Size, const FVector4& Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f));
	static FMeshData CreateAxis();

	static FMeshData CreateCone(float Radius, float Height, int Segments, const FVector4& Color);
	static FMeshData CreateCylinder(float Radius, float Height, int Segments, const FVector4& Color);
	static FMeshData CreateArrow(float BodyRadius, float BodyHeight, float HeadRadius, float HeadHeight, int Segments, const FVector4& Color); // Cylinder + Cone 합성
	static FMeshData CreateScaleBar(float BodyRadius, float BodyLength, float HeadSize, float Segments, const FVector4& Color); // Cylinder + Cone 합성
	static FMeshData CreateRing(float Radius, float TubeRadius, int Segments, int TubeSegments, const FVector4& Color);
	static FMeshData CreateCube(float Size, const FVector4& Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f));

	static FMeshData CreateSphere(float _radius, uint32 _numSlices, uint32 _numStacks, const FVector4& Color);

	static FMeshData GetMeshData(const FString& InName);
};