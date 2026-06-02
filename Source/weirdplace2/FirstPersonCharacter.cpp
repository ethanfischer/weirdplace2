#include "FirstPersonCharacter.h"
#include "BladderUrgencyComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CrosshairWidget.h"
#include "UI_Dialogue.h"
#include "Interactable.h"
#include "Seneca.h"
#include "Components/WidgetComponent.h"
#include "Rick.h"
#include "Hudson.h"
#include "LookAtPlayerComponent.h"
#include "DialogueWidgetProvider.h"
#include "Inventory.h"
#include "InventoryUIComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerInput.h"
#include "InputCoreTypes.h"
#include "WeirdplaceGameUserSettings.h"
#include "MenuUIComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWidget.h"
#if PLATFORM_LINUX
THIRD_PARTY_INCLUDES_START
#include <SDL3/SDL.h>
THIRD_PARTY_INCLUDES_END
#endif

AFirstPersonCharacter::AFirstPersonCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	// Create first person camera
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(RootComponent);
	FirstPersonCamera->SetRelativeLocation(FVector(0.0f, 0.0f, 64.0f)); // Eye height
	FirstPersonCamera->bUsePawnControlRotation = true;

	// Create item notification mesh (diegetic 3D item display)
	ItemNotificationMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemNotificationMesh"));
	ItemNotificationMesh->SetupAttachment(FirstPersonCamera);
	ItemNotificationMesh->SetRelativeLocation(FVector(30.0f, 0.0f, -8.0f));
	ItemNotificationMesh->SetVisibility(false);
	ItemNotificationMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Create bladder urgency reminder component
	BladderUrgencyComponent = CreateDefaultSubobject<UBladderUrgencyComponent>(TEXT("BladderUrgencyComponent"));

	// Create the menu UI component (mirrors InventoryUIComponent on AMyCharacter)
	MenuUIComponent = CreateDefaultSubobject<UMenuUIComponent>(TEXT("MenuUIComponent"));

	// Auto-load the option-step Input Actions so they don't need to be wired up
	// per-Blueprint. The IMC binds them to d-pad up/down (and W/S/arrows/left-stick
	// up/down); we dispatch to menu/inventory navigation in the handlers.
	static ConstructorHelpers::FObjectFinder<UInputAction> NextOptionFinder(TEXT("/Game/FirstPerson/Input/Actions/IA_NextOption.IA_NextOption"));
	if (NextOptionFinder.Succeeded())
	{
		NextOptionAction = NextOptionFinder.Object;
	}
	static ConstructorHelpers::FObjectFinder<UInputAction> PrevOptionFinder(TEXT("/Game/FirstPerson/Input/Actions/IA_PreviousOption.IA_PreviousOption"));
	if (PrevOptionFinder.Succeeded())
	{
		PreviousOptionAction = PrevOptionFinder.Object;
	}
	static ConstructorHelpers::FObjectFinder<UInputAction> NavLeftFinder(TEXT("/Game/FirstPerson/Input/Actions/IA_NavigateLeft.IA_NavigateLeft"));
	if (NavLeftFinder.Succeeded())
	{
		NavigateLeftAction = NavLeftFinder.Object;
	}
	static ConstructorHelpers::FObjectFinder<UInputAction> NavRightFinder(TEXT("/Game/FirstPerson/Input/Actions/IA_NavigateRight.IA_NavigateRight"));
	if (NavRightFinder.Succeeded())
	{
		NavigateRightAction = NavRightFinder.Object;
	}
}

void AFirstPersonCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Sprite icons (trigger boxes, empty actors, etc.) are editor-only but leak into PIE
	// when the viewport show flags get reset by Blueprint recompiles. Suppress them here
	// so rebuilds can't re-enable them.
	if (UGameViewportClient* GVC = GetWorld()->GetGameViewport())
	{
		GVC->EngineShowFlags.SetBillboardSprites(false);
	}

	// Prefer a Blueprint-authored RectLight component (commonly named "RectLight")
	// so designers can tune it directly in BP and have inventory logic use that light.
	TArray<URectLightComponent*> RectLights;
	GetComponents<URectLightComponent>(RectLights);
	for (URectLightComponent* RectLight : RectLights)
	{
		if (!RectLight)
		{
			continue;
		}

		if (RectLight->GetFName() == TEXT("RectLight"))
		{
			InventoryFlashlightComponent = RectLight;
			break;
		}
	}

	// Ensure inventory light starts disabled at runtime.
	SetInventoryFlashlightEnabled(false);

	// Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		CachedPlayerController = PlayerController;
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}

	// Cache settings singleton for gamepad-aware look scaling.
	CachedSettings = Cast<UWeirdplaceGameUserSettings>(UGameUserSettings::GetGameUserSettings());
	if (!CachedSettings)
	{
		UE_LOG(LogTemp, Error, TEXT("FirstPersonCharacter::BeginPlay - GameUserSettings is not UWeirdplaceGameUserSettings; check DefaultEngine.ini GameUserSettingsClassName"));
	}

	// Create crosshair widget
	if (CrosshairWidgetClass)
	{
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		if (PC)
		{
			CrosshairWidget = CreateWidget<UCrosshairWidget>(PC, CrosshairWidgetClass);
			if (CrosshairWidget)
			{
				CrosshairWidget->AddToViewport(0);
				bCreatedCrosshair = true;
			}
		}
	}

