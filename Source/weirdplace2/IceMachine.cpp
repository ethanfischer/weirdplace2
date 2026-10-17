#include "IceMachine.h"
#include "FirstPersonCharacter.h"
#include "Inventory.h"
#include "ItemDefinition.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	UStaticMeshComponent* MakePart(AActor* Owner, USceneComponent* Parent, const TCHAR* Name, const TCHAR* AssetPath, const FVector& Pivot)
	{
		UStaticMeshComponent* Part = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(Parent);
		Part->SetRelativeLocation(Pivot);
		ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(AssetPath);
		if (Mesh.Succeeded())
		{
			Part->SetStaticMesh(Mesh.Object);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("IceMachine: part mesh %s missing"), AssetPath);
		}
		Part->SetMobility(EComponentMobility::Movable); // they swing/tumble at runtime
		Part->SetCollisionProfileName(TEXT("BlockAll"));
		return Part;
	}
}

AIceMachine::AIceMachine()
{
	UStaticMeshComponent* Body = GetStaticMeshComponent();
	Body->SetMobility(EComponentMobility::Movable);
	ConstructorHelpers::FObjectFinder<UStaticMesh> BodyMesh(TEXT("/Game/Fab/IceMachineParts/SM_IceMachine_Body.SM_IceMachine_Body"));
	if (BodyMesh.Succeeded())
	{
		Body->SetStaticMesh(BodyMesh.Object);
	}

	// Pivots come from the split (body space, front = +Y): doors hinge on their
	// outer edge at the front face, padlocks sit on the hasps between the doors.
	DoorL    = MakePart(this, Body, TEXT("DoorL"),    TEXT("/Game/Fab/IceMachineParts/SM_IceMachine_DoorL.SM_IceMachine_DoorL"),       FVector(-64.04f, 60.82f, 0.0f));
	DoorR    = MakePart(this, Body, TEXT("DoorR"),    TEXT("/Game/Fab/IceMachineParts/SM_IceMachine_DoorR.SM_IceMachine_DoorR"),       FVector( 96.74f, 60.81f, 0.0f));
	PadlockL = MakePart(this, Body, TEXT("PadlockL"), TEXT("/Game/Fab/IceMachineParts/SM_IceMachine_PadlockL.SM_IceMachine_PadlockL"), FVector(  6.45f, 50.76f, 108.66f));
	PadlockR = MakePart(this, Body, TEXT("PadlockR"), TEXT("/Game/Fab/IceMachineParts/SM_IceMachine_PadlockR.SM_IceMachine_PadlockR"), FVector( 26.25f, 50.76f, 108.66f));
}

bool AIceMachine::UseItem(FName ItemID)
{
	if (ItemID != OpenerItemID || bBrokenOpen)
	{
		return false;
	}
	if (!MoneyDef)
	{
		UE_LOG(LogTemp, Error, TEXT("IceMachine %s: MoneyDef not set; cannot give money"), *GetName());
		return false;
	}
	Opener = Cast<AFirstPersonCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
	if (!Opener.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("IceMachine %s: no player character to give the money to"), *GetName());
		return false;
	}

	bBrokenOpen = true;
	if (BreakOpenSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, BreakOpenSound, GetActorLocation());
	}

	// Padlocks tumble, doors swing: driven in Tick from here.
	PadlockLStart = PadlockL->GetRelativeLocation();
	PadlockRStart = PadlockR->GetRelativeLocation();
	OpenElapsed = 0.0f;
	GetWorldTimerManager().SetTimer(AnimTimer, this, &AIceMachine::TickBreakOpen, AnimStep, true);
	UE_LOG(LogTemp, Log, TEXT("IceMachine %s: broken open with %s"), *GetName(), *ItemID.ToString());
	return true;
}

void AIceMachine::GiveMoney()
{
	AFirstPersonCharacter* FPChar = Opener.Get();
	UInventoryComponent* Inventory = FPChar ? FPChar->GetInventoryComponent() : nullptr;
	if (!Inventory)
	{
		UE_LOG(LogTemp, Error, TEXT("IceMachine %s: opener gone before the door finished; money not given"), *GetName());
		return;
	}
	const FInventoryItemData ItemData = MoneyDef->ToInventoryItemData();
	Inventory->AddItemWithData(ItemData);
	FPChar->ShowItemNotification(ItemData, MoneyDef->NotificationRotation);
	UE_LOG(LogTemp, Log, TEXT("IceMachine %s: gave %s"), *GetName(), *MoneyDef->ItemID.ToString());
}

void AIceMachine::TickBreakOpen()
{
	if (OpenElapsed < 0.0f)
	{
		return;
	}
	OpenElapsed += AnimStep;

	// Padlocks: knocked forward and down with a tumble, then gone.
	const float Fall = FMath::Clamp(OpenElapsed / PadlockFallSeconds, 0.0f, 1.0f);
	const FVector Knock(0.0f, 14.0f * Fall, -110.0f * Fall * Fall); // +Y is out the front
	PadlockL->SetRelativeLocation(PadlockLStart + Knock + FVector(-6.0f * Fall, 0.0f, 0.0f));
	PadlockR->SetRelativeLocation(PadlockRStart + Knock + FVector( 6.0f * Fall, 0.0f, 0.0f));
	PadlockL->SetRelativeRotation(FRotator(-160.0f * Fall, 0.0f,  90.0f * Fall));
	PadlockR->SetRelativeRotation(FRotator(-160.0f * Fall, 0.0f, -90.0f * Fall));
	if (Fall >= 1.0f)
	{
		PadlockL->SetVisibility(false);
		PadlockR->SetVisibility(false);
		PadlockL->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PadlockR->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// Left door: ease out to DoorOpenAngle. Its hinge is at min X, so +yaw swings
	// the free edge out the front (+Y). The right door stays shut.
	const float Door = FMath::Clamp((OpenElapsed - DoorDelaySeconds) / DoorOpenSeconds, 0.0f, 1.0f);
	const float Eased = 1.0f - FMath::Square(1.0f - Door);
	DoorL->SetRelativeRotation(FRotator(0.0f, DoorOpenAngle * Eased, 0.0f));

	if (Fall >= 1.0f && Door >= 1.0f)
	{
		OpenElapsed = -1.0f;
		GetWorldTimerManager().ClearTimer(AnimTimer);
		UE_LOG(LogTemp, Log, TEXT("IceMachine %s: door open"), *GetName());
		GiveMoney();
	}
}
