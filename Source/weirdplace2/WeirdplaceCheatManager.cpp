#include "WeirdplaceCheatManager.h"

#include "StorySubsystem.h"
#include "PayPhone.h"
#include "CRTTV.h"
#include "FirstPersonCharacter.h"
#include "MyCharacter.h"
#include "Inventory.h"
#include "ItemDefinition.h"
#include "Seneca.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

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

	// Dev visualization: teleport in front of the beat's actor so it's on screen
	// as it changes. Needs the pawn, so it lives here rather than in the subsystem.
	UClass* FrameClass = nullptr;
	switch (Target)
	{
	case EStoryFlag::TornadoWarningDisplayed: FrameClass = ACRTTV::StaticClass(); break;
	case EStoryFlag::SeenTornadoWarning:      FrameClass = APayPhone::StaticClass(); break;
	default: break;
	}

	AFirstPersonCharacter* Pawn = Cast<AFirstPersonCharacter>(PC->GetPawn());
	if (FrameClass && Pawn)
	{
		TArray<AActor*> Found;
		UGameplayStatics::GetAllActorsOfClass(World, FrameClass, Found);
		if (Found.Num() > 0)
		{
			AActor* Tgt = Found[0];
			const FVector TgtLoc = Tgt->GetActorLocation();
			FVector Fwd = Tgt->GetActorForwardVector();
			Fwd.Z = 0.f;
			Fwd = Fwd.GetSafeNormal();
			if (Fwd.IsNearlyZero()) { Fwd = FVector::ForwardVector; }
			const FVector NewLoc = TgtLoc + Fwd * 400.f + FVector(0.f, 0.f, 100.f);
			Pawn->SetActorLocation(NewLoc, false, nullptr, ETeleportType::TeleportPhysics);
			PC->SetControlRotation((TgtLoc - NewLoc).Rotation());
		}
	}

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
