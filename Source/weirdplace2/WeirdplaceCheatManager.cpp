#include "WeirdplaceCheatManager.h"

#include "StorySubsystem.h"
#include "MyCharacter.h"
#include "Inventory.h"
#include "InventoryUIComponent.h"
#include "ItemDefinition.h"
#include "Seneca.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"

void UWeirdplaceCheatManager::SkipTo(const FString& BeatName)
{
	APlayerController* PC = GetOuterAPlayerController();
	UWorld* World = PC ? PC->GetWorld() : nullptr;
	UStorySubsystem* Story = World ? World->GetSubsystem<UStorySubsystem>() : nullptr;
	if (!Story)
	{
		UE_LOG(LogTemp, Warning, TEXT("SkipTo - no UStorySubsystem"));
		return;
	}

	// No beat given: list one command per story beat (enum-driven, stays in sync).
	if (BeatName.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("SkipTo <beat> - story beats:"));
		if (GEngine) { GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Yellow, TEXT("SkipTo <beat>:")); }
		for (const FString& Name : UStorySubsystem::GetBeatDisplayNames())
		{
			const FString Line = FString::Printf(TEXT("  SkipTo %s"), *Name);
			UE_LOG(LogTemp, Warning, TEXT("%s"), *Line);
			if (GEngine) { GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Yellow, Line); }
		}
		return;
	}

	EStoryFlag Target;
	if (!UStorySubsystem::ResolveBeat(BeatName, Target))
	{
		UE_LOG(LogTemp, Warning, TEXT("SkipTo - unknown beat '%s'. Try: KeyBroke | TornadoWarning | Telephone"), *BeatName);
		return;
	}

	Story->SkipToBeat(Target);

	UE_LOG(LogTemp, Warning, TEXT("SkipTo '%s' done"), *BeatName);
}

void UWeirdplaceCheatManager::SkipToSmoking()
{
	APlayerController* PC = GetOuterAPlayerController();
	UWorld* World = PC ? PC->GetWorld() : nullptr;
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("SkipToSmoking - no world"));
		return;
	}

	TArray<AActor*> Senecas;
	UGameplayStatics::GetAllActorsOfClass(World, ASeneca::StaticClass(), Senecas);
	if (Senecas.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("SkipToSmoking - no ASeneca in world"));
		return;
	}
	Cast<ASeneca>(Senecas[0])->ForceSmokingAppearance();
}

void UWeirdplaceCheatManager::GiveItem(const FString& Name)
{
	if (Name.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("GiveItem - usage: GiveItem <Name> (e.g. 'GiveItem Key')"));
		return;
	}

	APlayerController* PC = GetOuterAPlayerController();
	AMyCharacter* Character = PC ? Cast<AMyCharacter>(PC->GetPawn()) : nullptr;
	if (!Character)
	{
		UE_LOG(LogTemp, Error, TEXT("GiveItem - no AMyCharacter pawn"));
		return;
	}

	UInventoryComponent* Inventory = Character->GetInventoryComponent();
	if (!Inventory)
	{
		UE_LOG(LogTemp, Error, TEXT("GiveItem - no InventoryComponent"));
		return;
	}

	const FString AssetPath = FString::Printf(TEXT("/Game/Inventory/DA_%s.DA_%s"), *Name, *Name);
	UItemDefinition* Def = LoadObject<UItemDefinition>(nullptr, *AssetPath);
	if (!Def)
	{
		UE_LOG(LogTemp, Error, TEXT("GiveItem - no UItemDefinition at %s"), *AssetPath);
		return;
	}

	Inventory->AddItemWithData(Def->ToInventoryItemData());
	UE_LOG(LogTemp, Display, TEXT("GiveItem - granted '%s' (ItemID=%s)"), *Name, *Def->ItemID.ToString());
}

void UWeirdplaceCheatManager::GiveAll()
{
	APlayerController* PC = GetOuterAPlayerController();
	AMyCharacter* Character = PC ? Cast<AMyCharacter>(PC->GetPawn()) : nullptr;
	if (!Character)
	{
		UE_LOG(LogTemp, Error, TEXT("GiveAll - no AMyCharacter pawn"));
		return;
	}

	UInventoryComponent* Inventory = Character->GetInventoryComponent();
	if (!Inventory)
	{
		UE_LOG(LogTemp, Error, TEXT("GiveAll - no InventoryComponent"));
		return;
	}

	// Enumerate every UItemDefinition under /Game/Inventory. ScanPathsSynchronous
	// ensures the registry knows the path in packaged non-shipping builds too (not
	// just the editor), so the cheat isn't editor-only.
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	const FString Folder = TEXT("/Game/Inventory");
	AssetRegistry.ScanPathsSynchronous({ Folder }, /*bForceRescan*/ false);

	TArray<FAssetData> AssetDatas;
	AssetRegistry.GetAssetsByPath(FName(*Folder), AssetDatas, /*bRecursive*/ true);

	int32 Granted = 0;
	for (const FAssetData& AD : AssetDatas)
	{
		UItemDefinition* Def = Cast<UItemDefinition>(AD.GetAsset());
		if (!Def || Def->ItemID.IsNone() || !Def->Mesh)
		{
			continue;
		}
		if (Inventory->HasItem(Def->ItemID))
		{
			continue;
		}
		Inventory->AddItemWithData(Def->ToInventoryItemData());
		++Granted;
	}

	if (UInventoryUIComponent* UI = Character->GetInventoryUIComponent())
	{
		UI->OpenInventoryUI();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("GiveAll - no InventoryUIComponent; items granted but UI not opened"));
	}

	UE_LOG(LogTemp, Display, TEXT("GiveAll - granted %d new item(s) from %s"), Granted, *Folder);
}
