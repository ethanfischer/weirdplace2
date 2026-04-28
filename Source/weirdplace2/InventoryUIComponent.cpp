#include "InventoryUIComponent.h"
#include "InventoryUIActor.h"
#include "Components/SceneComponent.h"
#include "Inventory.h"
#include "FirstPersonCharacter.h"
#include "MyCharacter.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

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

			if (AFirstPersonCharacter* FirstPersonCharacter = Cast<AFirstPersonCharacter>(GetOwner()))
			{
				FirstPersonCharacter->SetInventoryFlashlightEnabled(true);
			}
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

			if (AFirstPersonCharacter* FirstPersonCharacter = Cast<AFirstPersonCharacter>(GetOwner()))
			{
				FirstPersonCharacter->SetInventoryFlashlightEnabled(false);
			}
		}
		else
		{
			UpdateInventoryPosition();
		}
		break;

	case EInventoryUIState::Open:
		// Selection is now stick-driven via HandleNavigateAxis*. No tick work.
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

	// Play menu open sound
	if (MenuOpenSound)
	{
		UGameplayStatics::PlaySound2D(this, MenuOpenSound);
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

	// Spawn or unhide the UI actor
	SpawnInventoryUIActor();

	if (AFirstPersonCharacter* FirstPersonCharacter = Cast<AFirstPersonCharacter>(GetOwner()))
	{
		const float ThumbnailSize = 8.0f;
		const float ThumbnailSpacing = 2.0f;
		const float GridWidth = GridColumns * ThumbnailSize + (GridColumns - 1) * ThumbnailSpacing;
		const float GridHeight = GridRows * (ThumbnailSize * 1.4f) + (GridRows - 1) * ThumbnailSpacing;
		FirstPersonCharacter->SetInventoryFlashlightSize(GridWidth, GridHeight);
	}

	// Reset selection to first slot
	SelectedIndex = 0;
	bArmedX = true;
	bArmedY = true;

	CurrentState = EInventoryUIState::Opening;
	FreezePlayerMovement();
	BindConfirmInput();
	BindNavigateInput();

	// Disable interactions with environment
	if (AMyCharacter* MyCharacter = Cast<AMyCharacter>(GetOwner()))
	{
		MyCharacter->SetCanInteract(false);
	}

	// Update UI with current selection and active item
	if (InventoryUIActor)
	{
		InventoryUIActor->SetSelectedIndex(SelectedIndex);

		// Only refresh display if inventory changed since last open
		if (bInventoryNeedsRefresh)
		{
			InventoryUIActor->RefreshDisplay();
			bInventoryNeedsRefresh = false;
		}

		// Show current active item and border (if any)
		if (InventoryComponent)
		{
			FName ActiveItem = InventoryComponent->GetActiveItem();
			int32 ActiveIndex = -1;
			if (!ActiveItem.IsNone())
			{
				TArray<FName> Items = InventoryComponent->GetItems();
				ActiveIndex = Items.IndexOfByKey(ActiveItem);
			}
			InventoryUIActor->SetActiveItem(ActiveItem, ActiveIndex);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("Opening Inventory UI"));
}

void UInventoryUIComponent::CloseInventoryUI()
{
	if (CurrentState == EInventoryUIState::Closed || CurrentState == EInventoryUIState::Closing)
	{
		return;
	}

	// Play menu close sound
	if (MenuCloseSound)
	{
		UGameplayStatics::PlaySound2D(this, MenuCloseSound);
	}

	CurrentState = EInventoryUIState::Closing;
	UnbindNavigateInput();

	if (AFirstPersonCharacter* FirstPersonCharacter = Cast<AFirstPersonCharacter>(GetOwner()))
	{
		FirstPersonCharacter->SetInventoryFlashlightEnabled(false);
	}

	UE_LOG(LogTemp, Log, TEXT("Closing Inventory UI"));
}

bool UInventoryUIComponent::IsInventoryOpen() const
{
	return CurrentState == EInventoryUIState::Open || CurrentState == EInventoryUIState::Opening;
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

			// Play item selected sound
			if (MenuItemSelectedSound)
			{
				UGameplayStatics::PlaySound2D(this, MenuItemSelectedSound);
			}

			// Update the UI to show the confirmed item name and border
			if (InventoryUIActor)
			{
				InventoryUIActor->SetActiveItem(SelectedItem, SelectedIndex);
			}

			UE_LOG(LogTemp, Log, TEXT("Confirmed selection: %s"), *SelectedItem.ToString());
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
	// Reuse existing cached actor if available
	if (InventoryUIActor)
	{
		if (USceneComponent* Root = InventoryUIActor->GetRootComponent())
		{
			Root->SetVisibility(true, true);
		}
		InventoryUIActor->SetActorEnableCollision(true);
		InventoryUIActor->SetActorTickEnabled(true);
		UE_LOG(LogTemp, Log, TEXT("Reusing cached InventoryUIActor"));
		return;
	}

	// First time: spawn the actor
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
		if (USceneComponent* Root = InventoryUIActor->GetRootComponent())
		{
			Root->SetVisibility(false, true);
		}
		InventoryUIActor->SetActorEnableCollision(false);
		InventoryUIActor->SetActorTickEnabled(false);
		UE_LOG(LogTemp, Log, TEXT("Hid InventoryUIActor (cached for reuse)"));
	}
}

void UInventoryUIComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(InventoryUIActor))
	{
		InventoryUIActor->Destroy();
		InventoryUIActor = nullptr;
	}
	Super::EndPlay(EndPlayReason);
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

bool UInventoryUIComponent::SetSelectedIndexForTest(int32 Index)
{
	const int32 TotalSlots = GridColumns * GridRows;
	if (Index < 0 || Index >= TotalSlots)
	{
		UE_LOG(LogTemp, Warning, TEXT("SetSelectedIndexForTest - index %d out of range [0, %d)"), Index, TotalSlots);
		return false;
	}

	SelectedIndex = Index;
	if (InventoryUIActor)
	{
		InventoryUIActor->SetSelectedIndex(SelectedIndex);
	}
	return true;
}

void UInventoryUIComponent::BindNavigateInput()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC || !PC->InputComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("UInventoryUIComponent::BindNavigateInput - no PC/InputComponent"));
		return;
	}

	PC->InputComponent->BindAxis("Move Right / Left", this, &UInventoryUIComponent::HandleNavigateAxisX);
	PC->InputComponent->BindAxis("Move Forward / Backward", this, &UInventoryUIComponent::HandleNavigateAxisY);
}

void UInventoryUIComponent::UnbindNavigateInput()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC || !PC->InputComponent)
	{
		return;
	}
	// Wholesale clear - mirrors AMovieBox::StopInspection.
	PC->InputComponent->AxisBindings.Empty();
}

void UInventoryUIComponent::HandleNavigateAxisX(float AxisValue)
{
	if (CurrentState != EInventoryUIState::Open && CurrentState != EInventoryUIState::Opening)
	{
		return;
	}

	constexpr float FireThreshold = 0.5f;
	constexpr float RearmThreshold = 0.2f;

	const float AbsValue = FMath::Abs(AxisValue);
	if (!bArmedX)
	{
		if (AbsValue < RearmThreshold)
		{
			bArmedX = true;
		}
		return;
	}

	if (AbsValue > FireThreshold)
	{
		bArmedX = false;
		StepSelection(AxisValue > 0.0f ? 1 : -1, 0);
	}
}

void UInventoryUIComponent::HandleNavigateAxisY(float AxisValue)
{
	if (CurrentState != EInventoryUIState::Open && CurrentState != EInventoryUIState::Opening)
	{
		return;
	}

	constexpr float FireThreshold = 0.5f;
	constexpr float RearmThreshold = 0.2f;

	const float AbsValue = FMath::Abs(AxisValue);
	if (!bArmedY)
	{
		if (AbsValue < RearmThreshold)
		{
			bArmedY = true;
		}
		return;
	}

	if (AbsValue > FireThreshold)
	{
		bArmedY = false;
		// Forward (positive Y) navigates UP a row, which is -GridColumns in row-major linear index.
		StepSelection(0, AxisValue > 0.0f ? -1 : 1);
	}
}

void UInventoryUIComponent::StepSelection(int32 DeltaCol, int32 DeltaRow)
{
	const int32 TotalSlots = GridColumns * GridRows;
	if (TotalSlots <= 0)
	{
		return;
	}

	const int32 CurCol = SelectedIndex % GridColumns;
	const int32 CurRow = SelectedIndex / GridColumns;
	const int32 NewCol = FMath::Clamp(CurCol + DeltaCol, 0, GridColumns - 1);
	const int32 NewRow = FMath::Clamp(CurRow + DeltaRow, 0, GridRows - 1);
	const int32 NewIndex = NewRow * GridColumns + NewCol;

	if (NewIndex == SelectedIndex)
	{
		return;
	}

	SelectedIndex = NewIndex;
	if (InventoryUIActor)
	{
		InventoryUIActor->SetSelectedIndex(SelectedIndex);
	}
}

void UInventoryUIComponent::OnInventoryChanged(const TArray<FName>& CurrentItems)
{
	// Mark that inventory needs refresh
	bInventoryNeedsRefresh = true;

	// If UI is currently open, refresh immediately
	if (InventoryUIActor && IsInventoryOpen())
	{
		ClampSelectedIndex();
		InventoryUIActor->SetSelectedIndex(SelectedIndex);
		InventoryUIActor->RefreshDisplay();
		bInventoryNeedsRefresh = false;
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
