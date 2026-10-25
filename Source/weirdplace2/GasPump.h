#pragma once

#include "CoreMinimal.h"
#include "FlavorProp.h"
#include "GasPump.generated.h"

class UDiegeticTextComponent;
class UStaticMeshComponent;

// Forecourt gas pump. The mesh is two-sided: each of its -Y and +Y faces has
// a printed card-reader screen ("WELCOME / Pay Here Insert Card"). Pressing
// the pump covers the screen on the player's side with an error message for
// ErrorSeconds, then the printed screen shows again.
// Pure flavor — nothing in the story reads it. The six BP_GasPump instances in
// FirstPersonMap derive from this.
UCLASS()
class WEIRDPLACE2_API AGasPump : public AFlavorProp
{
	GENERATED_BODY()

public:
	AGasPump();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	bool IsShowingError() const { return bShowingError; }
	// The cover plate currently shown, or null when no error is up.
	UStaticMeshComponent* GetShownCover() const { return bShowingError ? (bErrorOnBackFace ? BackScreenCover : FrontScreenCover) : nullptr; }

	// Flat plates laid over the printed screens while the error shows, one per face.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gas Pump")
	UStaticMeshComponent* FrontScreenCover;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gas Pump")
	UDiegeticTextComponent* FrontScreenText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gas Pump")
	UStaticMeshComponent* BackScreenCover;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gas Pump")
	UDiegeticTextComponent* BackScreenText;

	// --- Screen layout, relative to the pump mesh, for the -Y ("front") face.
	// The printed screen sits at the foot of the dark recess on a panel that
	// leans back. The +Y face is the mirror image and reuses these with Y negated. ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Pump|Screen")
	FVector ScreenCenter = FVector(0.0f, -36.0f, 116.5f);

	// The panel leans back, so its normal points at the player and up by this much.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Pump|Screen")
	float ScreenTiltDeg = 21.0f;

	// Cover plate width x height (units).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Pump|Screen")
	FVector2D ScreenSize = FVector2D(23.5f, 21.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Pump|Screen")
	float ScreenTextSize = 2.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Pump|Screen")
	FLinearColor ScreenColor = FLinearColor(0.85f, 0.85f, 0.80f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Pump|Screen")
	FColor ScreenTextColor = FColor(30, 30, 30);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Pump|Screen", meta = (MultiLine = "true"))
	FText ErrorText;

	// How long the error stays up after a press.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gas Pump|Screen", meta = (ClampMin = "0.1"))
	float ErrorSeconds = 6.0f;

	// Re-lay the cover and text from the Screen* properties. Runs on construction;
	// callable so the layout can be tuned live in PIE.
	UFUNCTION(BlueprintCallable, Category = "Gas Pump")
	void ApplyScreenLayout();

protected:
	virtual void OnFlavorInteract() override;

private:
	// Lays out one face's cover + text. FaceSign is -1 for the -Y face, +1 for +Y.
	void LayoutFace(UStaticMeshComponent* Cover, UDiegeticTextComponent* Text, float FaceSign);
	// Shows the error on the face nearest the player.
	void ShowError();
	void HideError();
	void SetErrorVisible(bool bVisible);

	FTimerHandle ErrorTimer;
	bool bShowingError = false;
	// Which face the current error is on; set by ShowError.
	bool bErrorOnBackFace = false;
};
