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
			// The inventory UI is fully self-illuminated (thumbnails/slots emissive,
			// text uses the unlit M_UnlitText), so no inventory RectLight is needed.
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
			UnbindCloseInput();
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
		// Selection is driven by Enhanced Input nav actions on the character.
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
		// Square cells: grid height uses ThumbnailSize directly (see AInventoryUIActor::SlotHeightAspect).
		const float GridHeight = GridRows * ThumbnailSize + (GridRows - 1) * ThumbnailSpacing;
		FirstPersonCharacter->SetInventoryFlashlightSize(GridWidth, GridHeight);
	}

	// Persist the cursor position across opens — including when the previously-
	// selected item was just given to an NPC, so the player reopens onto the now-empty slot.
	ClampSelectedIndex();

	CurrentState = EInventoryUIState::Opening;
	FreezePlayerMovement();
	// Closing leaves the Exit Interaction binding live until the animation ends;
	// strip any stale binding before adding a new one so we don't accumulate handlers.
	UnbindCloseInput();
	BindCloseInput();

	// Disable interactions with environment
	if (AMyCharacter* MyCharacter = Cast<AMyCharacter>(GetOwner()))
	{
		MyCharacter->SetCanInteract(false);
	}

	// Update UI with current selection and active item
	if (InventoryUIActor)
	{
		// Push the scroll window before any rebuild so thumbnails render the right slice.
		InventoryUIActor->SetScrollOffset(ScrollOffset);

		// Only refresh display if inventory changed since last open
		if (bInventoryNeedsRefresh)
		{
			InventoryUIActor->RefreshDisplay();
			bInventoryNeedsRefresh = false;
		}

		// Sync selection highlight + active item border to the current selection.
		UpdateSelectedSlot();
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
		InventoryUIActor->SetScrollOffset(ScrollOffset);
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

void UInventoryUIComponent::BindCloseInput()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC || !PC->InputComponent) return;

	// Bind close (Q / B button) - closes inventory.
	// Active-item assignment is now driven by navigation, so there is no separate confirm action.
	PC->InputComponent->BindAction("Exit Interaction", IE_Pressed, this, &UInventoryUIComponent::CloseInventoryUI);
}

void UInventoryUIComponent::UnbindCloseInput()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC || !PC->InputComponent) return;

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
	const int32 Count = GetItemCount();
	if (Index < 0 || Index >= Count)
	{
		UE_LOG(LogTemp, Warning, TEXT("SetSelectedIndexForTest - index %d out of range [0, %d)"), Index, Count);
		return false;
	}

	AbsoluteSelectedIndex = Index;
	RecomputeScrollOffset();
	UpdateSelectedSlot();
	return true;
}

void UInventoryUIComponent::NavigatePrevious()
{
	if (CurrentState != EInventoryUIState::Open || GridRows <= 1)
	{
		return;
	}
	StepSelection(0, -1);
}

void UInventoryUIComponent::NavigateNext()
{
	if (CurrentState != EInventoryUIState::Open || GridRows <= 1)
	{
		return;
	}
	StepSelection(0, 1);
}

void UInventoryUIComponent::NavigateLeft()
{
	if (CurrentState != EInventoryUIState::Open)
	{
		return;
	}
	StepSelection(-1, 0);
}

void UInventoryUIComponent::NavigateRight()
{
	if (CurrentState != EInventoryUIState::Open)
	{
		return;
	}
	StepSelection(1, 0);
}

void UInventoryUIComponent::StepSelection(int32 DeltaCol, int32 DeltaRow)
{
	// Single-row horizontal strip: only DeltaCol moves the selection. Selection is
	// an absolute index over the whole item list; the window scrolls to follow it.
	const int32 Count = GetItemCount();
	if (Count <= 0)
	{
		return;
	}

	const int32 NewIndex = FMath::Clamp(AbsoluteSelectedIndex + DeltaCol, 0, Count - 1);
	if (NewIndex == AbsoluteSelectedIndex)
	{
		return;
	}

	AbsoluteSelectedIndex = NewIndex;
	RecomputeScrollOffset();
	UpdateSelectedSlot();
}

void UInventoryUIComponent::UpdateSelectedSlot()
{
	// Push the scroll window first (rebuilds thumbnails only if the offset moved),
	// then position the hover highlight at the selected VISIBLE slot, then set the
	// active item to whatever lives at the absolute slot (NAME_None clears it).
	const int32 VisibleSlot = AbsoluteSelectedIndex - ScrollOffset;
	if (InventoryUIActor)
	{
		InventoryUIActor->SetScrollOffset(ScrollOffset);
		InventoryUIActor->SetSelectedIndex(VisibleSlot);
	}

	if (!InventoryComponent)
	{
		return;
	}

	const TArray<FName> Items = InventoryComponent->GetItems();
	const FName ItemAtSlot = Items.IsValidIndex(AbsoluteSelectedIndex) ? Items[AbsoluteSelectedIndex] : NAME_None;

	const bool bChanged = InventoryComponent->GetActiveItem() != ItemAtSlot;
	InventoryComponent->SetActiveItem(ItemAtSlot);

	if (InventoryUIActor)
	{
		// Pass the ABSOLUTE index; the actor translates to a visible slot and hides
		// the border when the active item is scrolled out of view.
		InventoryUIActor->SetActiveItem(ItemAtSlot, ItemAtSlot.IsNone() ? -1 : AbsoluteSelectedIndex);
	}

	if (bChanged && !ItemAtSlot.IsNone() && MenuItemSelectedSound)
	{
		UGameplayStatics::PlaySound2D(this, MenuItemSelectedSound);
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
		InventoryUIActor->SetScrollOffset(ScrollOffset);
		InventoryUIActor->RefreshDisplay();
		bInventoryNeedsRefresh = false;
		UpdateSelectedSlot();
	}
}

int32 UInventoryUIComponent::GetItemCount() const
{
	return InventoryComponent ? InventoryComponent->GetItems().Num() : 0;
}

void UInventoryUIComponent::RecomputeScrollOffset()
{
	const int32 Count = GetItemCount();
	const int32 VisibleColumns = FMath::Max(1, GridColumns);

	// Slide the window minimally so the selection stays visible.
	if (AbsoluteSelectedIndex < ScrollOffset)
	{
		ScrollOffset = AbsoluteSelectedIndex;
	}
	else if (AbsoluteSelectedIndex >= ScrollOffset + VisibleColumns)
	{
		ScrollOffset = AbsoluteSelectedIndex - VisibleColumns + 1;
	}

	// Keep the window within the list: never scroll past the point where the last
	// item sits at the right edge, and never go negative.
	const int32 MaxOffset = FMath::Max(0, Count - VisibleColumns);
	ScrollOffset = FMath::Clamp(ScrollOffset, 0, MaxOffset);
}

void UInventoryUIComponent::ClampSelectedIndex()
{
	// Selection is an absolute index over the (unbounded) item list. Reconcile it
	// and the scroll window against the current count (e.g. items removed while
	// the UI was closed, or persisted cursor from a previous open).
	const int32 Count = GetItemCount();
	AbsoluteSelectedIndex = (Count <= 0) ? 0 : FMath::Clamp(AbsoluteSelectedIndex, 0, Count - 1);
	RecomputeScrollOffset();
}