#if PLATFORM_LINUX
	// UE 5.7's LinuxWindow.cpp calls SDL_StartTextInput on every window that
	// accepts input (SDL3 made text-input per-window). On Steam Deck the OS
	// keyboard pops up while text input is active. No in-game text entry, so
	// stop it on every top-level window we can see.
	StopLinuxTextInputOnAllWindows();
#endif
}

#if PLATFORM_LINUX
void AFirstPersonCharacter::StopLinuxTextInputOnAllWindows()
{
	if (!FSlateApplication::IsInitialized())
	{
		UE_LOG(LogTemp, Warning, TEXT("Linux SDL_StopTextInput: Slate not initialized"));
		return;
	}

	auto TryStop = [](const TSharedPtr<SWindow>& Win, const TCHAR* Tag)
	{
		if (!Win.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("Linux SDL_StopTextInput[%s]: window null"), Tag);
			return;
		}
		TSharedPtr<FGenericWindow> NativeWindow = Win->GetNativeWindow();
		if (!NativeWindow.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("Linux SDL_StopTextInput[%s]: native window null"), Tag);
			return;
		}
		SDL_Window* SDLWindow = static_cast<SDL_Window*>(NativeWindow->GetOSWindowHandle());
		if (!SDLWindow)
		{
			UE_LOG(LogTemp, Warning, TEXT("Linux SDL_StopTextInput[%s]: SDL handle null"), Tag);
			return;
		}
		const bool bWasActive = SDL_TextInputActive(SDLWindow);
		const bool bStopped = SDL_StopTextInput(SDLWindow);
		UE_LOG(LogTemp, Display, TEXT("Linux SDL_StopTextInput[%s]: handle=%p wasActive=%d ok=%d"), Tag, SDLWindow, bWasActive, bStopped);
	};

	// Primary path: the SWindow hosting our game viewport
	if (GEngine && GEngine->GameViewport)
	{
		TryStop(GEngine->GameViewport->GetWindow(), TEXT("GameViewport"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Linux SDL_StopTextInput[GameViewport]: no GEngine->GameViewport"));
	}

	// Fallback: every interactive top-level window currently known to Slate
	TArray<TSharedRef<SWindow>> AllWindows = FSlateApplication::Get().GetInteractiveTopLevelWindows();
	UE_LOG(LogTemp, Display, TEXT("Linux SDL_StopTextInput: %d top-level windows"), AllWindows.Num());
	for (int32 i = 0; i < AllWindows.Num(); ++i)
	{
		const FString Tag = FString::Printf(TEXT("TopLevel[%d]"), i);
		TryStop(AllWindows[i], *Tag);
	}
}
#endif

void AFirstPersonCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Diagnostic: trace Slate keyboard focus. On Steam Deck the OS keyboard
	// pops up when SDL/Slate enters text-input mode, which happens when a
	// focused widget claims keyboard focus / supports text entry. Logging
	// every transition makes whatever grabs focus at boot identifiable.
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication& App = FSlateApplication::Get();
		TSharedPtr<SWidget> Current = App.GetUserFocusedWidget(0);
		TSharedPtr<SWidget> Previous = LastFocusedWidget.Pin();
		if (Current != Previous || !bLoggedInitialFocus)
		{
			auto Describe = [](const TSharedPtr<SWidget>& W) -> FString
			{
				if (!W.IsValid()) return TEXT("None");
				return FString::Printf(TEXT("%s [%s]"),
					*W->GetTypeAsString(),
					*W->GetTag().ToString());
			};
			UE_LOG(LogTemp, Display, TEXT("Slate keyboard focus: %s -> %s (supportsKBFocus=%d)"),
				*Describe(Previous),
				*Describe(Current),
				Current.IsValid() ? Current->SupportsKeyboardFocus() : 0);
			LastFocusedWidget = Current;
			bLoggedInitialFocus = true;
		}
	}

	// Update crosshair based on context:
	// - In dialogue: show chat-bubble reticle (dialogue suppresses interaction).
	// - Inventory open: only react to filled inventory slots.
	// - Inventory closed: react to world interactables.
	if (bCreatedCrosshair && IsValid(CrosshairWidget))
	{
		if (IsInAnyDialogue())
		{
			CrosshairWidget->ShowDialogueCrosshair();
		}
		else if (IsDialogueCooldownActive())
		{
			// Suppress interactable reticle during post-dialogue cooldown so the
			// player isn't misled into thinking another E press will do anything.
			CrosshairWidget->ShowNormalCrosshair();
		}
		else
		{
			bool bShouldShowInteractable = false;

			if (UInventoryUIComponent* InventoryUIComp = GetInventoryUIComponent())
			{
				if (InventoryUIComp->IsInventoryOpen())
				{
					// Selection is now stick-driven, so the interactable crosshair is
					// shown whenever the currently selected slot holds a real item.
					// Items is sparse — empty slots are NAME_None and shouldn't show the reticle.
					if (UInventoryComponent* InventoryComp = GetInventoryComponent())
					{
						const int32 SelectedIndex = InventoryUIComp->GetSelectedIndex();
						const TArray<FName> Items = InventoryComp->GetItems();
						bShouldShowInteractable = Items.IsValidIndex(SelectedIndex) && !Items[SelectedIndex].IsNone();
					}
				}
				else
				{
					AActor* HitActor = nullptr;
					bool bDidHitInteractable = false;
					RaycastInteractableCheck(HitActor, bDidHitInteractable);
					bShouldShowInteractable = bDidHitInteractable;
				}
			}

			if (bShouldShowInteractable)
			{
				CrosshairWidget->ShowInteractableCrosshair();
			}
			else
			{
				CrosshairWidget->ShowNormalCrosshair();
			}
		}
	}

	// Slow spin on visible item notification meshes (Zelda-style)
	const float SpinSpeed = 45.0f; // degrees per second
	const FRotator SpinDelta(0.0f, SpinSpeed * DeltaTime, 0.0f);
	if (ItemNotificationMesh && ItemNotificationMesh->IsVisible())
	{
		ItemNotificationMesh->AddRelativeRotation(SpinDelta);
	}
	for (UStaticMeshComponent* Comp : StackNotificationMeshes)
	{
		if (Comp && Comp->IsVisible())
		{
			Comp->AddRelativeRotation(SpinDelta);
		}
	}
}

void AFirstPersonCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Note: IA_Look has no C++ binding here on purpose. The BP event graph
		// (BP_FirstPersonCharacter -> InputAction Look) wires it directly to
		// AddControllerYaw/PitchInput, both of which we override below to apply
		// the sensitivity scale. Adding a C++ binding too would double-process
		// the input.
		if (MoveAction)
		{
			EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AFirstPersonCharacter::HandleMoveInput);
		}
		if (JumpAction)
		{
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AFirstPersonCharacter::HandleJumpStarted);
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AFirstPersonCharacter::HandleJumpCompleted);
		}
		if (InteractAction)
		{
			EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Triggered, this, &AFirstPersonCharacter::HandleInteractTriggered);
			EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Completed, this, &AFirstPersonCharacter::HandleInteractCompleted);
		}
		if (InventoryAction)
		{
			EnhancedInputComponent->BindAction(InventoryAction, ETriggerEvent::Triggered, this, &AFirstPersonCharacter::HandleShowInventory);
			EnhancedInputComponent->BindAction(InventoryAction, ETriggerEvent::Completed, this, &AFirstPersonCharacter::HandleShowInventoryCompleted);
		}
		if (SettingsAction)
		{
			EnhancedInputComponent->BindAction(SettingsAction, ETriggerEvent::Triggered, this, &AFirstPersonCharacter::HandleShowMenu);
			EnhancedInputComponent->BindAction(SettingsAction, ETriggerEvent::Completed, this, &AFirstPersonCharacter::HandleShowMenuCompleted);
		}
		if (NextOptionAction)
		{
			EnhancedInputComponent->BindAction(NextOptionAction, ETriggerEvent::Started, this, &AFirstPersonCharacter::HandleNextOption);
		}
		if (PreviousOptionAction)
		{
			EnhancedInputComponent->BindAction(PreviousOptionAction, ETriggerEvent::Started, this, &AFirstPersonCharacter::HandlePreviousOption);
		}
		if (NavigateLeftAction)
		{
			EnhancedInputComponent->BindAction(NavigateLeftAction, ETriggerEvent::Started, this, &AFirstPersonCharacter::HandleNavigateLeft);
		}
		if (NavigateRightAction)
		{
			EnhancedInputComponent->BindAction(NavigateRightAction, ETriggerEvent::Started, this, &AFirstPersonCharacter::HandleNavigateRight);
		}
		if (BackAction)
		{
			EnhancedInputComponent->BindAction(BackAction, ETriggerEvent::Started, this, &AFirstPersonCharacter::HandleBack);
		}
	}
}

