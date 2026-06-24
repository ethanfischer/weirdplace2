#include "OutsideBathroomDoor.h"
#include "Seneca.h"
#include "StorySubsystem.h"
#include "MyCharacter.h"
#include "InventoryUIComponent.h"
#include "InspectablePickup.h"
#include "Inventory.h"
#include "ItemDefinition.h"
#include "ItemGlow.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TimelineComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

AOutsideBathroomDoor::AOutsideBathroomDoor()
{
	// Outside bathroom door is locked by default
	IsLocked = true;

	// Scene component marking the keyhole position - designer positions this in BP
	KeyLockSocket = CreateDefaultSubobject<USceneComponent>(TEXT("KeyLockSocket"));
	KeyLockSocket->SetupAttachment(RootComponent);

	// Scene component marking the broken-key pickup spawn position - designer positions this in BP
	BrokenKeyDropSocket = CreateDefaultSubobject<USceneComponent>(TEXT("BrokenKeyDropSocket"));
	BrokenKeyDropSocket->SetupAttachment(RootComponent);

	// Persistent mesh used only during the animation (hidden until the sequence runs)
	AnimKeyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AnimKeyMesh"));
	AnimKeyMesh->SetupAttachment(RootComponent);
	AnimKeyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AnimKeyMesh->SetCastShadow(false);
	AnimKeyMesh->SetVisibility(false);

	// Timeline components for the two animation phases
	KeyInsertTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("KeyInsertTimeline"));
	KeyTurnTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("KeyTurnTimeline"));
}

void AOutsideBathroomDoor::BeginPlay()
{
	Super::BeginPlay();

	// Same self-illumination overlay the held key uses, so the key keeps glowing
	// as it's inserted into the lock.
	GlowMaterial = ItemGlow::GetItemGlowMaterial();

	if (KeyInsertCurve && KeyInsertTimeline)
	{
		FOnTimelineFloat InsertCallback;
		InsertCallback.BindUFunction(this, FName("UpdateKeyInsert"));
		KeyInsertTimeline->AddInterpFloat(KeyInsertCurve, InsertCallback);
		KeyInsertTimeline->SetLooping(false);

		FOnTimelineEvent InsertFinishCallback;
		InsertFinishCallback.BindUFunction(this, FName("OnKeyInsertComplete"));
		KeyInsertTimeline->SetTimelineFinishedFunc(InsertFinishCallback);
	}

	if (KeyTurnCurve && KeyTurnTimeline)
	{
		FOnTimelineFloat TurnCallback;
		TurnCallback.BindUFunction(this, FName("UpdateKeyTurn"));
		KeyTurnTimeline->AddInterpFloat(KeyTurnCurve, TurnCallback);
		KeyTurnTimeline->SetLooping(false);

		FOnTimelineEvent TurnFinishCallback;
		TurnFinishCallback.BindUFunction(this, FName("OnKeyTurnComplete"));
		KeyTurnTimeline->SetTimelineFinishedFunc(TurnFinishCallback);
	}
}

