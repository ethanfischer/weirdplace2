#include "InventoryUIActor.h"
#include "Inventory.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	UTextRenderComponent* MakeText(AActor* Owner, USceneComponent* Parent, const TCHAR* Name, float Size)
	{
		UTextRenderComponent* Text = Owner->CreateDefaultSubobject<UTextRenderComponent>(Name);
		Text->SetupAttachment(Parent);
		Text->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
		Text->SetWorldSize(Size);
		Text->SetTextRenderColor(FColor::White);
		Text->SetHorizontalAlignment(EHTA_Center);
		Text->SetVerticalAlignment(EVRTA_TextCenter);
		Text->SetText(FText::GetEmpty());
		return Text;
	}
}

AInventoryUIActor::AInventoryUIActor()
{
	PrimaryActorTick.bCanEverTick = true;

	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	SetRootComponent(RootSceneComponent);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMeshAsset(TEXT("/Engine/BasicShapes/Plane.Plane"));
	PlaneMesh = PlaneMeshAsset.Object;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SolidColorAsset(TEXT("/Game/Materials/M_SolidColor.M_SolidColor"));
	SolidColorMaterial = SolidColorAsset.Object;

	BackgroundPanel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BackgroundPanel"));
	BackgroundPanel->SetupAttachment(RootSceneComponent);
	BackgroundPanel->SetStaticMesh(PlaneMesh);
	BackgroundPanel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BackgroundPanel->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
	BackgroundPanel->SetRelativeLocation(FVector(1.0f, 0.0f, 0.0f)); // behind everything

	ItemNameText = MakeText(this, RootSceneComponent, TEXT("ItemNameText"), TitleTextSize);
	ItemNameText->SetHorizontalAlignment(EHTA_Left);
}

void AInventoryUIActor::BeginPlay()
{
	Super::BeginPlay();

	// The UI is self-illuminated (no inventory light), so the text needs an
	// unlit material; the engine's default TextRender material is lit and would
	// render near-black indoors.
	UMaterialInterface* TextMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/MI_MenuText.MI_MenuText"));
	if (!TextMat)
	{
		UE_LOG(LogTemp, Error, TEXT("InventoryUIActor: MI_MenuText not found; item text will be dark."));
	}
	ItemNameText->SetWorldSize(TitleTextSize);
	if (TextMat)
	{
		ItemNameText->SetTextMaterial(TextMat);
	}

	UMaterialInterface* BaseMat = BackgroundPanel->GetMaterial(0);
	if (!BaseMat)
	{
		UE_LOG(LogTemp, Error, TEXT("InventoryUIActor: BackgroundPanel has no material set. Set one on BP_Inventory."));
		return;
	}
	BackgroundMaterial = UMaterialInstanceDynamic::Create(BaseMat, this);
	BackgroundPanel->SetMaterial(0, BackgroundMaterial);
}

void AInventoryUIActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateHoverAnimation(DeltaTime);
}

void AInventoryUIActor::SetInventoryComponent(UInventoryComponent* InInventoryComponent)
{
	InventoryComponent = InInventoryComponent;
}

void AInventoryUIActor::SetSelectedIndex(int32 Index)
{
	const int32 NewIndex = FMath::Clamp(Index, 0, GetTotalSlots() - 1);
	if (NewIndex != SelectedIndex)
	{
		PreviousSelectedIndex = SelectedIndex;
		SelectedIndex = NewIndex;
		HoverAnimationProgress = 0.0f;
		UpdateSelectionHighlight();
	}
}

void AInventoryUIActor::SetGridColumns(int32 Columns)
{
	GridColumns = FMath::Max(1, Columns);
}

void AInventoryUIActor::SetGridRows(int32 Rows)
{
	GridRows = FMath::Max(1, Rows);
}

void AInventoryUIActor::SetScrollOffset(int32 Offset)
{
	const int32 NewOffset = FMath::Max(0, Offset);
	if (NewOffset == ScrollOffset)
	{
		return;
	}
	ScrollOffset = NewOffset;
	RefreshDisplay();
}

