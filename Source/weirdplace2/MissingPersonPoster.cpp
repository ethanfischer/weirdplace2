#include "MissingPersonPoster.h"

#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AMissingPersonPoster::AMissingPersonPoster()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	// Self-lit aged-paper material so the poster reads at night with no scene light.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> WhiteMat(TEXT("/Game/CreatedMaterials/M_PosterPaper.M_PosterPaper"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BlackMat(TEXT("/Game/CreatedMaterials/PureBlack.PureBlack"));

	// Paper backing: thin board, readable face +X. 2cm deep x 50cm wide x 72cm tall.
	Paper = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Paper"));
	Paper->SetupAttachment(Root);
	if (CubeMesh.Succeeded()) { Paper->SetStaticMesh(CubeMesh.Object); }
	if (WhiteMat.Succeeded()) { Paper->SetMaterial(0, WhiteMat.Object); }
	Paper->SetRelativeScale3D(FVector(0.02f, 0.5f, 0.72f));
	Paper->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Paper->SetCastShadow(false);

	// Silhouette photo block (placeholder for a real Seneca-head render).
	Photo = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Photo"));
	Photo->SetupAttachment(Root);
	if (CubeMesh.Succeeded()) { Photo->SetStaticMesh(CubeMesh.Object); }
	if (BlackMat.Succeeded()) { Photo->SetMaterial(0, BlackMat.Object); }
	Photo->SetRelativeScale3D(FVector(0.03f, 0.32f, 0.34f));
	Photo->SetRelativeLocation(FVector(1.0f, 0.f, 3.f));
	Photo->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Photo->SetCastShadow(false);

	// Diegetic text, readable face +X, sitting just in front of the paper.
	HeaderText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("HeaderText"));
	HeaderText->SetupAttachment(Root);
	HeaderText->SetText(FText::FromString(TEXT("MISSING\nPERSON")));
	HeaderText->SetWorldSize(6.f);
	HeaderText->SetTextRenderColor(FColor::Black);
	HeaderText->SetHorizontalAlignment(EHTA_Center);
	HeaderText->SetVerticalAlignment(EVRTA_TextCenter);
	HeaderText->SetRelativeLocation(FVector(1.6f, 0.f, 28.f));
	HeaderText->SetCastShadow(false);

	NameText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("NameText"));
	NameText->SetupAttachment(Root);
	NameText->SetText(FText::FromString(TEXT("SENECA")));
	NameText->SetWorldSize(9.f);
	NameText->SetTextRenderColor(FColor::Black);
	NameText->SetHorizontalAlignment(EHTA_Center);
	NameText->SetVerticalAlignment(EVRTA_TextCenter);
	NameText->SetRelativeLocation(FVector(1.6f, 0.f, -28.f));
	NameText->SetCastShadow(false);
}

void AMissingPersonPoster::BeginPlay()
{
	Super::BeginPlay();
}

FVector AMissingPersonPoster::GetPosterForward() const
{
	return GetActorForwardVector();
}
