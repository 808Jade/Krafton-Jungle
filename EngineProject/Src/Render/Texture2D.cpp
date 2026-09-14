#include "EnginePCH.h"
#include "Texture2D.h"

#include "RenderCommand.h"

UTexture2D::UTexture2D()
{

}
UTexture2D::~UTexture2D()
{
}

UTexture2D* UTexture2D::CreateFromFile(const FString& FilePath)
{
	return RenderCommand::CreateTexture2DFromFile(FilePath);
}