void AFirstPersonCharacter::HandleNextOption()
{
	if (MenuUIComponent && MenuUIComponent->IsFullyOpen())
	{
		MenuUIComponent->NavigateNext();
		return;
	}
	if (UInventoryUIComponent* InvUI = FindComponentByClass<UInventoryUIComponent>())
	{
		if (InvUI->IsInventoryFullyOpen())
		{
			InvUI->NavigateNext();
		}
	}
}

void AFirstPersonCharacter::HandlePreviousOption()
{
	if (MenuUIComponent && MenuUIComponent->IsFullyOpen())
	{
		MenuUIComponent->NavigatePrevious();
		return;
	}
	if (UInventoryUIComponent* InvUI = FindComponentByClass<UInventoryUIComponent>())
	{
		if (InvUI->IsInventoryFullyOpen())
		{
			InvUI->NavigatePrevious();
		}
	}
}

void AFirstPersonCharacter::HandleNavigateLeft()
{
	if (MenuUIComponent && MenuUIComponent->IsFullyOpen())
	{
		MenuUIComponent->AdjustLeft();
		return;
	}
	if (UInventoryUIComponent* InvUI = FindComponentByClass<UInventoryUIComponent>())
	{
		if (InvUI->IsInventoryFullyOpen())
		{
			InvUI->NavigateLeft();
		}
	}
}

void AFirstPersonCharacter::HandleNavigateRight()
{
	if (MenuUIComponent && MenuUIComponent->IsFullyOpen())
	{
		MenuUIComponent->AdjustRight();
		return;
	}
	if (UInventoryUIComponent* InvUI = FindComponentByClass<UInventoryUIComponent>())
	{
		if (InvUI->IsInventoryFullyOpen())
		{
			InvUI->NavigateRight();
		}
	}
}

void AFirstPersonCharacter::HandleBack()
{
	if (MenuUIComponent && MenuUIComponent->IsFullyOpen())
	{
		MenuUIComponent->HandleBack();
		return;
	}
	if (UInventoryUIComponent* InvUI = FindComponentByClass<UInventoryUIComponent>())
	{
		if (InvUI->IsInventoryFullyOpen())
		{
			InvUI->CloseInventoryUI();
		}
	}
}

bool AFirstPersonCharacter::IsGamepadLookActive() const
{
	if (!CachedPlayerController)
	{
		return false;
	}
	// Mouse delta this frame is the authoritative signal: if the mouse moved,
	// the look input came from the mouse regardless of any stick reading.
	// (The previous stick-only check tripped on controller drift and applied
	// the gamepad scale curve to mouse-driven look.)
	float MouseX = 0.f, MouseY = 0.f;
	CachedPlayerController->GetInputMouseDelta(MouseX, MouseY);
	if (FMath::Abs(MouseX) > KINDA_SMALL_NUMBER || FMath::Abs(MouseY) > KINDA_SMALL_NUMBER)
	{
		return false;
	}
	// No mouse motion this frame — fall through to a deflection check above
	// the typical drift band (~0.05–0.10 on most pads).
	const float StickX = CachedPlayerController->GetInputAnalogKeyState(EKeys::Gamepad_RightX);
	const float StickY = CachedPlayerController->GetInputAnalogKeyState(EKeys::Gamepad_RightY);
	return FMath::Abs(StickX) > 0.15f || FMath::Abs(StickY) > 0.15f;
}

float AFirstPersonCharacter::ComputeGamepadLookScale() const
{
	if (!CachedSettings)
	{
		return 1.0f;
	}
	const float V = CachedSettings->GetGamepadLookSensitivity();
	return (V * V) * UWeirdplaceGameUserSettings::GamepadLookSensitivityScaleFactor;
}

float AFirstPersonCharacter::ComputeMouseLookScale() const
{
	if (!CachedSettings)
	{
		return 1.0f;
	}
	const float V = CachedSettings->GetMouseLookSensitivity();
	return V * UWeirdplaceGameUserSettings::MouseLookSensitivityScaleFactor;
}

void AFirstPersonCharacter::AddControllerYawInput(float Val)
{
	const float Scale = IsGamepadLookActive() ? ComputeGamepadLookScale() : ComputeMouseLookScale();
	Super::AddControllerYawInput(Val * Scale);
}

void AFirstPersonCharacter::AddControllerPitchInput(float Val)
{
	const float Scale = IsGamepadLookActive() ? ComputeGamepadLookScale() : ComputeMouseLookScale();
	Super::AddControllerPitchInput(Val * Scale);
}

