#include "Seneca.h"
#include "FirstPersonCharacter.h"
#include "MyCharacter.h"
#include "Inventory.h"
#include "BPFL_Utilities.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/ChildActorComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DlgSystem/DlgDialogue.h"
#include "DlgSystem/DlgContext.h"
#include "Engine/StaticMesh.h"
#include "Door.h"

ASeneca::ASeneca()
{
	PrimaryActorTick.bCanEverTick = false;
	// Components are created in Blueprint to preserve MetaHuman setup
}

void ASeneca::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Log, TEXT("Seneca::BeginPlay - Participant name: '%s'"), *GetParticipantName_Implementation().ToString());

	// Find the dialogue widget component - it's inside a Child Actor Component
	TArray<UChildActorComponent*> ChildActorComponents;
	GetComponents<UChildActorComponent>(ChildActorComponents);
	for (UChildActorComponent* ChildActorComp : ChildActorComponents)
	{
		if (ChildActorComp->GetName().Contains(TEXT("WorldSpace_UI_Dialogue")))
		{
			if (AActor* ChildActor = ChildActorComp->GetChildActor())
			{
				DialogueWidgetComponent = ChildActor->FindComponentByClass<UWidgetComponent>();
			}
			break;
		}
	}

	// Initialize quest state
	BoolValues.Add(FName("GaveKey"), false);
	BoolValues.Add(FName("KeyDropped"), false);
	IntValues.Add(FName("MovieCount"), 0);

	// Bind to player inventory changes
	ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	if (AMyCharacter* MyCharacter = Cast<AMyCharacter>(PlayerCharacter))
	{
		if (UInventoryComponent* Inventory = MyCharacter->GetInventoryComponent())
		{
			Inventory->OnInventoryChanged.AddDynamic(this, &ASeneca::OnInventoryChanged);
			UpdateMovieCount();
		}
	}

	if (TriggerSphere)
	{
		// Update sphere radius in case it was changed in editor
		TriggerSphere->SetSphereRadius(DialogueTriggerRadius);

		// Bind overlap events
		TriggerSphere->OnComponentBeginOverlap.AddDynamic(this, &ASeneca::OnSphereBeginOverlap);
		TriggerSphere->OnComponentEndOverlap.AddDynamic(this, &ASeneca::OnSphereEndOverlap);
	}
}

void ASeneca::Interact_Implementation()
{
	// Start dialogue when interacted with
	ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);

	if (AFirstPersonCharacter* FPCharacter = Cast<AFirstPersonCharacter>(PlayerCharacter))
	{
		if (Dialogue)
		{
			FPCharacter->StartDialogueWithNPC(Dialogue, this);
		}
	}
}

void ASeneca::OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Set look-at behavior (applies to any actor)
	if (BodyMesh)
	{
		UBPFL_Utilities::SetShouldLookAtPlayer(true, OtherActor, BodyMesh);
	}

	// Start dialogue only if OtherActor is the player
	AFirstPersonCharacter* FPCharacter = Cast<AFirstPersonCharacter>(OtherActor);
	if (FPCharacter && Dialogue)
	{
		FPCharacter->StartDialogueWithNPC(Dialogue, this);
	}
}

void ASeneca::OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	// Stop look-at behavior
	if (BodyMesh)
	{
		UBPFL_Utilities::SetShouldLookAtPlayer(false, OtherActor, BodyMesh);
	}
}

// --- IDlgDialogueParticipant Implementation ---

FName ASeneca::GetParticipantName_Implementation() const
{
	return FName("Seneca");
}

FText ASeneca::GetParticipantDisplayName_Implementation(FName ActiveSpeaker) const
{
	return FText::FromString("Seneca");
}

ETextGender ASeneca::GetParticipantGender_Implementation() const
{
	return ETextGender::Masculine;
}

UTexture2D* ASeneca::GetParticipantIcon_Implementation(FName ActiveSpeaker, FName ActiveSpeakerState) const
{
	return nullptr;
}

bool ASeneca::CheckCondition_Implementation(const UDlgContext* Context, FName ConditionName) const
{
	if (ConditionName == FName("HasEnoughMovies"))
	{
		const int32* MovieCount = IntValues.Find(FName("MovieCount"));
		return MovieCount && *MovieCount >= RequiredMovieCount;
	}

	if (ConditionName == FName("HasKey"))
	{
		ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
		if (AMyCharacter* MyCharacter = Cast<AMyCharacter>(PlayerCharacter))
		{
			if (UInventoryComponent* Inventory = MyCharacter->GetInventoryComponent())
			{
				return Inventory->HasItem(KeyToGive);
			}
		}
		return false;
	}

	// Fallback: check BoolValues map
	const bool* Value = BoolValues.Find(ConditionName);
	return Value && *Value;
}

float ASeneca::GetFloatValue_Implementation(FName ValueName) const
{
	return 0.0f;
}

int32 ASeneca::GetIntValue_Implementation(FName ValueName) const
{
	const int32* Value = IntValues.Find(ValueName);
	return Value ? *Value : 0;
}

bool ASeneca::GetBoolValue_Implementation(FName ValueName) const
{
	const bool* Value = BoolValues.Find(ValueName);
	return Value && *Value;
}

FName ASeneca::GetNameValue_Implementation(FName ValueName) const
{
	return NAME_None;
}