void AInventoryUIActor::SetShown(bool bShown)
{
	RootSceneComponent->SetVisibility(bShown, true);
	if (bShown)
	{
		UpdateSelectionHighlight();
	}
}

void AInventoryUIActor::RefreshDisplay()
{
	ClearThumbnails();
	CreateThumbnails();
	UpdateBackgroundSize();
	UpdateSelectionHighlight();
}

void AInventoryUIActor::SetOpacity(float Opacity)
{
	CurrentOpacity = Opacity;

	if (BackgroundMaterial)
	{
		FLinearColor AdjustedColor = BackgroundColor;
		AdjustedColor.A *= Opacity;
		BackgroundMaterial->SetVectorParameterValue(FName("BaseColor"), AdjustedColor);
	}

	for (UStaticMeshComponent* Mesh : ThumbnailMeshes)
	{
		if (UMaterialInstanceDynamic* DynMat = Mesh ? Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0)) : nullptr)
		{
			DynMat->SetScalarParameterValue(FName("Opacity"), Opacity);
		}
	}

	FColor TextColor = TitleColor.ToFColor(true);
	TextColor.A = FMath::Clamp(static_cast<int32>(Opacity * 255), 0, 255);
	ItemNameText->SetTextRenderColor(TextColor);
}

void AInventoryUIActor::CreateThumbnails()
{
	if (!InventoryComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("InventoryUIActor: no inventory component"));
		return;
	}

	const TArray<FName> Items = InventoryComponent->GetItems();
	VisibleCount = FMath::Clamp(Items.Num() - ScrollOffset, 0, GetTotalSlots());

	UMaterialInterface* FrontFaceMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_VHSCoverFront.M_VHSCoverFront"));
	UMaterialInterface* ItemThumbnailMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_ItemThumbnail.M_ItemThumbnail"));
	if (!FrontFaceMaterial || !ItemThumbnailMaterial)
	{
		UE_LOG(LogTemp, Error, TEXT("InventoryUIActor: thumbnail materials missing (M_VHSCoverFront=%d, M_ItemThumbnail=%d)"),
			FrontFaceMaterial != nullptr, ItemThumbnailMaterial != nullptr);
		return;
	}

	// Selection underline: a thin emissive bar just below the selected thumbnail.
	if (!SelectionHighlight)
	{
		SelectionHighlight = NewObject<UStaticMeshComponent>(this);
		SelectionHighlight->SetStaticMesh(PlaneMesh);
		SelectionHighlight->SetupAttachment(RootSceneComponent);
		SelectionHighlight->RegisterComponent();
		SelectionHighlight->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SelectionHighlight->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
		UMaterialInstanceDynamic* FrameMat = UMaterialInstanceDynamic::Create(SolidColorMaterial, this);
		FrameMat->SetVectorParameterValue(FName("Color"), SelectionColor);
		FrameMat->SetVectorParameterValue(FName("EmissiveColor"), SelectionColor);
		SelectionHighlight->SetMaterial(0, FrameMat);
	}

	// One thumbnail per visible slot, so ThumbnailMeshes stays indexed by visible
	// slot (the selection and hover animation index that way).
	for (int32 p = 0; p < GetTotalSlots(); p++)
	{
		const int32 AbsIndex = ScrollOffset + p;
		if (!Items.IsValidIndex(AbsIndex))
		{
			ThumbnailMeshes.Add(nullptr);
			continue;
		}
		const FName ItemID = Items[AbsIndex];

		UStaticMeshComponent* Thumbnail = NewObject<UStaticMeshComponent>(this);
		Thumbnail->SetStaticMesh(PlaneMesh);
		Thumbnail->SetupAttachment(RootSceneComponent);
		Thumbnail->RegisterComponent();
		Thumbnail->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Thumbnail->SetRelativeLocation(CalculateSlotPosition(p));
		Thumbnail->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
		SetThumbnailScale(Thumbnail, 1.0f);

		// Thumbnail override on the item data, else the convention path, else the
		// VHS cover texture (movies are named after their cover).
		UTexture2D* ItemThumbnailTex = InventoryComponent->GetItemData(ItemID).Thumbnail;
		if (!ItemThumbnailTex)
		{
			ItemThumbnailTex = LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/Images/ItemThumbnails/%s_thumbnail"), *ItemID.ToString()));
		}
		if (ItemThumbnailTex)
		{
			UMaterialInstanceDynamic* ThumbnailMat = UMaterialInstanceDynamic::Create(ItemThumbnailMaterial, this);
			ThumbnailMat->SetTextureParameterValue(FName("ThumbnailTexture"), ItemThumbnailTex);
			Thumbnail->SetMaterial(0, ThumbnailMat);
		}
		else if (UTexture2D* CoverTexture = LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/VHSCovers/%s"), *ItemID.ToString())))
		{
			UMaterialInstanceDynamic* FrontFaceMat = UMaterialInstanceDynamic::Create(FrontFaceMaterial, this);
			FrontFaceMat->SetTextureParameterValue(FName("CoverTexture"), CoverTexture);
			Thumbnail->SetMaterial(0, FrontFaceMat);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("InventoryUIActor: no thumbnail or VHS cover for item '%s'"), *ItemID.ToString());
		}

		ThumbnailMeshes.Add(Thumbnail);
	}
}