void AFirstPersonCharacter::HandleMoveInput(const FInputActionValue& Value)
{
	FVector2D MovementVector = Value.Get<FVector2D>();

	FVector RightVector = GetActorRightVector();
	AddMovementInput(RightVector, MovementVector.X, false);

	FVector ForwardVector = GetActorForwardVector();
	AddMovementInput(ForwardVector, MovementVector.Y, false);
}

void AFirstPersonCharacter::HandleJumpStarted()
{
	Jump();
}

void AFirstPersonCharacter::HandleJumpCompleted()
{
	StopJumping();
}

void AFirstPersonCharacter::HandleInteractTriggered()
{
	if (bInteractDoOnceCompleted)
	{
		return;
	}
	bInteractDoOnceCompleted = true;

	// While the menu is open, the Interact action is the menu's confirm button.
	// Suppress raycasting/dialogue advance so a single press doesn't double-fire.
	if (MenuUIComponent && MenuUIComponent->IsOpen())
	{
		MenuUIComponent->HandleConfirm();
		return;
	}

	// If in dialogue, advance it instead of raycasting
	EPlayerActivityState State = GetActivityState();
	if (State == EPlayerActivityState::InSimpleDialogue)
	{
		AdvanceSimpleDialogue();
		return;
	}
	if (State == EPlayerActivityState::InDialogue)
	{
		AdvanceDialogue();
		return;
	}

	// Check if we can interact
	if (!GetCanInteract() || IsDialogueCooldownActive())
	{
		return;
	}

	AActor* HitActor = nullptr;
	bool bDidHitInteractable = false;
	RaycastInteractableCheck(HitActor, bDidHitInteractable);

	UE_LOG(LogTemp, Warning, TEXT("HandleInteractTriggered - bDidHit=%d, HitActor=%s, Class=%s"),
		bDidHitInteractable,
		HitActor ? *HitActor->GetName() : TEXT("null"),
		HitActor ? *HitActor->GetClass()->GetName() : TEXT("null"));

	if (bDidHitInteractable && HitActor)
	{
		if (HitActor->GetClass()->ImplementsInterface(UInteractable::StaticClass()))
		{
			IInteractable::Execute_Interact(HitActor);
		}
	}
}

void AFirstPersonCharacter::HandleInteractCompleted()
{
	// Reset DoOnce
	bInteractDoOnceCompleted = false;
}

void AFirstPersonCharacter::HandleShowInventory()
{
	if (MenuUIComponent && MenuUIComponent->IsOpen())
	{
		return;
	}
	if (bInventoryDoOnceCompleted)
	{
		return;
	}
	bInventoryDoOnceCompleted = true;

	if (!IsInventoryUnlocked())
	{
		UE_LOG(LogTemp, Log, TEXT("HandleShowInventory - inventory not yet unlocked (talk to Seneca first)"));
		return;
	}

	if (GetActivityState() != EPlayerActivityState::FreeRoaming)
	{
		return;
	}

	if (UInventoryUIComponent* UI = GetInventoryUIComponent())
	{
		UI->ToggleInventoryUI();
	}
}

void AFirstPersonCharacter::HandleShowInventoryCompleted()
{
	// Reset DoOnce
	bInventoryDoOnceCompleted = false;
}

void AFirstPersonCharacter::HandleShowMenu()
{
	if (bMenuDoOnceCompleted)
	{
		return;
	}
	bMenuDoOnceCompleted = true;

	if (UInventoryUIComponent* InvUI = GetInventoryUIComponent())
	{
		if (InvUI->IsInventoryOpen())
		{
			return;
		}
	}

	if (!MenuUIComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("HandleShowMenu - MenuUIComponent is null"));
		return;
	}

	// Allow toggle while in Interacting state (the menu UI itself sets that state).
	if (GetActivityState() != EPlayerActivityState::FreeRoaming && !MenuUIComponent->IsOpen())
	{
		return;
	}

	MenuUIComponent->ToggleMenu();
}

void AFirstPersonCharacter::HandleShowMenuCompleted()
{
	bMenuDoOnceCompleted = false;
}

void AFirstPersonCharacter::SetInventoryFlashlightEnabled(bool bEnabled)
{
	if (!InventoryFlashlightComponent)
	{
		return;
	}

	InventoryFlashlightComponent->SetVisibility(bEnabled);
	InventoryFlashlightComponent->SetHiddenInGame(!bEnabled);
}

bool AFirstPersonCharacter::IsInventoryFlashlightEnabled() const
{
	return InventoryFlashlightComponent && InventoryFlashlightComponent->IsVisible();
}

void AFirstPersonCharacter::SetInventoryFlashlightSize(float Width, float Height)
{
	if (!InventoryFlashlightComponent)
	{
		return;
	}

	InventoryFlashlightComponent->SetSourceWidth(FMath::Max(Width, 1.0f));
	InventoryFlashlightComponent->SetSourceHeight(FMath::Max(Height, 1.0f));
}

