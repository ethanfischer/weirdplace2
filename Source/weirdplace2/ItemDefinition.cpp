#include "ItemDefinition.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"

namespace
{
	FString DefinitionPackage(FName ItemID)
	{
		return FString::Printf(TEXT("/Game/Inventory/DA_%s"), *ItemID.ToString());
	}
}

bool UItemDefinition::Exists(FName ItemID)
{
	return !ItemID.IsNone() && FPackageName::DoesPackageExist(DefinitionPackage(ItemID));
}

UItemDefinition* UItemDefinition::Find(FName ItemID)
{
	if (!Exists(ItemID))
	{
		return nullptr;
	}
	const FString Package = DefinitionPackage(ItemID);
	const FString ObjectPath = FString::Printf(TEXT("%s.DA_%s"), *Package, *ItemID.ToString());
	UItemDefinition* Def = LoadObject<UItemDefinition>(nullptr, *ObjectPath);
	if (!Def)
	{
		UE_LOG(LogTemp, Error, TEXT("UItemDefinition::Find: package %s exists but %s isn't a UItemDefinition"), *Package, *ObjectPath);
	}
	return Def;
}

FInventoryItemData UItemDefinition::ToInventoryItemData() const
{
	FInventoryItemData Data;
	Data.ItemID = ItemID;
	Data.Mesh = Mesh;
	Data.Scale = Scale;

	if (MaterialOverrides.Num() > 0)
	{
		Data.Materials = MaterialOverrides;
	}
	else if (Mesh)
	{
		const TArray<FStaticMaterial>& StaticMats = Mesh->GetStaticMaterials();
		Data.Materials.Reserve(StaticMats.Num());
		for (int32 i = 0; i < StaticMats.Num(); i++)
		{
			Data.Materials.Add(Mesh->GetMaterial(i));
		}
	}

	return Data;
}