void AInventoryUIActor::ClearThumbnails()
{
	for (UStaticMeshComponent* Mesh : ThumbnailMeshes)
	{
		if (Mesh)
		{
			Mesh->DestroyComponent();
		}
	}
	ThumbnailMeshes.Empty();
}

void AInventoryUIActor::UpdateSelectionHighlight()
{
	if (!SelectionHighlight)
	{
		return;
	}
	// Only underline a real item; with nothing shown there's nothing to select.
	const bool bHasItem = ThumbnailMeshes.IsValidIndex(SelectedIndex) && ThumbnailMeshes[SelectedIndex] != nullptr;
	SelectionHighlight->SetVisibility(bHasItem);
	const float Grown = ThumbnailSize * HoverScaleMultiplier;
	FVector Position = CalculateSlotPosition(SelectedIndex);
	Position.Z -= Grown * 0.5f + UnderlineGap + UnderlineThickness * 0.5f;
	SelectionHighlight->SetRelativeLocation(Position);
	// Plane is 100x100: local X (after the 90 degree pitch) is height, Y is width.
	SelectionHighlight->SetRelativeScale3D(FVector(UnderlineThickness * 0.01f, Grown * 0.01f, 1.0f));
}

void AInventoryUIActor::SetItemName(const FName& ItemID)
{
	FString ItemName;
	if (ItemID.IsNone())
	{
		ItemName = TEXT("Empty");
	}
	else
	{
		FString Raw = ItemID.ToString();
		Raw = Raw.Replace(TEXT("_"), TEXT(" "));
		Raw = Raw.Replace(TEXT("-"), TEXT(" "));

		// Split camelCase so e.g. "BrokenKey" -> "Broken Key", "BlankVHS" -> "Blank VHS":
		// insert a space before an uppercase letter that follows a lowercase letter or
		// digit. Runs of capitals (acronyms) and already-spaced titles are left intact.
		for (int32 i = 0; i < Raw.Len(); ++i)
		{
			const TCHAR C = Raw[i];
			if (i > 0 && FChar::IsUpper(C) && (FChar::IsLower(Raw[i - 1]) || FChar::IsDigit(Raw[i - 1])))
			{
				ItemName.AppendChar(TEXT(' '));
			}
			ItemName.AppendChar(C);
		}
	}

	// IDs are often ALL CAPS; shown text is title case so it reads as a name, not a shout.
	if (ItemName.ToUpper() == ItemName)
	{
		ItemName = ItemName.ToLower();
		bool bStart = true;
		for (int32 i = 0; i < ItemName.Len(); ++i)
		{
			if (bStart && FChar::IsAlpha(ItemName[i]))
			{
				ItemName[i] = FChar::ToUpper(ItemName[i]);
			}
			bStart = (ItemName[i] == TEXT(' '));
		}
	}

	// Word-wrap long titles to fit above the strip
	const int32 MaxLineLength = 26;
	FString WrappedText;
	int32 CurrentLineLength = 0;
	TArray<FString> Words;
	ItemName.ParseIntoArray(Words, TEXT(" "), true);
	for (const FString& Word : Words)
	{
		if (CurrentLineLength > 0 && CurrentLineLength + 1 + Word.Len() > MaxLineLength)
		{
			WrappedText += TEXT("\n");
			CurrentLineLength = 0;
		}
		else if (CurrentLineLength > 0)
		{
			WrappedText += TEXT(" ");
			CurrentLineLength += 1;
		}
		WrappedText += Word;
		CurrentLineLength += Word.Len();
	}
	ItemNameText->SetText(FText::FromString(WrappedText));

	// Fit inside the panel: shrink the title when a line is wider than the grid.
	const float Avail = GetGridWidth() + BackgroundPadding;
	ItemNameText->SetWorldSize(TitleTextSize);
	const float W = ItemNameText->GetTextLocalSize().Y;
	if (W > Avail)
	{
		ItemNameText->SetWorldSize(TitleTextSize * Avail / W);
	}
}

