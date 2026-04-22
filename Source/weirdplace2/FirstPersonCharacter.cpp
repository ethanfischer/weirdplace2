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
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
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
}

void AFirstPersonCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

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
					if (InventoryUIComp->IsReticleOverGrid())
					{
						if (UInventoryComponent* InventoryComp = GetInventoryComponent())
						{
							const int32 SelectedIndex = InventoryUIComp->GetSelectedIndex();
							const TArray<FName> Items = InventoryComp->GetItems();
							bShouldShowInteractable = Items.IsValidIndex(SelectedIndex);
						}
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
}

void AFirstPersonCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (LookAction)
		{
			EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AFirstPersonCharacter::HandleLookInput);
		}
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
	}
}

void AFirstPersonCharacter::HandleLookInput(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();
	AddControllerYawInput(LookAxisVector.X);
	AddControllerPitchInput(LookAxisVector.Y);
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

void AFirstPersonCharacter::ShowItemNotification(const FInventoryItemData& ItemData)
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

bool AFirstPersonCharacter::IsItemNotificationVisible() const
{
	return ItemNotificationMesh && ItemNotificationMesh->IsVisible();
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
	if (GetActivityState() == EPlayerActivityState::WaitingForItemInteractionInDialogue)
	{
		if (AActor* NPCActor = Cast<AActor>(CurrentDialogueNPC))
		{
			ActorsToIgnore.Add(NPCActor);
		}
	}

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
