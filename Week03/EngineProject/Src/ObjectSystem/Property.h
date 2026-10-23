#pragma once

#include "Core/EngineString.h"
#include "Math/EngineMath.h"

class UMaterial;
class UFont;

enum class EPropertyType { Unknown, Float, Int, String, Bool, Vector, Vector4, Color, Material, Font };

template <typename T>
constexpr EPropertyType GetPropertyType()
{
    static_assert(sizeof(T) == 0, "Not Valid Type");
    return EPropertyType::Unknown;
}

#define DEFINE_PROPERTY_TYPE(CppType, EnumValue)                    \
    template <> constexpr EPropertyType GetPropertyType<CppType>()  \
    { return EPropertyType::EnumValue; }

DEFINE_PROPERTY_TYPE(int, Int)
DEFINE_PROPERTY_TYPE(uint32, Int)
DEFINE_PROPERTY_TYPE(float, Float)
DEFINE_PROPERTY_TYPE(bool, Bool)
DEFINE_PROPERTY_TYPE(FString, String)
DEFINE_PROPERTY_TYPE(FVector, Vector)
DEFINE_PROPERTY_TYPE(FVector4, Vector4)
DEFINE_PROPERTY_TYPE(UMaterial*, Material)
DEFINE_PROPERTY_TYPE(UFont*, Font)

struct FProperty
{
    FString Name;
    EPropertyType Type;
    size_t Offset;
    size_t Size;
};