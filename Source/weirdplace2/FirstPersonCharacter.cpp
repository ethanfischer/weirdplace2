#include "FirstPersonCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/WidgetComponent.h"
#include "CrosshairWidget.h"
#include "UI_Dialogue.h"
#include "Interactable.h"
#include "Seneca.h"
#include "InventoryUI.h"
#include "Inventory.h"
#include "InventoryUIComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "DlgSystem/DlgDialogue.h"
#include "DlgSystem/DlgContext.h"
#include "DlgSystem/DlgManager.h"
#include "DlgSystem/DlgDialogueParticipant.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"

AFirstPersonCharacter::AFirstPersonCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// Create first person camera
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(RootComponent);
	FirstPersonCamera->SetRelativeLocation(FVector(0.0f, 0.0f, 64.0f)); // Eye height
	FirstPersonCamera->bUsePawnControlRotation = true;
}

void AFirstPersonCharacter::BeginPlay()
{
	Super::BeginPlay();

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

	// Close inventory UI on start
	if (InventoryUIWidgetComponent)
	{
		UUserWidget* UserWidget = InventoryUIWidgetComponent->GetUserWidgetObject();
		if (UserWidget && UserWidget->GetClass()->ImplementsInterface(UInventoryUI::StaticClass()))
		{
			IInventoryUI::Execute_CloseUI(UserWidget);
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
	// - Inventory open: only react to filled inventory slots.
	// - Inventory closed: react to world interactables.
	if (bCreatedCrosshair && IsValid(CrosshairWidget))
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

	// Check if we can interact
	if (!GetCanInteract())
	{
		return;
	}

	AActor* HitActor = nullptr;
	bool bDidHitInteractable = false;
	RaycastInteractableCheck(HitActor, bDidHitInteractable);

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

	if (InventoryUIWidgetComponent)
	{
		UUserWidget* UserWidget = InventoryUIWidgetComponent->GetUserWidgetObject();
		if (UserWidget && UserWidget->GetClass()->ImplementsInterface(UInventoryUI::StaticClass()))
		{
			IInventoryUI::Execute_OpenUI(UserWidget);
		}
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

void AFirstPersonCharacter::RaycastInteractableCheck(AActor*& OutHitActor, bool& bDidHitInteractable)
{
	OutHitActor = nullptr;
	bDidHitInteractable = false;

	if (!FirstPersonCamera)
	{
		return;
	}

	FVector Start = FirstPersonCamera->GetComponentLocation();
	FVector ForwardVector = FirstPersonCamera->GetForwardVector();
	FVector End = Start + (ForwardVector * InteractionDistance);

	// Setup object types to trace
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_GameTraceChannel6)); // Custom channel

	TArray<AActor*> ActorsToIgnore;
	FHitResult HitResult;

	bool bHit = UKismetSystemLibrary::LineTraceSingleForObjects(
		this,
		Start,
		End,
		ObjectTypes,
		false, // bTraceComplex
		ActorsToIgnore,
		EDrawDebugTrace::None,
		HitResult,
		true // bIgnoreSelf
	);

	if (bHit && HitResult.bBlockingHit)
	{
		AActor* HitActor = HitResult.GetActor();
		if (HitActor && HitActor->GetClass()->ImplementsInterface(UInteractable::StaticClass()))
		{
			OutHitActor = HitActor;
			bDidHitInteractable = true;
		}
	}
}

void AFirstPersonCharacter::StartDialogueWithNPC(UDlgDialogue* Dialogue, UObject* NPC)
{
	if (!NPC || !Dialogue)
	{
		return;
	}

	// Get UI_Dialogue from NPC's widget component
	if (ASeneca* Seneca = Cast<ASeneca>(NPC))
	{
		if (Seneca->DialogueWidgetComponent)
		{
			UUserWidget* Widget = Seneca->DialogueWidgetComponent->GetUserWidgetObject();
			UI_Dialogue = Cast<UUI_Dialogue>(Widget);
		}
	}

	if (IsInDialogue)
	{
		SelectDialogueOption(0);
	}
	else
	{
		TArray<UObject*> Participants;
		APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
		Participants.Add(NPC);
		Participants.Add(PlayerPawn);

		// Debug: Check if NPC implements dialogue participant interface
		bool bImplementsInterface = NPC->GetClass()->ImplementsInterface(UDlgDialogueParticipant::StaticClass());
		UE_LOG(LogTemp, Log, TEXT("StartDialogueWithNPC - NPC '%s' implements IDlgDialogueParticipant: %s"),
			*NPC->GetName(), bImplementsInterface ? TEXT("YES") : TEXT("NO"));

		if (bImplementsInterface)
		{
			FName ParticipantName = IDlgDialogueParticipant::Execute_GetParticipantName(NPC);
			UE_LOG(LogTemp, Log, TEXT("StartDialogueWithNPC - NPC returns ParticipantName: '%s'"), *ParticipantName.ToString());
		}

		DialogueContext = UDlgManager::StartDialogue(Dialogue, Participants);

		if (IsValid(DialogueContext))
		{
			if (UI_Dialogue)
			{
				UI_Dialogue->Open(DialogueContext);
			}
			IsInDialogue = true;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Start dialogue has invalid dialogue context!"));
		}
	}
}

void AFirstPersonCharacter::SelectDialogueOption(int32 OptionIndex)
{
	if (!IsValid(DialogueContext))
	{
		return;
	}

	if (!DialogueContext->IsValidOptionIndex(OptionIndex))
	{
		return;
	}

	bool bSuccess = DialogueContext->ChooseOption(OptionIndex);
	if (bSuccess)
	{
		if (UI_Dialogue)
		{
			UI_Dialogue->Update(DialogueContext);
		}
	}
	else
	{
		// Dialogue ended
		DialogueContext = nullptr;
		IsInDialogue = false;
		if (UI_Dialogue)
		{
			UI_Dialogue->Close();
		}
	}
}
