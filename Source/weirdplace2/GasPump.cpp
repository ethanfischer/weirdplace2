#include "GasPump.h"
#include "DiegeticTextComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "UObject/ConstructorHelpers.h"

AGasPump::AGasPump()
{
	ErrorText = FText::FromString(TEXT("SYSTEM ERROR\nPlease see clerk\nfor assistance"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PlaneMat(TEXT("/Engine/BasicShapes/BasicShapeMaterial"));

	auto MakeCover = [&](const TCHAR* Name) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* Cover = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Cover->SetupAttachment(GetStaticMeshComponent());
		Cover->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Cover->SetCastShadow(false);
		Cover->SetStaticMesh(PlaneMesh.Object);
		Cover->SetMaterial(0, PlaneMat.Object);
		return Cover;
	};
	auto MakeText = [&](const TCHAR* Name) -> UDiegeticTextComponent*
	{
		UDiegeticTextComponent* Text = CreateDefaultSubobject<UDiegeticTextComponent>(Name);
		Text->SetupAttachment(GetStaticMeshComponent());
		Text->bFacePlayer = false; // printed on the panel like the WELCOME text it replaces
		Text->SetHorizontalAlignment(EHTA_Center);
		Text->SetVerticalAlignment(EVRTA_TextCenter);
		Text->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return Text;
	};
	FrontScreenCover = MakeCover(TEXT("FrontScreenCover"));
	FrontScreenText = MakeText(TEXT("FrontScreenText"));
	BackScreenCover = MakeCover(TEXT("BackScreenCover"));
	BackScreenText = MakeText(TEXT("BackScreenText"));

	SetErrorVisible(false);
}

void AGasPump::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyScreenLayout();
}

void AGasPump::ApplyScreenLayout()
{
	LayoutFace(FrontScreenCover, FrontScreenText, -1.0f);
	LayoutFace(BackScreenCover, BackScreenText, +1.0f);
}

void AGasPump::LayoutFace(UStaticMeshComponent* Cover, UDiegeticTextComponent* Text, float FaceSign)
{
	// ScreenCenter.Y is authored for the -Y face; negate it for +Y.
	const FVector Center(ScreenCenter.X, -FaceSign * ScreenCenter.Y, ScreenCenter.Z);
	// Panel normal: out of the face, tilted up by ScreenTiltDeg.
	const FVector Normal = FVector(0.0f, FaceSign, 0.0f).RotateAngleAxis(FaceSign * ScreenTiltDeg, FVector::XAxisVector);

	// Plane mesh is 100x100 in XY with +Z normal; sit it a hair proud of the panel.
	Cover->SetRelativeLocation(Center + Normal * 1.0f);
	Cover->SetRelativeRotation(FRotationMatrix::MakeFromZX(Normal, FVector::XAxisVector).Rotator());
	Cover->SetRelativeScale3D(FVector(ScreenSize.X / 100.0f, ScreenSize.Y / 100.0f, 1.0f));

	// Text renders facing +X; put that along the normal, upright.
	Text->SetRelativeLocation(Center + Normal * 1.3f);
	Text->SetRelativeRotation(FRotationMatrix::MakeFromXZ(Normal, FVector::UpVector).Rotator());
	Text->SetWorldSize(ScreenTextSize);
	Text->SetTextRenderColor(ScreenTextColor);
	Text->SetText(ErrorText);
}

void AGasPump::BeginPlay()
{
	Super::BeginPlay();
	for (UStaticMeshComponent* Cover : { FrontScreenCover, BackScreenCover })
	{
		if (UMaterialInstanceDynamic* MID = Cover->CreateAndSetMaterialInstanceDynamic(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), ScreenColor);
		}
	}
	SetErrorVisible(false);
}

void AGasPump::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ErrorTimer);
	}
	Super::EndPlay(EndPlayReason);
}

void AGasPump::OnFlavorInteract()
{
	ShowError();
}

void AGasPump::ShowError()
{
	// The interact carries no instigator; the player pawn is the only presser.
	APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!Player)
	{
		UE_LOG(LogTemp, Error, TEXT("AGasPump %s: no player pawn to pick a face"), *GetName());
		return;
	}
	SetErrorVisible(false);
	const FVector ToPlayer = Player->GetActorLocation() - GetActorLocation();
	bErrorOnBackFace = FVector::DotProduct(ToPlayer, GetActorRightVector()) > 0.0f;
	bShowingError = true;
	SetErrorVisible(true);
	// Another press while it's up just keeps it up a little longer.
	GetWorld()->GetTimerManager().SetTimer(ErrorTimer, this, &AGasPump::HideError, ErrorSeconds, false);
	UE_LOG(LogTemp, Log, TEXT("AGasPump %s: system error shown on %s face"), *GetName(), bErrorOnBackFace ? TEXT("+Y") : TEXT("-Y"));
}

void AGasPump::HideError()
{
	bShowingError = false;
	SetErrorVisible(false);
}

void AGasPump::SetErrorVisible(bool bVisible)
{
	const bool bFront = bVisible && !bErrorOnBackFace;
	const bool bBack = bVisible && bErrorOnBackFace;
	FrontScreenCover->SetVisibility(bFront);
	FrontScreenText->SetVisibility(bFront);
	BackScreenCover->SetVisibility(bBack);
	BackScreenText->SetVisibility(bBack);
}