void AFirstPersonCharacter::ShowItemNotification(const FInventoryItemData& ItemData, const FRotator& InitialRotation)
{
	if (!ItemNotificationMesh || !ItemData.Mesh)
	{
		UE_LOG(LogTemp, Error, TEXT("ShowItemNotification - Missing mesh component or item mesh"));
		return;
	}

	ItemNotificationMesh->SetStaticMesh(ItemData.Mesh);
	for (int32 i = 0; i < ItemData.Materials.Num(); i++)
	{
		ItemNotificationMesh->SetMaterial(i, ItemData.Materials[i]);
	}

	// Auto-scale: normalize the mesh so its longest axis fits ~8cm
	const FBox MeshBox = ItemData.Mesh->GetBoundingBox();
	const FVector Extents = MeshBox.GetExtent(); // half-extents
	const float MaxExtent = FMath::Max3(Extents.X, Extents.Y, Extents.Z);
	const float DesiredHalfSize = 4.0f; // 4cm half = 8cm total
	const float UniformScale = (MaxExtent > KINDA_SMALL_NUMBER) ? (DesiredHalfSize / MaxExtent) : 1.0f;
	ItemNotificationMesh->SetRelativeScale3D(FVector(UniformScale));

	ItemNotificationMesh->SetRelativeRotation(InitialRotation);
	ItemNotificationMesh->SetVisibility(true);

	GetWorldTimerManager().ClearTimer(ItemNotificationTimerHandle);
	GetWorldTimerManager().SetTimer(ItemNotificationTimerHandle, [this]()
	{
		if (ItemNotificationMesh)
		{
			ItemNotificationMesh->SetVisibility(false);
		}
	}, 3.0f, false);
}

void AFirstPersonCharacter::ShowItemNotificationStack(const TArray<FInventoryItemData>& Items, const FRotator& ItemRotation)
{
	ClearItemNotificationStack();

	if (!FirstPersonCamera || Items.Num() == 0)
	{
		return;
	}

	const FVector BaseOffset(30.0f, 0.0f, -8.0f);
	float CurrentZ = 0.0f;

	for (const FInventoryItemData& ItemData : Items)
	{
		if (!ItemData.Mesh)
		{
			continue;
		}

		UStaticMeshComponent* MeshComp = NewObject<UStaticMeshComponent>(this);
		MeshComp->SetupAttachment(FirstPersonCamera);
		MeshComp->SetStaticMesh(ItemData.Mesh);
		for (int32 i = 0; i < ItemData.Materials.Num(); i++)
		{
			MeshComp->SetMaterial(i, ItemData.Materials[i]);
		}

		// Use the item's own scale to preserve aspect ratio (e.g. flat VHS-case shape)
		// then apply a uniform multiplier so the largest axis fits ~8cm
		const FBox MeshBox = ItemData.Mesh->GetBoundingBox();
		const FVector Extents = MeshBox.GetExtent();
		const FVector ScaledExtents = Extents * ItemData.Scale;
		const float MaxScaledExtent = FMath::Max3(ScaledExtents.X, ScaledExtents.Y, ScaledExtents.Z);
		const float DesiredHalfSize = 4.0f;
		const float SizeMultiplier = (MaxScaledExtent > KINDA_SMALL_NUMBER) ? (DesiredHalfSize / MaxScaledExtent) : 1.0f;
		const FVector FinalScale = ItemData.Scale * SizeMultiplier;
		MeshComp->SetRelativeScale3D(FinalScale);

		MeshComp->SetRelativeLocation(BaseOffset + FVector(0.0f, 0.0f, CurrentZ));
		MeshComp->SetRelativeRotation(ItemRotation);
		MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MeshComp->SetVisibility(true);
		MeshComp->RegisterComponent();

		// Compute stack height from scaled + rotated bounding box
		const FVector FinalExtents = Extents * FinalScale;
		const FRotationMatrix RotMatrix(ItemRotation);
		const float RotatedHalfHeight =
			FMath::Abs(RotMatrix.TransformVector(FVector(FinalExtents.X, 0, 0)).Z) +
			FMath::Abs(RotMatrix.TransformVector(FVector(0, FinalExtents.Y, 0)).Z) +
			FMath::Abs(RotMatrix.TransformVector(FVector(0, 0, FinalExtents.Z)).Z);
		CurrentZ += RotatedHalfHeight * 2.0f;

		StackNotificationMeshes.Add(MeshComp);
	}

	UE_LOG(LogTemp, Log, TEXT("ShowItemNotificationStack - Showing %d items"), StackNotificationMeshes.Num());
}

