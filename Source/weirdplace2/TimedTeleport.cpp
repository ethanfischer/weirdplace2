// Fill out your copyright notice in the Description page of Project Settings.


#include "TimedTeleport.h"
#include "MyCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

// Sets default values for this component's properties
UTimedTeleport::UTimedTeleport()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UTimedTeleport::BeginPlay()
{
	Super::BeginPlay();

	// ...

}


// Called every frame
void UTimedTeleport::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	Timer += DeltaTime;
	if (Timer <= TimeToTeleport)
	{
		return;
	}

	// Timer expired — run guards. If any guard fails, leave Timer armed so the
	// teleport fires the moment conditions clear (do NOT reset Timer here).

	// Guard 1: defer while player is in any dialogue
	ACharacter* PlayerChar = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	AMyCharacter* MyChar = Cast<AMyCharacter>(PlayerChar);
	if (!MyChar)
	{
		UE_LOG(LogTemp, Error, TEXT("UTimedTeleport - Player is not AMyCharacter"));
		return;
	}
	if (MyChar->IsInAnyDialogue())
	{
		return;
	}

	// Guard 2 & 3: defer while player camera cone covers Rick or his destination
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC)
	{
		UE_LOG(LogTemp, Error, TEXT("UTimedTeleport - No PlayerController"));
		return;
	}

	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);
	const FVector CamForward = CamRot.Vector();

	// ~60 degree half-angle cone (cos 60° ≈ 0.5), matches ASeneca::IsPlayerLookingAt
	const float LookAtCosThreshold = 0.5f;

	const FVector ToOwner = (GetOwner()->GetActorLocation() - CamLoc).GetSafeNormal();
	if (FVector::DotProduct(CamForward, ToOwner) > LookAtCosThreshold)
	{
		return;
	}

	const FVector ToDest = (TeleportLocation - CamLoc).GetSafeNormal();
	if (FVector::DotProduct(CamForward, ToDest) > LookAtCosThreshold)
	{
		return;
	}

	Timer = 0;
	GetOwner()->SetActorTransform(FTransform(TeleportRotation, TeleportLocation));
}

