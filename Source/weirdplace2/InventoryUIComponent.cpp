#include "InventoryUIComponent.h"
#include "InventoryUIActor.h"
#include "Inventory.h"
#include "MyCharacter.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

UInventoryUIComponent::UInventoryUIComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UInventoryUIComponent::BeginPlay()
{
	Super::BeginPlay();

	// Find inventory component on owner
	AActor* Owner = GetOwner();
	if (Owner)
	{
		if (AMyCharacter* MyCharacter = Cast<AMyCharacter>(Owner))
		{
			InventoryComponent = MyCharacter->GetInventoryComponent();
		}
		else
		{
			InventoryComponent = Owner->FindComponentByClass<UInventoryComponent>();
		}

		if (InventoryComponent)
		{
			InventoryComponent->OnInventoryChanged.AddDynamic(this, &UInventoryUIComponent::OnInventoryChanged);
		}
	}

	// Load default UI actor class if not set
	if (!InventoryUIActorClass)
	{
		InventoryUIActorClass = AInventoryUIActor::StaticClass();
	}
}

void UInventoryUIComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Handle animation state machine
	switch (CurrentState)
	{
	case EInventoryUIState::Opening:
		AnimationProgress += DeltaTime / AnimationDuration;
		if (AnimationProgress >= 1.0f)
		{
			AnimationProgress = 1.0f;
			CurrentState = EInventoryUIState::Open;
		}
		UpdateInventoryPosition();
		break;

	case EInventoryUIState::Closing:
		AnimationProgress -= DeltaTime / AnimationDuration;
		if (AnimationProgress <= 0.0f)
		{
			AnimationProgress = 0.0f;
			CurrentState = EInventoryUIState::Closed;
			DestroyInventoryUIActor();
			UnbindConfirmInput();
			UnfreezePlayerMovement();

			// Re-enable interactions with environment
			if (AMyCharacter* MyCharacter = Cast<AMyCharacter>(GetOwner()))
			{
				MyCharacter->SetCanInteract(true);
			}
		}
		else
		{
			UpdateInventoryPosition();
		}
		break;

	case EInventoryUIState::Open:
		// Update selection based on where player is looking
		UpdateReticleSelection();
		break;

	case EInventoryUIState::Closed:
		// Nothing to do
		break;
	}
}

void UInventoryUIComponent::ToggleInventoryUI()
{
	if (CurrentState == EInventoryUIState::Closed || CurrentState == EInventoryUIState::Closing)
	{
		OpenInventoryUI();
	}
	else
	{
		CloseInventoryUI();
	}
}

void UInventoryUIComponent::OpenInventoryUI()
{
	if (CurrentState == EInventoryUIState::Open || CurrentState == EInventoryUIState::Opening)
	{
		return;
	}

	// Store initial camera position/rotation for the UI
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (PC)
	{
		FVector CameraLocation;
		FRotator CameraRotation;
		PC->GetPlayerViewPoint(CameraLocation, CameraRotation);

		// Calculate and store the UI position
		FVector ForwardDir = CameraRotation.Vector();
		FVector UpDir = FRotationMatrix(CameraRotation).GetScaledAxis(EAxis::Z);

		StoredUIPosition = CameraLocation + ForwardDir * InventoryDistance + UpDir * VerticalOffset;
		StoredUIRotation = CameraRotation;
	}

	// Spawn the UI actor if needed
	if (!InventoryUIActor)
	{
		SpawnInventoryUIActor();
	}

	// Reset selection to first slot
	SelectedIndex = 0;

	CurrentState = EInventoryUIState::Opening;
	FreezePlayerMovement();
	BindConfirmInput();

	// Disable interactions with environment
	if (AMyCharacter* MyCharacter = Cast<AMyCharacter>(GetOwner()))
	{
		MyCharacter->SetCanInteract(false);
	}

	// Update UI with current selection
	if (InventoryUIActor)
	{
		InventoryUIActor->SetSelectedIndex(SelectedIndex);
		InventoryUIActor->RefreshDisplay();
	}

	UE_LOG(LogTemp, Log, TEXT("Opening Inventory UI"));
}

void UInventoryUIComponent::CloseInventoryUI()
{
	if (CurrentState == EInventoryUIState::Closed || CurrentState == EInventoryUIState::Closing)
	{
		return;
	}

	CurrentState = EInventoryUIState::Closing;

	UE_LOG(LogTemp, Log, TEXT("Closing Inventory UI"));
}

