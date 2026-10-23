#include "InspectablePickup.h"
#include "DiegeticTextComponent.h"
#include "FirstPersonCharacter.h"
#include "Inventory.h"
#include "ItemDefinition.h"
#include "ItemGlow.h"
#include "Components/InputComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"

AInspectablePickup::AInspectablePickup()
{
	PrimaryActorTick.bCanEverTick = false;

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	CollisionSphere->SetSphereRadius(25.0f);
	// Block the interact raycast (ECC_Visibility) so look-at picks this up.
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionSphere->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	CollisionSphere->SetGenerateOverlapEvents(false);
	RootComponent = CollisionSphere;

	PickupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
	PickupMesh->SetupAttachment(RootComponent);
	PickupMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AInspectablePickup::BeginPlay()
{
	Super::BeginPlay();

	if (ItemDef && ItemDef->Mesh)
	{
		PickupMesh->SetStaticMesh(ItemDef->Mesh);
		PickupMesh->SetRelativeScale3D(ItemDef->Scale);
		for (int32 i = 0; i < ItemDef->MaterialOverrides.Num(); i++)
		{
			PickupMesh->SetMaterial(i, ItemDef->MaterialOverrides[i]);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("InspectablePickup '%s': ItemDef missing or has no Mesh"), *GetName());
	}

	MyCharacter = Cast<AFirstPersonCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));

	// Self-illumination overlay, applied from spawn so the dropped item reads on
	// the dark floor — not just once it's pulled in to inspect.
	GlowMaterial = ItemGlow::GetItemGlowMaterial();
	if (GlowMaterial && PickupMesh)
	{
		PickupMesh->SetOverlayMaterial(GlowMaterial);
	}

	TArray<UDiegeticTextComponent*> AllDiegeticText;
	GetComponents<UDiegeticTextComponent>(AllDiegeticText);
	for (UDiegeticTextComponent* Comp : AllDiegeticText)
	{
		if (Comp->GetFName() == TEXT("PutBackPrompt"))
		{
			PutBackPrompt = Comp;
			break;
		}
	}
	if (PutBackPrompt)
	{
		PutBackPrompt->SetVisibility(false);
	}
}

bool AInspectablePickup::CanInteract()
{
	return !bCollected;
}

FName AInspectablePickup::GetItemID() const
{
	return ItemDef ? ItemDef->ItemID : NAME_None;
}

void AInspectablePickup::Interact_Implementation()
{
	if (bCollected) return;
	if (!MyCharacter)
	{
		MyCharacter = Cast<AFirstPersonCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
		if (!MyCharacter) return;
	}

	PlayerController = GetWorld()->GetFirstPlayerController();
	if (!PlayerController) return;

	FVector CameraLocation;
	PlayerController->GetPlayerViewPoint(CameraLocation, CameraRotation);

	OriginalActorTransform = GetActorTransform();

	const FVector NewLocation = CameraLocation + (CameraRotation.Vector() * InspectionDistance);
	const FRotator NewRotation = (CameraLocation - NewLocation).Rotation();

	SetActorLocation(NewLocation);
	SetActorRotation(NewRotation);
	if (ItemDef)
	{
		AddActorLocalRotation(ItemDef->InspectionRotation);
	}

	InspectedActor = this;

	MyCharacter->BeginInteractionHold(/*bFreezeLook*/ true);

	if (AFirstPersonCharacter* FPChar = Cast<AFirstPersonCharacter>(MyCharacter))
	{
		FPChar->SetItemHoldLightEnabled(true);
	}

	PlayerController->InputComponent->BindAxis("Turn Right / Left Mouse", this, &AInspectablePickup::RotateInspectedActor);
	PlayerController->InputComponent->BindAxis("Turn Right / Left Gamepad", this, &AInspectablePickup::RotateInspectedActor);

	// Interact (IA_Interact: E, click, gamepad) collects while inspecting.
	MyCharacter->SetInspectionCollectHandler([Weak = TWeakObjectPtr<AInspectablePickup>(this)]()
	{
		if (AInspectablePickup* Pickup = Weak.Get()) { Pickup->CollectInspectedItem(); }
	});
	MyCharacter->SetInspectionBackHandler([Weak = TWeakObjectPtr<AInspectablePickup>(this)]()
	{
		if (AInspectablePickup* Pickup = Weak.Get()) { Pickup->StopInspection(); }
	});

	if (PutBackPrompt)
	{
		PutBackPrompt->SetVisibility(true);
	}
}

void AInspectablePickup::RotateInspectedActor(float AxisValue)
{
	if (!InspectedActor) return;
	const FVector HeadUp = FRotationMatrix(CameraRotation).GetUnitAxis(EAxis::Z);
	const FQuat DeltaRotation = FQuat(HeadUp, FMath::DegreesToRadians(-AxisValue * 2.0f));
	InspectedActor->AddActorWorldRotation(DeltaRotation);
}

void AInspectablePickup::CollectInspectedItem()
{
	if (bCollected) return;
	if (!ItemDef || ItemDef->ItemID.IsNone())
	{
		UE_LOG(LogTemp, Error, TEXT("InspectablePickup '%s': ItemDef missing or has no ItemID"), *GetName());
		return;
	}
	if (!MyCharacter) return;

	UInventoryComponent* Inventory = MyCharacter->GetInventoryComponent();
	if (!Inventory)
	{
		UE_LOG(LogTemp, Error, TEXT("InspectablePickup '%s': player has no InventoryComponent"), *GetName());
		return;
	}

	const FInventoryItemData ItemData = ItemDef->ToInventoryItemData();
	Inventory->AddItemWithData(ItemData);

	if (PickupSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, PickupSound, GetActorLocation());
	}

	bCollected = true;

	// Defer one tick to mirror MovieBox's StopInspection ordering vs the
	// IA_Interact path that fires on the same E press in 5.7.
	GetWorld()->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			StopInspection();
			Destroy();
		}));
}

void AInspectablePickup::StopInspection()
{
	if (!InspectedActor) return;

	PlayerController = GetWorld()->GetFirstPlayerController();
	if (!PlayerController) return;

	InspectedActor->SetActorTransform(OriginalActorTransform);

	if (PlayerController->InputComponent)
	{
		PlayerController->InputComponent->AxisBindings.RemoveAll([](const FInputAxisBinding& Binding)
		{
			return Binding.AxisName == TEXT("Turn Right / Left Mouse")
				|| Binding.AxisName == TEXT("Turn Right / Left Gamepad");
		});
	}
	if (MyCharacter)
	{
		MyCharacter->ClearInspectionCollectHandler();
		MyCharacter->ClearInspectionBackHandler();
	}

	if (PutBackPrompt) PutBackPrompt->SetVisibility(false);

	InspectedActor = nullptr;

	if (MyCharacter)
	{
		MyCharacter->EndInteractionHold(/*bUnfreezeLook*/ true);

		if (AFirstPersonCharacter* FPChar = Cast<AFirstPersonCharacter>(MyCharacter))
		{
			FPChar->SetItemHoldLightEnabled(false);
		}
	}
}
