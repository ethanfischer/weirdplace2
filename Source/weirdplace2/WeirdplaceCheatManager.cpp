#include "WeirdplaceCheatManager.h"

#include "StorySubsystem.h"
#include "FirstPersonCharacter.h"
#include "Inventory.h"
#include "InventoryUIComponent.h"
#include "ItemDefinition.h"
#include "Seneca.h"
#include "Door.h"
#include "GameFramework/Pawn.h"
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

void UWeirdplaceCheatManager::AutoSkip(const FString& BeatName)
{
	auto Show = [](const FString& Msg, FColor Color = FColor::Cyan)
	{
		UE_LOG(LogTemp, Display, TEXT("%s"), *Msg);
		if (GEngine) { GEngine->AddOnScreenDebugMessage(-1, 8.f, Color, Msg); }
	};

	// No argument: report the current setting.
	if (BeatName.IsEmpty())
	{
		const FString Current = UStorySubsystem::GetAutoSkipBeat();
		Show(Current.IsEmpty()
			? TEXT("AutoSkip: not set. Usage: AutoSkip <beat> | AutoSkip clear")
			: FString::Printf(TEXT("AutoSkip: set to '%s'. `AutoSkip clear` to disable."), *Current));
		return;
	}

	const FString Lower = BeatName.ToLower();
	if (Lower == TEXT("clear") || Lower == TEXT("off") || Lower == TEXT("none"))
	{
		UStorySubsystem::SetAutoSkipBeat(FString());
		Show(TEXT("AutoSkip: cleared."));
		return;
	}

	EStoryFlag Target;
	if (!UStorySubsystem::ResolveBeat(BeatName, Target))
	{
		Show(FString::Printf(TEXT("AutoSkip - unknown beat '%s'. Try: KeyBroke | Tornado | Telephone"), *BeatName), FColor::Yellow);
		return;
	}

	// Store the canonical display name so the saved value stays readable.
	const FString Canonical = UStorySubsystem::GetBeatDisplayName(Target);
	UStorySubsystem::SetAutoSkipBeat(Canonical);
	Show(FString::Printf(TEXT("AutoSkip: every play will now skip to '%s'. `AutoSkip clear` to disable."), *Canonical));
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

void UWeirdplaceCheatManager::SkipToKeypad()
{
	APlayerController* PC = GetOuterAPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	UWorld* World = PC ? PC->GetWorld() : nullptr;
	if (!Pawn || !World)
	{
		UE_LOG(LogTemp, Warning, TEXT("SkipToKeypad - no player pawn/world"));
		return;
	}

	// Find the first keypad-locked door (the employee bathroom).
	TArray<AActor*> Doors;
	UGameplayStatics::GetAllActorsOfClass(World, ADoor::StaticClass(), Doors);
	ADoor* Keypad = nullptr;
	for (AActor* A : Doors)
	{
		ADoor* D = Cast<ADoor>(A);
		if (D && D->UsesKeypadLock())
		{
			Keypad = D;
			break;
		}
	}
	if (!Keypad)
	{
		UE_LOG(LogTemp, Warning, TEXT("SkipToKeypad - no door with bUsesKeypadLock found"));
		return;
	}

	// Stand ~150cm out from the door, on whichever side the player is currently on,
	// at the player's current height, facing the door.
	const FVector DoorLoc = Keypad->GetActorLocation();
	const FVector PlayerLoc = Pawn->GetActorLocation();
	const FVector ThroughAxis = Keypad->GetActorRightVector(); // perpendicular to the panel
	const float Side = FVector::DotProduct(ThroughAxis, PlayerLoc - DoorLoc) >= 0.0f ? 1.0f : -1.0f;

	FVector Spot = DoorLoc + ThroughAxis * Side * 150.0f;
	Spot.Z = PlayerLoc.Z; // keep the player's floor height

	const FRotator LookAtDoor = (DoorLoc - Spot).Rotation();

	Pawn->SetActorLocation(Spot, /*bSweep*/ false, nullptr, ETeleportType::TeleportPhysics);
	PC->SetControlRotation(LookAtDoor);

	UE_LOG(LogTemp, Display, TEXT("SkipToKeypad - teleported in front of keypad door '%s'"),
		*Keypad->GetActorNameOrLabel());
}

void UWeirdplaceCheatManager::GiveItem(const FString& Name)
{
	if (Name.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("GiveItem - usage: GiveItem <Name> (e.g. 'GiveItem Key')"));
		return;
	}

	APlayerController* PC = GetOuterAPlayerController();
	AFirstPersonCharacter* Character = PC ? Cast<AFirstPersonCharacter>(PC->GetPawn()) : nullptr;
	if (!Character)
	{
		UE_LOG(LogTemp, Error, TEXT("GiveItem - no AFirstPersonCharacter pawn"));
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
	AFirstPersonCharacter* Character = PC ? Cast<AFirstPersonCharacter>(PC->GetPawn()) : nullptr;
	if (!Character)
	{
		UE_LOG(LogTemp, Error, TEXT("GiveAll - no AFirstPersonCharacter pawn"));
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