bool UInventoryUIComponent::IsInventoryOpen() const
{
	return CurrentState == EInventoryUIState::Open || CurrentState == EInventoryUIState::Opening;
}

bool UInventoryUIComponent::IsLookingAtItem() const
{
	if (CurrentState != EInventoryUIState::Open || !InventoryComponent)
	{
		return false;
	}

	TArray<FName> Items = InventoryComponent->GetItems();
	return Items.IsValidIndex(SelectedIndex);
}

void UInventoryUIComponent::ConfirmSelection()
{
	if (CurrentState != EInventoryUIState::Open) return;

	if (InventoryComponent)
	{
		TArray<FName> Items = InventoryComponent->GetItems();
		if (Items.IsValidIndex(SelectedIndex))
		{
			FName SelectedItem = Items[SelectedIndex];
			InventoryComponent->SetActiveItem(SelectedItem);
			UE_LOG(LogTemp, Log, TEXT("Confirmed selection: %s"), *SelectedItem.ToString());
			// TODO: Add visual/audio feedback for selection
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("Selected empty slot %d - no action"), SelectedIndex);
		}
	}

	// Don't close inventory - user must press Tab or Exit to close
}

void UInventoryUIComponent::SpawnInventoryUIActor()
{
	if (InventoryUIActor) return;

	UWorld* World = GetWorld();
	if (!World || !InventoryUIActorClass) return;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.Owner = GetOwner();

	InventoryUIActor = World->SpawnActor<AInventoryUIActor>(InventoryUIActorClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

	if (InventoryUIActor)
	{
		InventoryUIActor->SetInventoryComponent(InventoryComponent);
		InventoryUIActor->SetGridColumns(GridColumns);
		InventoryUIActor->SetGridRows(GridRows);
		UE_LOG(LogTemp, Log, TEXT("Spawned InventoryUIActor"));
	}
}

void UInventoryUIComponent::DestroyInventoryUIActor()
{
	if (InventoryUIActor)
	{
		InventoryUIActor->Destroy();
		InventoryUIActor = nullptr;
		UE_LOG(LogTemp, Log, TEXT("Destroyed InventoryUIActor"));
	}
}

void UInventoryUIComponent::UpdateInventoryPosition()
{
	if (!InventoryUIActor) return;

	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC) return;

	FVector CameraLocation;
	FRotator CameraRotation;
	PC->GetPlayerViewPoint(CameraLocation, CameraRotation);

	// During animation, interpolate from animated position to stored position
	// The UI animates in from below, then stays fixed
	float EasedProgress = FMath::InterpEaseInOut(0.0f, 1.0f, AnimationProgress, 2.0f);

	// Calculate animated start position (below the target)
	FVector UpDir = FRotationMatrix(StoredUIRotation).GetScaledAxis(EAxis::Z);
	FVector AnimatedPosition = StoredUIPosition - UpDir * AnimationDropDistance * (1.0f - EasedProgress);

	InventoryUIActor->SetActorLocation(AnimatedPosition);
	InventoryUIActor->SetActorRotation(StoredUIRotation);

	// Update opacity based on animation
	InventoryUIActor->SetOpacity(EasedProgress);
}

void UInventoryUIComponent::BindConfirmInput()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC || !PC->InputComponent) return;

	// Bind confirm (E / A button) - selects item but doesn't close
	PC->InputComponent->BindAction("InventoryConfirmSelection", IE_Pressed, this, &UInventoryUIComponent::ConfirmSelection);

	// Bind close (Q / B button) - closes inventory
	PC->InputComponent->BindAction("Exit Interaction", IE_Pressed, this, &UInventoryUIComponent::CloseInventoryUI);
}

void UInventoryUIComponent::UnbindConfirmInput()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC || !PC->InputComponent) return;

	PC->InputComponent->RemoveActionBinding("InventoryConfirmSelection", IE_Pressed);
	PC->InputComponent->RemoveActionBinding("Exit Interaction", IE_Pressed);
}

void UInventoryUIComponent::FreezePlayerMovement()
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	ACharacter* Character = Cast<ACharacter>(Owner);
	if (Character)
	{
		UCharacterMovementComponent* MovementComp = Character->GetCharacterMovement();
		if (MovementComp)
		{
			MovementComp->DisableMovement();
			UE_LOG(LogTemp, Log, TEXT("Froze player movement"));
		}
	}
}

