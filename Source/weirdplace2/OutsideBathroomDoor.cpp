#include "OutsideBathroomDoor.h"
#include "Seneca.h"
#include "MyCharacter.h"
#include "Inventory.h"
#include "ItemDefinition.h"
#include "HeldItemComponent.h"
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

	FName ActiveItem = Inventory->GetActiveItem();
	UE_LOG(LogTemp, Warning, TEXT("OutsideBathroomDoor - ActiveItem='%s', KeyToRemove='%s', HasKey=%d"),
		*ActiveItem.ToString(), *KeyToRemove.ToString(), Inventory->HasItem(KeyToRemove));

	if (ActiveItem != KeyToRemove)
	{
		UE_LOG(LogTemp, Warning, TEXT("OutsideBathroomDoor - Active item does not match key, playing locked sound"));
		if (LockedDoorSound)
		{
			UGameplayStatics::PlaySound2D(this, LockedDoorSound);
		}
		return;
	}

	StartKeyBreakSequence();
}

void AOutsideBathroomDoor::StartKeyBreakSequence()
{
	ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	AMyCharacter* MyCharacter = Cast<AMyCharacter>(PlayerCharacter);
	if (!MyCharacter)
	{
		UE_LOG(LogTemp, Error, TEXT("OutsideBathroomDoor::StartKeyBreakSequence - No AMyCharacter"));
		return;
	}

	UHeldItemComponent* HeldItem = MyCharacter->GetHeldItemComponent();
	UInventoryComponent* Inventory = MyCharacter->GetInventoryComponent();
	if (!HeldItem || !Inventory)
	{
		UE_LOG(LogTemp, Error, TEXT("OutsideBathroomDoor::StartKeyBreakSequence - Missing HeldItem or Inventory"));
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

	// Hide the real held item immediately - seamless hand-off to AnimKeyMesh
	HeldItem->HideHeldItem();

	// Capture materials from key's inventory data before removing it
	FInventoryItemData KeyData = Inventory->GetItemData(KeyToRemove);
	KeyMaterials = KeyData.Materials;

	// Remove key from inventory and clear active item so HeldItemComponent stops tracking it
	Inventory->RemoveItem(KeyToRemove);
	Inventory->ClearActiveItem();

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

	// Two timers anchored at OnKeyTurnComplete (== KeyBreakSound fire):
	//   KeyFallDelay   — broken half visibly falls
	//   KeyCollectDelay — inventory grant (fires CollectSound), Seneca notify
	// Keeping the collect anchored to the break sound (not the fall) gives
	// designers a single knob to tune the break→collect audio gap.
	GetWorldTimerManager().SetTimer(KeyFallTimerHandle, this, &AOutsideBathroomDoor::EnableKeyFall, KeyFallDelay, false);
	GetWorldTimerManager().SetTimer(KeyCollectTimerHandle, this, &AOutsideBathroomDoor::GrantBrokenKey, KeyCollectDelay, false);
}

void AOutsideBathroomDoor::EnableKeyFall()
{
	if (AnimKeyMesh)
	{
		AnimKeyMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		AnimKeyMesh->SetSimulatePhysics(true);
	}
}

void AOutsideBathroomDoor::GrantBrokenKey()
{
	ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	AMyCharacter* MyCharacter = Cast<AMyCharacter>(PlayerCharacter);
	if (MyCharacter)
	{
		UInventoryComponent* Inventory = MyCharacter->GetInventoryComponent();
		if (Inventory && BrokenKeyDef && BrokenKeyDef->Mesh)
		{
			FInventoryItemData BrokenKeyData = BrokenKeyDef->ToInventoryItemData();
			// Reuse the player's actual key materials so the inventory entry
			// matches the original key visual rather than the def's defaults.
			BrokenKeyData.Materials = KeyMaterials;
			Inventory->AddItemWithData(BrokenKeyData);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("OutsideBathroomDoor::GrantBrokenKey - missing Inventory or BrokenKeyDef"));
		}
	}

	bDidDropKey = true;

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
