//#pragma once
//
//#include "../Core/Types.h"
//#include "EngineMath.h"
//
//template<typename T>
//struct TVector {
//
//	union
//	{
//		T V[3];
//		struct
//		{
//			T X;
//			T Y;
//			T Z;
//		};
//	};
//
//public:
//	/* Constructor */
//	TVector()
//	{
//		X = T(0);
//		Y = T(0);
//		Z = T(0);
//	}
//	TVector(T x, T y, T z)
//	{
//		X = x;
//		Y = y;
//		Z = z;
//	}
//	explicit TVector(T f)
//	{
//		X = f;
//		Y = f;
//		Z = f;
//	}
//	TVector(const TVector& V1)
//	{
//		X = V1.X;
//		Y = V1.Y;
//		Z = V1.Z;
//	}
//
//public:
//	/* Public Functions */
//	void Set(T x, T y, T z)
//	{
//		X = x;
//		Y = y;
//		Z = z;
//	}
//
//	T Size() const // 길이 반환
//	{
//		T sum = X * X + Y * Y + Z * Z;
//		return sqrt(sum);
//	}
//
//	T Length() const // ==  size()
//	{
//		T sum = X * X + Y * Y + Z * Z;
//		return sqrt(sum);
//	}
//	
//	T Dot(const TVector<T>& V1) const
//	{
//		return X * V1.X + Y * V1.Y + Z * V1.Z;
//	}
//
//	TVector<T> Cross(const TVector<T>& V1) const
//	{
//		return TVector<T>(Y * V1.Z - Z * V1.Y, Z * V1.X - X * V1.Z, X * V1.Y - Y * V1.X);
//	}
//
//	TVector<T> GetAbs() const
//	{
//		T AbsX = X;
//		T AbsY = Y;
//		T AbsZ = Z;
//		if (X < 0) AbsX = -X;
//		if (Y < 0) AbsY = -Y;
//		if (Z < 0) AbsZ = -Z;
//		return FVector(AbsX, AbsY, AbsZ);
//	}
//
//	TVector<T> Normalize() const
//	{
//		T size = Size();
//
//		if (!FMath::IsNearlyZero(size))
//		{
//			return FVector(X / size, Y / size, Z / size);
//		}
//
//		return *this;
//	}
//
//
///* operator */
//
//	TVector<T> operator - ()
//	{
//		return TVector<T>(-X, -Y, -Z);
//	}
//
//	TVector<T>& operator = (const TVector<T>& V1)
//	{
//		X = V1.X;
//		Y = V1.Y;
//		Z = V1.Z;
//		return *this;
//	}
//
//	TVector<T> operator - (const TVector<T>& V1) const
//	{
//		return TVector<T>(X - V1.X, Y - V1.Y, Z - V1.Z);
//	}
//
//	TVector<T>& operator -= (const TVector<T>& V1)
//	{
//		X -= V1.X;
//		Y -= V1.Y;
//		Z -= V1.Z;
//		return *this;
//	}
//
//	TVector<T> operator + (const TVector<T>& V1) const
//	{
//		return TVector<T>(X + V1.X, Y + V1.Y, Z + V1.Z);
//	}
//
//
//	TVector<T>& operator += (const TVector<T>& V1)
//	{
//		X += V1.X;
//		Y += V1.Y;
//		Z += V1.Z;
//		return *this;
//	}
//
//	TVector<T> operator * (const TVector<T>& V1) const
//	{
//		return TVector<T>(X * V1.X, Y * V1.Y, Z * V1.Z);
//	}
//
//	TVector<T> operator * (const T& f) const
//	{
//		return TVector<T>(X * f, Y * f, Z * f);
//	}
//
//
//	TVector<T>& operator *= (const TVector<T>& V1)
//	{
//		X *= V1.X;
//		Y *= V1.Y;
//		Z *= V1.Z;
//		return *this;
//	}
//
//	TVector<T>& operator *= (const T& f)
//	{
//		X *= f;
//		Y *= f;
//		Z *= f;
//		return *this;
//	}
//
//	TVector<T> operator / (const TVector<T>& V1) const
//	{
//		assert(V1.X != T(0) || V1.Y != T(0) || V1.Z != T(0));
//		return TVector<T>(X / V1.X, Y / V1.Y, Z / V1.Z);
//	}
//
//
//	TVector<T> operator / (const T& f) const
//	{
//		return TVector<T>(X / f, Y / f, Z / f);
//	}
//
//	TVector<T>& operator /= (const TVector<T>& V1)
//	{
//		const T InvX = T(1) / V1.X;
//		const T InvY = T(1) / V1.Y;
//		const T InvZ = T(1) / V1.Z;
//
//		X *= InvX;
//		Y *= InvY;
//		Z *= InvZ;
//
//		return *this;
//	}
//
//	TVector<T>& operator /= (const T& f)
//	{
//		const T InvF = T(1) / f;
//		X *= InvF;
//		Y *= InvF;
//		Z *= InvF;
//
//		return *this;
//	}
//
//	TVector<T> operator ^ (const TVector<T>& V1) const
//	{
//		return this->Cross(V1);
//	}
//
//	bool operator == (const TVector<T>& V1) const
//	{
//		return (X == V1.X) && (Y == V1.Y) && (Z == V1.Z);
//	}
//
//	bool operator != (const TVector<T>& V1) const
//	{
//		return !(*this == V1);
//	}
//
//
//	T operator[] (int32 Index) const
//	{
//		return V[Index];
//	}
//
//	T& operator[] (int32 Index)
//	{
//		return V[Index];
//	}
//
//
//	/* Static */
//	static T DotProduct(const TVector<T>& V1, const TVector<T>& V2)
//	{
//		return V1.Dot(V2);
//	}
//	static TVector<T> CrossProduct(const TVector<T>& V1, const TVector<T>& V2)
//	{
//		return V1.Cross(V2);
//	}
//
//	static T Distance(const TVector<T>& V1, const TVector<T>& V2) 
//	{
//		T dX = V1.X - V2.X;
//		T dY = V1.Y - V2.Y;
//		T dZ = V1.Z - V2.Z;
//		T sum = dX * dX + dY * dY + dZ * dZ;
//
//		return sqrt(sum);
//	}
//
//
//};