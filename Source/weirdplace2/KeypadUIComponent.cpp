#include "KeypadUIComponent.h"
#include "KeypadUIActor.h"
#include "MyCharacter.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

UKeypadUIComponent::UKeypadUIComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UKeypadUIComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!KeypadUIActorClass)
	{
		KeypadUIActorClass = AKeypadUIActor::StaticClass();
	}
}

void UKeypadUIComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(KeypadUIActor))
	{
		KeypadUIActor->Destroy();
		KeypadUIActor = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void UKeypadUIComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	switch (CurrentState)
	{
	case EKeypadUIState::Opening:
		AnimationProgress += DeltaTime / AnimationDuration;
		if (AnimationProgress >= 1.0f)
		{
			AnimationProgress = 1.0f;
			CurrentState = EKeypadUIState::Open;
		}
		UpdateKeypadPosition();
		break;

	case EKeypadUIState::Closing:
		AnimationProgress -= DeltaTime / AnimationDuration;
		if (AnimationProgress <= 0.0f)
		{
			AnimationProgress = 0.0f;
			CurrentState = EKeypadUIState::Closed;
			DestroyKeypadUIActor();
			UnbindCloseInput();
			UnfreezePlayerMovement();

			if (AMyCharacter* MyCharacter = Cast<AMyCharacter>(GetOwner()))
			{
				MyCharacter->SetCanInteract(true);
			}
		}
		else
		{
			UpdateKeypadPosition();
		}
		break;

	case EKeypadUIState::Open:
	case EKeypadUIState::Closed:
		break;
	}
}

void UKeypadUIComponent::OpenForCode(int32 InCodeLength, const FKeypadSubmitDelegate& InDelegate)
{
	if (CurrentState == EKeypadUIState::Open || CurrentState == EKeypadUIState::Opening)
	{
		return;
	}

	CodeLength = FMath::Max(1, InCodeLength);
	EnteredCode.Empty();
	SelectedCell = 0;
	SubmitDelegate = InDelegate;

	if (MenuOpenSound)
	{
		UGameplayStatics::PlaySound2D(this, MenuOpenSound);
	}

	// Store the camera-relative spawn position (UI stays fixed once opened).
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
	{
		FVector CameraLocation;
		FRotator CameraRotation;
		PC->GetPlayerViewPoint(CameraLocation, CameraRotation);

		const FVector ForwardDir = CameraRotation.Vector();
		const FVector UpDir = FRotationMatrix(CameraRotation).GetScaledAxis(EAxis::Z);
		StoredUIPosition = CameraLocation + ForwardDir * KeypadDistance + UpDir * VerticalOffset;
		StoredUIRotation = CameraRotation;
	}

	SpawnKeypadUIActor();

	CurrentState = EKeypadUIState::Opening;
	AnimationProgress = 0.0f;
	FreezePlayerMovement();
	UnbindCloseInput();
	BindCloseInput();

	if (AMyCharacter* MyCharacter = Cast<AMyCharacter>(GetOwner()))
	{
		MyCharacter->SetCanInteract(false);
	}

	if (KeypadUIActor)
	{
		KeypadUIActor->SetCodeLength(CodeLength);
		KeypadUIActor->SetSelectedCell(SelectedCell);
		KeypadUIActor->SetEnteredCode(EnteredCode);
	}

	UE_LOG(LogTemp, Log, TEXT("Opening Keypad UI (code length %d)"), CodeLength);
}

void UKeypadUIComponent::CloseKeypadUI()
{
	if (CurrentState == EKeypadUIState::Closed || CurrentState == EKeypadUIState::Closing)
	{
		return;
	}

	if (MenuCloseSound)
	{
		UGameplayStatics::PlaySound2D(this, MenuCloseSound);
	}

	CurrentState = EKeypadUIState::Closing;
	SubmitDelegate.Unbind();

	UE_LOG(LogTemp, Log, TEXT("Closing Keypad UI"));
}

void UKeypadUIComponent::PressSelectedDigit()
{
	if (CurrentState != EKeypadUIState::Open)
	{
		return;
	}
	if (EnteredCode.Len() >= CodeLength)
	{
		return;
	}

	const int32 Digit = SelectedCell + 1; // cells 0..8 -> digits 1..9
	EnteredCode.AppendInt(Digit);

	if (MenuItemSelectedSound)
	{
		UGameplayStatics::PlaySound2D(this, MenuItemSelectedSound);
	}
	if (KeypadUIActor)
	{
		KeypadUIActor->SetEnteredCode(EnteredCode);
	}

	if (EnteredCode.Len() >= CodeLength)
	{
		SubmitCode();
	}
}

void UKeypadUIComponent::SubmitCode()
{
	const bool bAccepted = SubmitDelegate.IsBound() && SubmitDelegate.Execute(EnteredCode);
	if (!bAccepted)
	{
		if (DenySound)
		{
			UGameplayStatics::PlaySound2D(this, DenySound);
		}
		DenySoundPlayCount++;
		UE_LOG(LogTemp, Log, TEXT("Keypad: wrong code '%s' — deny + close"), *EnteredCode);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("Keypad: correct code '%s' — accepted"), *EnteredCode);
	}
	CloseKeypadUI();
}