void AOutsideBathroomDoor::Interact_Implementation()
{
	UE_LOG(LogTemp, Warning, TEXT("OutsideBathroomDoor::Interact_Implementation CALLED. bDidDropKey=%d, IsLocked=%d"), bDidDropKey, IsLocked);

	// Re-entrancy guard: while the key-break sequence is running, the Key has
	// already been removed from inventory, but bDidDropKey isn't set
	// until the broken half spawns ~3s later. A re-entrant interact in that window
	// (the UE5.7 double-fire input quirk) would otherwise rattle the locked door.
	// Ignore it.
	if (bKeyBreakInProgress && !bDidDropKey)
	{
		UE_LOG(LogTemp, Warning, TEXT("OutsideBathroomDoor - key-break in progress, ignoring re-entrant interact"));
		return;
	}

	// If key was already dropped, behave as a normal locked door
	if (bDidDropKey)
	{
		UE_LOG(LogTemp, Warning, TEXT("OutsideBathroomDoor - Key already dropped, falling through to Super"));
		Super::Interact_Implementation();
		return;
	}

	ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	AMyCharacter* MyCharacter = Cast<AMyCharacter>(PlayerCharacter);
	if (!MyCharacter)
	{
		UE_LOG(LogTemp, Error, TEXT("OutsideBathroomDoor - Could not get AMyCharacter"));
		Super::Interact_Implementation();
		return;
	}

	UInventoryComponent* Inventory = MyCharacter->GetInventoryComponent();
	if (!Inventory)
	{
		UE_LOG(LogTemp, Error, TEXT("OutsideBathroomDoor - No InventoryComponent on character"));
		Super::Interact_Implementation();
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("OutsideBathroomDoor - KeyToRemove='%s', HasKey=%d"),
		*KeyToRemove.ToString(), Inventory->HasItem(KeyToRemove));

	if (!Inventory->HasItem(KeyToRemove))
	{
		UE_LOG(LogTemp, Warning, TEXT("OutsideBathroomDoor - player does not have the key, playing locked sound"));
		LockedSoundPlayCount++;
		if (LockedDoorSound)
		{
			UGameplayStatics::PlaySound2D(this, LockedDoorSound);
		}
		return;
	}

	// Have the key: pop the inventory so the player picks it and presses E to give.
	if (UInventoryUIComponent* InvUI = MyCharacter->GetInventoryUIComponent())
	{
		InvUI->OpenForGive(FInventoryGiveDelegate::CreateUObject(this, &AOutsideBathroomDoor::OnKeyOffered));
	}
	else
	{
		StartKeyBreakSequence();
	}
}

bool AOutsideBathroomDoor::OnKeyOffered(FName ItemID)
{
	if (ItemID != KeyToRemove)
	{
		return false; // wrong item — keep the inventory open
	}

	// ConfirmGiveSelection closes the UI on accept; just run the break sequence
	// (which removes the key).
	StartKeyBreakSequence();
	return true;
}

void AOutsideBathroomDoor::StartKeyBreakSequence()
{
	// Arm the re-entrancy guard: from here until bDidDropKey is set, any further
	// interact is ignored (see Interact_Implementation).
	bKeyBreakInProgress = true;

	ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	AMyCharacter* MyCharacter = Cast<AMyCharacter>(PlayerCharacter);
	if (!MyCharacter)
	{
		UE_LOG(LogTemp, Error, TEXT("OutsideBathroomDoor::StartKeyBreakSequence - No AMyCharacter"));
		return;
	}

	UInventoryComponent* Inventory = MyCharacter->GetInventoryComponent();
	if (!Inventory)
	{
		UE_LOG(LogTemp, Error, TEXT("OutsideBathroomDoor::StartKeyBreakSequence - Missing Inventory"));
		return;
	}

	// Validate all required assets before making irreversible inventory changes
	if (!KeyLockSocket)
	{
		UE_LOG(LogTemp, Error, TEXT("OutsideBathroomDoor::StartKeyBreakSequence - KeyLockSocket is null"));
		return;
	}
	if (!AnimKeyMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("OutsideBathroomDoor::StartKeyBreakSequence - AnimKeyMesh is null"));
		return;
	}
	if (!FullKeyMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("OutsideBathroomDoor::StartKeyBreakSequence - FullKeyMesh is null"));
		return;
	}
	if (!KeyInsertTimeline)
	{
		UE_LOG(LogTemp, Error, TEXT("OutsideBathroomDoor::StartKeyBreakSequence - KeyInsertTimeline is null"));
		return;
	}

	// Start the key just in front of the keyhole so the insertion is a visible
	// slide into the door surface, not a depth-axis shrink from the camera
	FVector WorldApproachDir = KeyLockSocket->GetComponentTransform().TransformVectorNoScale(KeyInsertApproachAxis.GetSafeNormal());
	KeyAnimStartPos = KeyLockSocket->GetComponentLocation() + WorldApproachDir * KeyInsertStartOffset;
	KeyAnimStartRot = KeyLockSocket->GetComponentRotation() + KeyMeshRotationOffset;

	// Capture materials from key's inventory data before removing it
	FInventoryItemData KeyData = Inventory->GetItemData(KeyToRemove);
	KeyMaterials = KeyData.Materials;

	// Remove the key from inventory; the animated AnimKeyMesh takes over visually.
	Inventory->RemoveItem(KeyToRemove);

	// Set up the animated key mesh at the hand position
	if (AnimKeyMesh)
	{
		if (FullKeyMesh)
		{
			AnimKeyMesh->SetStaticMesh(FullKeyMesh);
		}
		for (int32 i = 0; i < KeyMaterials.Num(); i++)
		{
			if (KeyMaterials[i])
			{
				AnimKeyMesh->SetMaterial(i, KeyMaterials[i]);
			}
		}
		AnimKeyMesh->SetWorldScale3D(FVector(0.001f));
		AnimKeyMesh->SetWorldLocation(KeyAnimStartPos);
		AnimKeyMesh->SetWorldRotation(KeyAnimStartRot);
		AnimKeyMesh->SetVisibility(true);
		// Keep the held-key glow on the key as it's inserted into the lock. The
		// overlay persists across the later broken-mesh swap.
		if (GlowMaterial)
		{
			AnimKeyMesh->SetOverlayMaterial(GlowMaterial);
		}
	}

	if (KeyInsertSound)
	{
		if (KeyInsertSoundDelay <= 0.0f)
		{
			PlayKeyInsertSound();
		}
		else
		{
			GetWorldTimerManager().SetTimer(KeyInsertSoundTimerHandle, this, &AOutsideBathroomDoor::PlayKeyInsertSound, KeyInsertSoundDelay, false);
		}
	}

	if (KeyInsertTimeline)
	{
		KeyInsertTimeline->PlayFromStart();
	}

	UE_LOG(LogTemp, Warning, TEXT("OutsideBathroomDoor: Key break sequence started from pos %s"), *KeyAnimStartPos.ToString());
}