void AFirstPersonCharacter::ClearItemNotificationStack()
{
	for (UStaticMeshComponent* Comp : StackNotificationMeshes)
	{
		if (Comp)
		{
			Comp->DestroyComponent();
		}
	}
	StackNotificationMeshes.Empty();
}

bool AFirstPersonCharacter::IsItemNotificationVisible() const
{
	if (ItemNotificationMesh && ItemNotificationMesh->IsVisible())
	{
		return true;
	}
	for (const UStaticMeshComponent* Comp : StackNotificationMeshes)
	{
		if (Comp && Comp->IsVisible())
		{
			return true;
		}
	}
	return false;
}

void AFirstPersonCharacter::RaycastInteractableCheck(AActor*& OutHitActor, bool& bDidHitInteractable)
{
	OutHitActor = nullptr;
	bDidHitInteractable = false;

	if (!FirstPersonCamera)
	{
		return;
	}

	FVector Start = FirstPersonCamera->GetComponentLocation();
	FVector End = Start + (FirstPersonCamera->GetForwardVector() * InteractionDistance);

	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_GameTraceChannel6));

	TArray<AActor*> ActorsToIgnore;

	TArray<FHitResult> HitResults;
	bool bHit = UKismetSystemLibrary::LineTraceMultiForObjects(
		this, Start, End, ObjectTypes,
		false, ActorsToIgnore, EDrawDebugTrace::None, HitResults, true);

	if (!bHit)
	{
		return;
	}

	for (const FHitResult& HitResult : HitResults)
	{
		if (!HitResult.bBlockingHit)
		{
			continue;
		}

		AActor* HitActor = HitResult.GetActor();
		if (!HitActor)
		{
			continue;
		}

		if (!HitActor->GetClass()->ImplementsInterface(UInteractable::StaticClass()))
		{
			// Non-interactable solid geometry (wall, furniture) — stop searching.
			return;
		}

		if (IInteractable* Interactable = Cast<IInteractable>(HitActor);
			Interactable && !Interactable->CanInteract())
		{
			continue;
		}
		// NPC-specific checks: LoS (prevents through-wall capsule interaction)
		// and range (LookAtPlayerComponent sphere). Only apply to actors that
		// have a LookAtPlayerComponent — doors/props don't need these.
		if (HitActor->FindComponentByClass<ULookAtPlayerComponent>())
		{
			if (!IsLineOfSightClearToActor(Start, HitActor, ActorsToIgnore))
			{
				continue;
			}
			if (!IsWithinNPCInteractionRange(HitActor))
			{
				continue;
			}
		}

		OutHitActor = HitActor;
		bDidHitInteractable = true;
		return;
	}
}

bool AFirstPersonCharacter::IsLineOfSightClearToActor(const FVector& CameraLocation, AActor* Target, const TArray<AActor*>& AdditionalIgnoreActors) const
{
	// Trace toward the target's XY position at camera height — not to ImpactPoint
	// (collider may poke through a wall) or actor origin (often at feet, dips into floor).
	const FVector ActorLoc = Target->GetActorLocation();
	const FVector LoSTarget(ActorLoc.X, ActorLoc.Y, CameraLocation.Z);

	TArray<AActor*> IgnoreActors = AdditionalIgnoreActors;
	IgnoreActors.Add(Target);

	FHitResult LoSHit;
	bool bBlocked = UKismetSystemLibrary::LineTraceSingle(
		GetWorld(), CameraLocation, LoSTarget,
		UEngineTypes::ConvertToTraceType(ECC_Visibility),
		false, IgnoreActors, EDrawDebugTrace::None, LoSHit, true,
		FLinearColor::Red, FLinearColor::Green, 0.0f);

	return !bBlocked;
}

bool AFirstPersonCharacter::IsWithinNPCInteractionRange(AActor* Target) const
{
	// If the actor has a LookAtPlayerComponent, require the player to be inside
	// its sphere. Actors without this component are always in range.
	if (ULookAtPlayerComponent* LookAtComp = Target->FindComponentByClass<ULookAtPlayerComponent>())
	{
		return LookAtComp->IsOverlappingActor(this);
	}
	return true;
}

void AFirstPersonCharacter::StartSimpleDialogue(const FText& SpeakerName, const TArray<FText>& Lines, UObject* NPC)
{
	if (Lines.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("StartSimpleDialogue - No lines to display"));
		return;
	}

	UI_Dialogue = nullptr;
	if (IDialogueWidgetProvider* Provider = Cast<IDialogueWidgetProvider>(NPC))
	{
		UI_Dialogue = Provider->GetDialogueWidget();
	}
	if (!UI_Dialogue)
	{
		UE_LOG(LogTemp, Error, TEXT("StartSimpleDialogue - NPC does not provide a dialogue widget"));
		return;
	}

	SimpleDialogueLines = Lines;
	SimpleDialogueLineIndex = 0;
	SimpleDialogueSpeaker = SpeakerName;
	SetActivityState(EPlayerActivityState::InSimpleDialogue);
	CurrentDialogueNPC = NPC;

	UI_Dialogue->OpenWithText(SimpleDialogueSpeaker, SimpleDialogueLines[0]);
}