void UKeypadUIComponent::NavigateLeft()
{
	if (CurrentState != EKeypadUIState::Open) { return; }
	StepSelection(-1, 0);
}

void UKeypadUIComponent::NavigateRight()
{
	if (CurrentState != EKeypadUIState::Open) { return; }
	StepSelection(1, 0);
}

void UKeypadUIComponent::NavigateNext()
{
	if (CurrentState != EKeypadUIState::Open) { return; }
	StepSelection(0, 1);
}

void UKeypadUIComponent::NavigatePrevious()
{
	if (CurrentState != EKeypadUIState::Open) { return; }
	StepSelection(0, -1);
}

void UKeypadUIComponent::StepSelection(int32 DeltaCol, int32 DeltaRow)
{
	int32 Row = SelectedCell / 3;
	int32 Col = SelectedCell % 3;
	Col = FMath::Clamp(Col + DeltaCol, 0, 2);
	Row = FMath::Clamp(Row + DeltaRow, 0, 2);

	const int32 NewCell = Row * 3 + Col;
	if (NewCell == SelectedCell)
	{
		return;
	}
	SelectedCell = NewCell;
	if (KeypadUIActor)
	{
		KeypadUIActor->SetSelectedCell(SelectedCell);
	}
}

bool UKeypadUIComponent::SetSelectedDigitForTest(int32 CellIndex)
{
	if (CurrentState != EKeypadUIState::Open)
	{
		UE_LOG(LogTemp, Warning, TEXT("SetSelectedDigitForTest - keypad not fully open"));
		return false;
	}
	if (CellIndex < 0 || CellIndex > 8)
	{
		UE_LOG(LogTemp, Warning, TEXT("SetSelectedDigitForTest - cell %d out of range [0,8]"), CellIndex);
		return false;
	}
	SelectedCell = CellIndex;
	if (KeypadUIActor)
	{
		KeypadUIActor->SetSelectedCell(SelectedCell);
	}
	return true;
}

void UKeypadUIComponent::SpawnKeypadUIActor()
{
	if (KeypadUIActor)
	{
		if (USceneComponent* Root = KeypadUIActor->GetRootComponent())
		{
			Root->SetVisibility(true, true);
		}
		KeypadUIActor->SetActorEnableCollision(true);
		KeypadUIActor->SetActorTickEnabled(true);
		return;
	}

	UWorld* World = GetWorld();
	if (!World || !KeypadUIActorClass)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.Owner = GetOwner();

	KeypadUIActor = World->SpawnActor<AKeypadUIActor>(KeypadUIActorClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	if (KeypadUIActor)
	{
		KeypadUIActor->SetCodeLength(CodeLength);
		KeypadUIActor->BuildKeypad();
		UE_LOG(LogTemp, Log, TEXT("Spawned KeypadUIActor"));
	}
}

void UKeypadUIComponent::DestroyKeypadUIActor()
{
	if (KeypadUIActor)
	{
		if (USceneComponent* Root = KeypadUIActor->GetRootComponent())
		{
			Root->SetVisibility(false, true);
		}
		KeypadUIActor->SetActorEnableCollision(false);
		KeypadUIActor->SetActorTickEnabled(false);
	}
}

void UKeypadUIComponent::UpdateKeypadPosition()
{
	if (!KeypadUIActor)
	{
		return;
	}

	const float EasedProgress = FMath::InterpEaseInOut(0.0f, 1.0f, AnimationProgress, 2.0f);

	const FVector UpDir = FRotationMatrix(StoredUIRotation).GetScaledAxis(EAxis::Z);
	const FVector AnimatedPosition = StoredUIPosition - UpDir * AnimationDropDistance * (1.0f - EasedProgress);

	KeypadUIActor->SetActorLocation(AnimatedPosition);
	KeypadUIActor->SetActorRotation(StoredUIRotation);
	KeypadUIActor->SetOpacity(EasedProgress);
}

void UKeypadUIComponent::BindCloseInput()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC || !PC->InputComponent)
	{
		return;
	}
	// Q / gamepad B closes the keypad.
	PC->InputComponent->BindAction("Exit Interaction", IE_Pressed, this, &UKeypadUIComponent::CloseKeypadUI);
}

void UKeypadUIComponent::UnbindCloseInput()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC || !PC->InputComponent)
	{
		return;
	}
	PC->InputComponent->RemoveActionBinding("Exit Interaction", IE_Pressed);
}

void UKeypadUIComponent::FreezePlayerMovement()
{
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (UCharacterMovementComponent* MovementComp = Character->GetCharacterMovement())
		{
			MovementComp->DisableMovement();
		}
	}
}

void UKeypadUIComponent::UnfreezePlayerMovement()
{
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (UCharacterMovementComponent* MovementComp = Character->GetCharacterMovement())
		{
			MovementComp->SetMovementMode(MOVE_Walking);
		}
	}
}