bool ASeneca::OnDialogueEvent_Implementation(UDlgContext* Context, FName EventName)
{
	UE_LOG(LogTemp, Log, TEXT("Seneca::OnDialogueEvent - EventName: '%s'"), *EventName.ToString());

	if (EventName == FName("GiveKey"))
	{
		GiveKey();
		BoolValues.Add(FName("GaveKey"), true);
		return true;
	}

	if (EventName == FName("MoveToSmoking"))
	{
		MoveToSmokingPosition();
		return true;
	}

	if (EventName == FName("MoveToEmployeeBathroom"))
	{
		MoveToEmployeeBathroomPosition();
		return true;
	}

	if (EventName == FName("UnlockEmployeeDoor"))
	{
		UnlockEmployeeBathroomDoor();
		return true;
	}

	return false;
}

void ASeneca::GiveKey()
{
	UE_LOG(LogTemp, Log, TEXT("Seneca::GiveKey called"));

	ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	if (!PlayerCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("Seneca::GiveKey - Failed to get player character"));
		return;
	}

	AMyCharacter* MyCharacter = Cast<AMyCharacter>(PlayerCharacter);
	if (!MyCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("Seneca::GiveKey - Player is not AMyCharacter"));
		return;
	}

	UInventoryComponent* Inventory = MyCharacter->GetInventoryComponent();
	if (!Inventory)
	{
		UE_LOG(LogTemp, Warning, TEXT("Seneca::GiveKey - Player has no InventoryComponent"));
		return;
	}

	if (KeyToGive == NAME_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("Seneca::GiveKey - KeyToGive is NAME_None"));
		return;
	}

	if (!KeyMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("Seneca::GiveKey - KeyMesh is not set, cannot give key with visual data"));
		return;
	}

	// Build inventory item data with key visuals
	FInventoryItemData ItemData;
	ItemData.ItemID = KeyToGive;
	ItemData.Mesh = KeyMesh;
	ItemData.Scale = KeyScale;

	// Copy materials from the mesh's default materials
	for (int32 i = 0; i < KeyMesh->GetStaticMaterials().Num(); i++)
	{
		ItemData.Materials.Add(KeyMesh->GetMaterial(i));
	}

	Inventory->AddItemWithData(ItemData);
	UE_LOG(LogTemp, Log, TEXT("Seneca::GiveKey - Gave key '%s' to player with visual data"), *KeyToGive.ToString());
}

bool ASeneca::ModifyFloatValue_Implementation(FName ValueName, bool bDelta, float Value)
{
	return false;
}

bool ASeneca::ModifyIntValue_Implementation(FName ValueName, bool bDelta, int32 Value)
{
	if (bDelta)
	{
		int32& Current = IntValues.FindOrAdd(ValueName, 0);
		Current += Value;
	}
	else
	{
		IntValues.Add(ValueName, Value);
	}
	return true;
}

bool ASeneca::ModifyBoolValue_Implementation(FName ValueName, bool bNewValue)
{
	BoolValues.Add(ValueName, bNewValue);
	return true;
}

bool ASeneca::ModifyNameValue_Implementation(FName ValueName, FName NameValue)
{
	return false;
}

// --- Quest State Methods ---

void ASeneca::OnKeyDropped()
{
	UE_LOG(LogTemp, Log, TEXT("Seneca::OnKeyDropped - Key was dropped, moving to smoking position"));
	BoolValues.Add(FName("KeyDropped"), true);
	MoveToSmokingPosition();
}

void ASeneca::MoveToSmokingPosition()
{
	MoveToTarget(SmokingPositionTarget);
	if (SmokingDialogue)
	{
		Dialogue = SmokingDialogue;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Seneca::MoveToSmokingPosition - SmokingDialogue is not set"));
	}
}

void ASeneca::MoveToEmployeeBathroomPosition()
{
	MoveToTarget(EmployeeBathroomPositionTarget);
	if (EmployeeBathroomDialogue)
	{
		Dialogue = EmployeeBathroomDialogue;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Seneca::MoveToEmployeeBathroomPosition - EmployeeBathroomDialogue is not set"));
	}
}

void ASeneca::UnlockEmployeeBathroomDoor()
{
	if (EmployeeBathroomDoor)
	{
		EmployeeBathroomDoor->SetLocked(false);
		UE_LOG(LogTemp, Log, TEXT("Seneca::UnlockEmployeeBathroomDoor - Employee bathroom door unlocked"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Seneca::UnlockEmployeeBathroomDoor - EmployeeBathroomDoor is not set"));
	}
}

void ASeneca::MoveToTarget(AActor* Target)
{
	if (!Target)
	{
		UE_LOG(LogTemp, Warning, TEXT("Seneca::MoveToTarget - Target is null"));
		return;
	}

	SetActorLocation(Target->GetActorLocation());
	SetActorRotation(Target->GetActorRotation());
	UE_LOG(LogTemp, Log, TEXT("Seneca::MoveToTarget - Moved to %s"), *Target->GetName());
}

void ASeneca::UpdateMovieCount()
{
	ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	if (AMyCharacter* MyCharacter = Cast<AMyCharacter>(PlayerCharacter))
	{
		if (UInventoryComponent* Inventory = MyCharacter->GetInventoryComponent())
		{
			IntValues.Add(FName("MovieCount"), Inventory->GetItemCount());
		}
	}
}

void ASeneca::OnInventoryChanged(const TArray<FName>& CurrentItems)
{
	UpdateMovieCount();
}
