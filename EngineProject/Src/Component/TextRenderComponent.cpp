#include "EnginePCH.h"
#include "TextRenderComponent.h"

#include "Asset/AssetManager.h"
#include "Engine/World.h"

UTextRenderComponent::UTextRenderComponent()
{
}

UTextRenderComponent::~UTextRenderComponent()
{
}

void UTextRenderComponent::BeginPlay()
{
	Super::BeginPlay();

	Mesh = UAssetManager::GetAssetByPath<UStaticMesh>("ParticleQuad");
	Material = UAssetManager::GetAssetByPath<UMaterial>("SubUVMaterial");
	
	if (!Font)
	{
		Font = UAssetManager::GetAssetByPath<UFont>("Fonts/atlas.json");
	}

	GetOwner()->GetWorld()->AddTextComponent(this);
}

void UTextRenderComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
}
