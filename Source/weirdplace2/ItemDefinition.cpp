#include "ItemDefinition.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

FInventoryItemData UItemDefinition::ToInventoryItemData() const
{
	FInventoryItemData Data;
	Data.ItemID = ItemID;
	Data.Mesh = Mesh;
	Data.Scale = Scale;
	Data.Rotation = HeldRotation;

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