void AOutsideBathroomDoor::UpdateKeyInsert(float Alpha)
{
	if (!AnimKeyMesh || !KeyLockSocket) return;

	FVector WorldApproachDir = KeyLockSocket->GetComponentTransform().TransformVectorNoScale(KeyInsertApproachAxis.GetSafeNormal());
	FVector TargetPos = KeyLockSocket->GetComponentLocation() + WorldApproachDir * KeyInsertEndOffset;
	AnimKeyMesh->SetWorldLocation(FMath::Lerp(KeyAnimStartPos, TargetPos, Alpha));
}

void AOutsideBathroomDoor::OnKeyInsertComplete()
{
	UE_LOG(LogTemp, Warning, TEXT("OutsideBathroomDoor: Key inserted - starting turn phase"));

	if (KeyTurnSound && KeyLockSocket)
	{
		if (KeyTurnSoundDelay <= 0.0f)
		{
			PlayKeyTurnSound();
		}
		else
		{
			GetWorldTimerManager().SetTimer(KeyTurnSoundTimerHandle, this, &AOutsideBathroomDoor::PlayKeyTurnSound, KeyTurnSoundDelay, false);
		}
	}

	if (KeyTurnTimeline)
	{
		KeyTurnTimeline->PlayFromStart();
	}
}

void AOutsideBathroomDoor::UpdateKeyTurn(float Alpha)
{
	if (!AnimKeyMesh || !KeyLockSocket) return;

	FQuat BaseRot = FQuat(KeyLockSocket->GetComponentRotation() + KeyMeshRotationOffset);
	FVector WorldTurnAxis = KeyLockSocket->GetComponentTransform().TransformVectorNoScale(KeyTurnAxis.GetSafeNormal());
	FQuat TurnDelta = FQuat(WorldTurnAxis, FMath::DegreesToRadians(FMath::Lerp(0.0f, KeyTurnAngle, Alpha)));
	AnimKeyMesh->SetWorldRotation(TurnDelta * BaseRot);
}

void AOutsideBathroomDoor::OnKeyTurnComplete()
{
	UE_LOG(LogTemp, Warning, TEXT("OutsideBathroomDoor: Key turned - breaking"));

	if (KeyBreakSound && KeyLockSocket)
	{
		if (KeyBreakSoundDelay <= 0.0f)
		{
			PlayKeyBreakSound();
		}
		else
		{
			GetWorldTimerManager().SetTimer(KeyBreakSoundTimerHandle, this, &AOutsideBathroomDoor::PlayKeyBreakSound, KeyBreakSoundDelay, false);
		}
	}

	// Swap to broken mesh immediately so the visual snap is instant
	if (AnimKeyMesh && BrokenKeyDef && BrokenKeyDef->Mesh)
	{
		AnimKeyMesh->SetStaticMesh(BrokenKeyDef->Mesh);
	}

	GetWorldTimerManager().SetTimer(KeyFallTimerHandle, this, &AOutsideBathroomDoor::EnableKeyFall, KeyFallDelay, false);
}

