#include "ItemGlow.h"

#include "Materials/MaterialInterface.h"

UMaterialInterface* ItemGlow::GetItemGlowMaterial()
{
	static const TCHAR* const Path = TEXT("/Game/CreatedMaterials/M_ItemDarkGlow.M_ItemDarkGlow");
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, Path);
	if (!Material)
	{
		UE_LOG(LogTemp, Error, TEXT("ItemGlow: glow material not found at %s"), Path);
	}
	return Material;
}