void AInventoryUIActor::UpdateBackgroundSize()
{
	const float GridW = GetGridWidth();
	const float TotalWidth = GridW + BackgroundPadding * 2.0f;
	const float TotalHeight = ThumbnailSize + BackgroundPadding * 2.0f + TitleBandHeight;

	// Plane is 100x100; the panel's local Y is width, local X (after the 90°
	// pitch) is height. Shift it up by half the title band so the thumbnails stay
	// centered on the actor and the band sits above them.
	BackgroundPanel->SetRelativeScale3D(FVector(TotalHeight * 0.01f, TotalWidth * 0.01f, 1.0f));
	BackgroundPanel->SetRelativeLocation(FVector(1.0f, 0.0f, TitleBandHeight * 0.5f));

	// Title top-left: flush with the first thumbnail's left edge.
	const float TitleZ = ThumbnailSize * 0.5f + BackgroundPadding + TitleBandHeight * 0.5f;
	ItemNameText->SetRelativeLocation(FVector(0.0f, -GridW * 0.5f, TitleZ));
}

FVector AInventoryUIActor::CalculateSlotPosition(int32 Index) const
{
	// The actor faces the camera along +X, so local -Y is the viewer's left:
	// the first shown item sits leftmost.
	// Slots are laid out for the full window, so the pane keeps one size however
	// many items are carried and items pack in from the left.
	const float SlotWidth = ThumbnailSize + ThumbnailSpacing;
	const float Span = (GetTotalSlots() - 1) * SlotWidth;
	return FVector(0.0f, -Span * 0.5f + Index * SlotWidth, 0.0f);
}

float AInventoryUIActor::GetGridWidth() const
{
	const int32 N = GetTotalSlots();
	return N * ThumbnailSize + (N - 1) * ThumbnailSpacing;
}

void AInventoryUIActor::SetThumbnailScale(UStaticMeshComponent* Mesh, float Scale) const
{
	const float Size = ThumbnailSize * Scale;
	Mesh->SetRelativeScale3D(FVector(Size * 0.01f, Size * 0.01f, 1.0f));
}

void AInventoryUIActor::UpdateHoverAnimation(float DeltaTime)
{
	HoverAnimationProgress = FMath::FInterpTo(HoverAnimationProgress, 1.0f, DeltaTime, HoverAnimationSpeed);

	// Grow the selected thumbnail, reset the one we just left.
	const float CurrentScale = FMath::Lerp(1.0f, HoverScaleMultiplier, HoverAnimationProgress);
	if (ThumbnailMeshes.IsValidIndex(SelectedIndex) && ThumbnailMeshes[SelectedIndex])
	{
		SetThumbnailScale(ThumbnailMeshes[SelectedIndex], CurrentScale);
	}
	if (PreviousSelectedIndex >= 0 && PreviousSelectedIndex != SelectedIndex)
	{
		if (ThumbnailMeshes.IsValidIndex(PreviousSelectedIndex) && ThumbnailMeshes[PreviousSelectedIndex])
		{
			SetThumbnailScale(ThumbnailMeshes[PreviousSelectedIndex], 1.0f);
		}
		PreviousSelectedIndex = -1;
	}

}