void UInventoryUIComponent::UnfreezePlayerMovement()
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	ACharacter* Character = Cast<ACharacter>(Owner);
	if (Character)
	{
		UCharacterMovementComponent* MovementComp = Character->GetCharacterMovement();
		if (MovementComp)
		{
			MovementComp->SetMovementMode(MOVE_Walking);
			UE_LOG(LogTemp, Log, TEXT("Unfroze player movement"));
		}
	}
}

void UInventoryUIComponent::UpdateReticleSelection()
{
	int32 NewIndex = CalculateSlotFromReticle();

	if (NewIndex != SelectedIndex && NewIndex >= 0)
	{
		SelectedIndex = NewIndex;
		if (InventoryUIActor)
		{
			InventoryUIActor->SetSelectedIndex(SelectedIndex);
		}
	}
}

int32 UInventoryUIComponent::CalculateSlotFromReticle() const
{
	if (!InventoryUIActor) return SelectedIndex;

	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC) return SelectedIndex;

	FVector CameraLocation;
	FRotator CameraRotation;
	PC->GetPlayerViewPoint(CameraLocation, CameraRotation);

	// Get the direction the player is looking
	FVector LookDir = CameraRotation.Vector();

	// Get UI actor's transform
	FVector UILocation = InventoryUIActor->GetActorLocation();
	FRotator UIRotation = InventoryUIActor->GetActorRotation();

	// Calculate the plane of the UI (facing the original camera position)
	FVector UIForward = UIRotation.Vector();
	FVector UIRight = FRotationMatrix(UIRotation).GetScaledAxis(EAxis::Y);
	FVector UIUp = FRotationMatrix(UIRotation).GetScaledAxis(EAxis::Z);

	// Ray-plane intersection
	// The UI plane has normal = UIForward, point = UILocation
	float Denom = FVector::DotProduct(LookDir, UIForward);
	if (FMath::Abs(Denom) < 0.0001f)
	{
		// Ray is parallel to plane
		return SelectedIndex;
	}

	float T = FVector::DotProduct(UILocation - CameraLocation, UIForward) / Denom;
	if (T < 0)
	{
		// Intersection is behind camera
		return SelectedIndex;
	}

	// Point where look ray hits the UI plane
	FVector HitPoint = CameraLocation + LookDir * T;

	// Convert to local coordinates relative to UI center
	FVector LocalHit = HitPoint - UILocation;
	float LocalY = FVector::DotProduct(LocalHit, UIRight);  // Horizontal
	float LocalZ = FVector::DotProduct(LocalHit, UIUp);     // Vertical

	// Calculate grid dimensions (must match InventoryUIActor calculations)
	float ThumbnailSize = 8.0f;  // Match default in InventoryUIActor
	float ThumbnailSpacing = 2.0f;
	float SlotWidth = ThumbnailSize + ThumbnailSpacing;
	float SlotHeight = ThumbnailSize * 1.4f + ThumbnailSpacing;

	float GridWidth = (GridColumns - 1) * SlotWidth;
	float GridHeight = (GridRows - 1) * SlotHeight;

	// Convert local position to grid coordinates
	// Grid is centered, so offset by half
	float GridY = LocalY + GridWidth * 0.5f;
	float GridZ = -LocalZ + GridHeight * 0.5f;  // Flip Z because grid row 0 is at top

	// Calculate column and row
	int32 Col = FMath::FloorToInt(GridY / SlotWidth + 0.5f);
	int32 Row = FMath::FloorToInt(GridZ / SlotHeight + 0.5f);

	// Clamp to valid range
	Col = FMath::Clamp(Col, 0, GridColumns - 1);
	Row = FMath::Clamp(Row, 0, GridRows - 1);

	int32 SlotIndex = Row * GridColumns + Col;
	int32 TotalSlots = GridColumns * GridRows;

	return FMath::Clamp(SlotIndex, 0, TotalSlots - 1);
}

void UInventoryUIComponent::OnInventoryChanged(const TArray<FName>& CurrentItems)
{
	// Refresh UI if open
	if (InventoryUIActor && IsInventoryOpen())
	{
		ClampSelectedIndex();
		InventoryUIActor->SetSelectedIndex(SelectedIndex);
		InventoryUIActor->RefreshDisplay();
	}
}

void UInventoryUIComponent::ClampSelectedIndex()
{
	// Clamp to total slots (not item count, since we now have fixed grid)
	int32 TotalSlots = GridColumns * GridRows;
	if (SelectedIndex >= TotalSlots)
	{
		SelectedIndex = TotalSlots - 1;
	}
	if (SelectedIndex < 0)
	{
		SelectedIndex = 0;
	}
}