void AFirstPersonCharacter::AdvanceSimpleDialogue()
{
	if (ItemNotificationMesh)
	{
		ItemNotificationMesh->SetVisibility(false);
		GetWorldTimerManager().ClearTimer(ItemNotificationTimerHandle);
	}
	ClearItemNotificationStack();

	SimpleDialogueLineIndex++;

	if (SimpleDialogueLineIndex < SimpleDialogueLines.Num())
	{
		if (UI_Dialogue)
		{
			UI_Dialogue->UpdateWithText(SimpleDialogueSpeaker, SimpleDialogueLines[SimpleDialogueLineIndex]);
		}
	}
	else
	{
		// Dialogue exhausted
		SetActivityState(EPlayerActivityState::FreeRoaming);
		SimpleDialogueLines.Empty();
		SimpleDialogueLineIndex = 0;

		if (UI_Dialogue)
		{
			UI_Dialogue->Close();
			UI_Dialogue = nullptr;
		}

		UObject* EndedNPC = CurrentDialogueNPC;
		CurrentDialogueNPC = nullptr;
		if (ASeneca* Seneca = Cast<ASeneca>(EndedNPC))
		{
			Seneca->OnDialogueEnded();
		}
		else if (AHudson* Hudson = Cast<AHudson>(EndedNPC))
		{
			Hudson->OnDialogueEnded();
		}
	}
}

void AFirstPersonCharacter::StartDialogue(const TArray<FSimpleDialogueLine>& Lines, UObject* NPC)
{
	if (Lines.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("StartDialogue - No lines to display"));
		return;
	}

	UI_Dialogue = nullptr;
	if (IDialogueWidgetProvider* Provider = Cast<IDialogueWidgetProvider>(NPC))
	{
		UI_Dialogue = Provider->GetDialogueWidget();
	}
	if (!UI_Dialogue)
	{
		UE_LOG(LogTemp, Error, TEXT("StartDialogue - NPC does not provide a dialogue widget"));
		return;
	}

	DialogueLines = Lines;
	DialogueLineIndex = 0;
	SetActivityState(EPlayerActivityState::InDialogue);
	CurrentDialogueNPC = NPC;

	UI_Dialogue->OpenWithText(DialogueLines[0].Speaker, DialogueLines[0].Text);

	OnDialogueLineShown.Broadcast(0);
}

void AFirstPersonCharacter::AdvanceDialogue()
{
	if (ItemNotificationMesh)
	{
		ItemNotificationMesh->SetVisibility(false);
		GetWorldTimerManager().ClearTimer(ItemNotificationTimerHandle);
	}
	ClearItemNotificationStack();

	// If blocked, consume the advance: hide dialogue and broadcast the current index
	// so external systems (e.g. CarRideComponent) can play an interstitial beat.
	if (bBlockNextDialogueAdvance)
	{
		bBlockNextDialogueAdvance = false;

		if (UI_Dialogue)
		{
			UI_Dialogue->Close();
		}

		OnDialogueLineShown.Broadcast(DialogueLineIndex);
		return;
	}

	DialogueLineIndex++;

	if (DialogueLineIndex < DialogueLines.Num())
	{
		if (UI_Dialogue)
		{
			const FSimpleDialogueLine& Line = DialogueLines[DialogueLineIndex];
			UI_Dialogue->OpenWithText(Line.Speaker, Line.Text);
		}

		OnDialogueLineShown.Broadcast(DialogueLineIndex);
	}
	else
	{
		// Dialogue exhausted
		SetActivityState(EPlayerActivityState::FreeRoaming);
		DialogueLines.Empty();
		DialogueLineIndex = 0;

		if (UI_Dialogue)
		{
			UI_Dialogue->Close();
			UI_Dialogue = nullptr;
		}

		UObject* EndedNPC = CurrentDialogueNPC;
		CurrentDialogueNPC = nullptr;
		if (ASeneca* Seneca = Cast<ASeneca>(EndedNPC))
		{
			Seneca->OnDialogueEnded();
		}
		else if (ARick* Rick = Cast<ARick>(EndedNPC))
		{
			Rick->OnDialogueEnded();
		}
		else if (AHudson* Hudson = Cast<AHudson>(EndedNPC))
		{
			Hudson->OnDialogueEnded();
		}
	}
}