void AOutsideBathroomDoor::EnableKeyFall()
{
	if (AnimKeyMesh)
	{
		AnimKeyMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		AnimKeyMesh->SetSimulatePhysics(true);
	}

	GetWorldTimerManager().SetTimer(BrokenKeyLandTimerHandle, this, &AOutsideBathroomDoor::SpawnBrokenKeyPickup, BrokenKeyLandDelay, false);
}

void AOutsideBathroomDoor::SpawnBrokenKeyPickup()
{
	if (!BrokenKeyDef || !BrokenKeyDef->Mesh)
	{
		UE_LOG(LogTemp, Error, TEXT("OutsideBathroomDoor::SpawnBrokenKeyPickup - BrokenKeyDef missing or has no Mesh"));
		return;
	}

	if (!BrokenKeyDropSocket)
	{
		UE_LOG(LogTemp, Error, TEXT("OutsideBathroomDoor::SpawnBrokenKeyPickup - BrokenKeyDropSocket missing"));
		return;
	}

	if (AnimKeyMesh)
	{
		AnimKeyMesh->SetSimulatePhysics(false);
		AnimKeyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		AnimKeyMesh->SetVisibility(false);
	}

	const FTransform DropTransform = BrokenKeyDropSocket->GetComponentTransform();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = this;

	AInspectablePickup* Pickup = GetWorld()->SpawnActorDeferred<AInspectablePickup>(
		AInspectablePickup::StaticClass(),
		DropTransform,
		this,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (!Pickup)
	{
		UE_LOG(LogTemp, Error, TEXT("OutsideBathroomDoor::SpawnBrokenKeyPickup - failed to spawn AInspectablePickup"));
		return;
	}

	Pickup->SetItemDef(BrokenKeyDef);
	UGameplayStatics::FinishSpawningActor(Pickup, DropTransform);

	UE_LOG(LogTemp, Warning, TEXT("OutsideBathroomDoor: Spawned broken-key pickup at %s (BrokenKeyDropSocket world loc)"),
		*DropTransform.GetLocation().ToString());

	bDidDropKey = true;

	// Record the narrative beat for the tornado/telephone chain (additive —
	// the Seneca-smoking path below is untouched). Items 1/2/4/5 read this.
	if (UWorld* World = GetWorld())
	{
		if (UStorySubsystem* Story = World->GetSubsystem<UStorySubsystem>())
		{
			Story->SetFlag(EStoryFlag::KeyBroke);
		}
	}

	if (SenecaRef)
	{
		SenecaRef->OnKeyDropped();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("OutsideBathroomDoor - SenecaRef is NOT SET, cannot notify key drop"));
	}

	UE_LOG(LogTemp, Warning, TEXT("OutsideBathroomDoor: Key break sequence complete"));
}

bool AOutsideBathroomDoor::IsAnimKeyGlowActive() const
{
	return AnimKeyMesh && AnimKeyMesh->GetOverlayMaterial() != nullptr;
}

void AOutsideBathroomDoor::PlayKeyInsertSound()
{
	if (KeyInsertSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, KeyInsertSound, KeyAnimStartPos);
	}
}

void AOutsideBathroomDoor::PlayKeyTurnSound()
{
	if (KeyTurnSound && KeyLockSocket)
	{
		UGameplayStatics::PlaySoundAtLocation(this, KeyTurnSound, KeyLockSocket->GetComponentLocation());
	}
}

void AOutsideBathroomDoor::PlayKeyBreakSound()
{
	if (KeyBreakSound && KeyLockSocket)
	{
		UGameplayStatics::PlaySoundAtLocation(this, KeyBreakSound, KeyLockSocket->GetComponentLocation());
	}
}
