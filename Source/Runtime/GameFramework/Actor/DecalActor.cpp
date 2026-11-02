#include "EnginePCH.h"
#include "DecalActor.h"
#include "Asset/AssetManager.h"

ADecalActor::ADecalActor()
{
	DecalComponent = CreateDefaultSubobject<UDecalComponent>("UDecalComponent");
	SetRootComponent(DecalComponent);

	BillboardComponent = CreateDefaultSubobject<UBillboardComponent>("UBillboardComponent");
	BillboardComponent->SetEditorIcon(true);
	BillboardComponent->SetHiddenInDetails(true);
	BillboardComponent->SetIconSize(2.0f);
	BillboardComponent->SetupAttachment(DecalComponent);
	BillboardComponent->SetHiddenInDetails(true);
	BillboardComponent->bCastShadow = false;
	BillboardComponent->bReceiveShadow = false;

	UMaterial* Base = UAssetManager::GetAssetByPath<UMaterial>("BillboardMaterial");
	UTexture2D* Icon = UAssetManager::Get().LoadTexture("Assets/Icons/Decal.png");
	if (Base && Icon)
	{
		UMaterial* IconMat = UMaterial::CreateInstance(Base);
		IconMat->Textures[0] = Icon;
		BillboardComponent->SetMaterial(0, IconMat);
	}
}