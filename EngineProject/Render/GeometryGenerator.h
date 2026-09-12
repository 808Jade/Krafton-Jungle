#pragma once

#include "Render/Vertex.h"
#include "Engine/Engine.h"
#include "StaticMeshData.h"



class FGeometryGenerator
{
public:
	static FStaticMeshData CreateTextQuad(const FVector& Start, const FVector& End, const FVector4& Color);
	static FStaticMeshData CreateLine(const FVector& Start, const FVector& End, const FVector4& Color);
	static FStaticMeshData CreatePlane(float Size, const FVector4& Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f));
	static FStaticMeshData CreateAxis();

	static FStaticMeshData CreateCone(float Radius, float Height, int Segments, const FVector4& Color);
	static FStaticMeshData CreateCylinder(float Radius, float Height, int Segments, const FVector4& Color);
	static FStaticMeshData CreateArrow(float BodyRadius, float BodyHeight, float HeadRadius, float HeadHeight, int Segments, const FVector4& Color); // Cylinder + Cone 합성
	static FStaticMeshData CreateScaleBar(float BodyRadius, float BodyLength, float HeadSize, float Segments, const FVector4& Color); // Cylinder + Cone 합성
	static FStaticMeshData CreateRing(float Radius, float TubeRadius, int Segments, int TubeSegments, const FVector4& Color);
	static FStaticMeshData CreateCube(float Size, const FVector4& Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f));

	static FStaticMeshData CreateSphere(float _radius, uint32 _numSlices, uint32 _numStacks, const FVector4& Color);

	static FStaticMeshData GetMeshData(const FString& InName);
};