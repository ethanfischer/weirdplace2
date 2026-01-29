#include "InventoryUIComponent.h"
#include "InventoryUIActor.h"
#include "Inventory.h"
#include "MyCharacter.h"
#include "GameFramework/Character.h"
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
			UnbindNavigationInput();
		}
		else
		{
			UpdateInventoryPosition();
		}
		break;

	case EInventoryUIState::Open:
		// Nothing to do
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

	// Spawn the UI actor if needed
	if (!InventoryUIActor)
	{
		SpawnInventoryUIActor();
	}

	// Reset selection to first item
	SelectedIndex = 0;
	ClampSelectedIndex();

	CurrentState = EInventoryUIState::Opening;
	BindNavigationInput();

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

void UInventoryUIComponent::NavigateUp()
{
	if (CurrentState != EInventoryUIState::Open) return;

	int32 ItemCount = InventoryComponent ? InventoryComponent->GetItemCount() : 0;
	if (ItemCount == 0) return;

	// Move up by one row
	int32 NewIndex = SelectedIndex - GridColumns;
	if (NewIndex >= 0)
	{
		SelectedIndex = NewIndex;
		if (InventoryUIActor)
		{
			InventoryUIActor->SetSelectedIndex(SelectedIndex);
		}
	}
}

void UInventoryUIComponent::NavigateDown()
{
	if (CurrentState != EInventoryUIState::Open) return;

	int32 ItemCount = InventoryComponent ? InventoryComponent->GetItemCount() : 0;
	if (ItemCount == 0) return;

	// Move down by one row
	int32 NewIndex = SelectedIndex + GridColumns;
	if (NewIndex < ItemCount)
	{
		SelectedIndex = NewIndex;
		if (InventoryUIActor)
		{
			InventoryUIActor->SetSelectedIndex(SelectedIndex);
		}
	}
}

void UInventoryUIComponent::NavigateLeft()
{
	if (CurrentState != EInventoryUIState::Open) return;

	int32 ItemCount = InventoryComponent ? InventoryComponent->GetItemCount() : 0;
	if (ItemCount == 0) return;

	// Move left, wrap within row
	int32 CurrentRow = SelectedIndex / GridColumns;
	int32 CurrentCol = SelectedIndex % GridColumns;

	if (CurrentCol > 0)
	{
		SelectedIndex--;
		if (InventoryUIActor)
		{
			InventoryUIActor->SetSelectedIndex(SelectedIndex);
		}
	}
}

void UInventoryUIComponent::NavigateRight()
{
	if (CurrentState != EInventoryUIState::Open) return;

	int32 ItemCount = InventoryComponent ? InventoryComponent->GetItemCount() : 0;
	if (ItemCount == 0) return;

	// Move right within row
	int32 NewIndex = SelectedIndex + 1;
	int32 CurrentRow = SelectedIndex / GridColumns;
	int32 NewRow = NewIndex / GridColumns;

	// Only move if staying in same row and within item count
	if (NewRow == CurrentRow && NewIndex < ItemCount)
	{
		SelectedIndex = NewIndex;
		if (InventoryUIActor)
		{
			InventoryUIActor->SetSelectedIndex(SelectedIndex);
		}
	}
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
		}
	}

	// Close inventory after selection
	CloseInventoryUI();
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

	AActor* Owner = GetOwner();
	if (!Owner) return;

	// Get camera location and rotation
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC) return;

	FVector CameraLocation;
	FRotator CameraRotation;
	PC->GetPlayerViewPoint(CameraLocation, CameraRotation);

	// Calculate base position in front of camera
	FVector ForwardDir = CameraRotation.Vector();
	FVector RightDir = FRotationMatrix(CameraRotation).GetScaledAxis(EAxis::Y);
	FVector UpDir = FRotationMatrix(CameraRotation).GetScaledAxis(EAxis::Z);

	// Apply eased animation
	float EasedProgress = FMath::InterpEaseInOut(0.0f, 1.0f, AnimationProgress, 2.0f);

	// Calculate position with animation offset
	float AnimOffset = AnimationDropDistance * (1.0f - EasedProgress);
	FVector TargetPosition = CameraLocation
		+ ForwardDir * InventoryDistance
		+ UpDir * (VerticalOffset - AnimOffset);

	// Face away from camera (so we see the front of the UI)
	FRotator TargetRotation = CameraRotation;

	InventoryUIActor->SetActorLocation(TargetPosition);
	InventoryUIActor->SetActorRotation(TargetRotation);

	// Update opacity based on animation
	InventoryUIActor->SetOpacity(EasedProgress);
}

void UInventoryUIComponent::BindNavigationInput()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC || !PC->InputComponent) return;

	PC->InputComponent->BindAction("InventoryNavigateUp", IE_Pressed, this, &UInventoryUIComponent::NavigateUp);
	PC->InputComponent->BindAction("InventoryNavigateDown", IE_Pressed, this, &UInventoryUIComponent::NavigateDown);
	PC->InputComponent->BindAction("InventoryNavigateLeft", IE_Pressed, this, &UInventoryUIComponent::NavigateLeft);
	PC->InputComponent->BindAction("InventoryNavigateRight", IE_Pressed, this, &UInventoryUIComponent::NavigateRight);
	PC->InputComponent->BindAction("InventoryConfirmSelection", IE_Pressed, this, &UInventoryUIComponent::ConfirmSelection);
}

void UInventoryUIComponent::UnbindNavigationInput()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC || !PC->InputComponent) return;

	PC->InputComponent->RemoveActionBinding("InventoryNavigateUp", IE_Pressed);
	PC->InputComponent->RemoveActionBinding("InventoryNavigateDown", IE_Pressed);
	PC->InputComponent->RemoveActionBinding("InventoryNavigateLeft", IE_Pressed);
	PC->InputComponent->RemoveActionBinding("InventoryNavigateRight", IE_Pressed);
	PC->InputComponent->RemoveActionBinding("InventoryConfirmSelection", IE_Pressed);
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
	int32 ItemCount = InventoryComponent ? InventoryComponent->GetItemCount() : 0;
	if (ItemCount == 0)
	{
		SelectedIndex = 0;
	}
	else if (SelectedIndex >= ItemCount)
	{
		SelectedIndex = ItemCount - 1;
	}
}
