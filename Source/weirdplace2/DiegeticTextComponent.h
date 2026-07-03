#pragma once

#include "CoreMinimal.h"
#include "Components/TextRenderComponent.h"
#include "DiegeticTextComponent.generated.h"

// World-space text that reads consistently and turns to face the player —
// the shared building block for labels and prompts next to objects, NPCs,
// etc. Inherits the engine default font/material (same as the hand-tuned
// CantCarryText) so it looks right with no asset wiring; just set the text.
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class WEIRDPLACE2_API UDiegeticTextComponent : public UTextRenderComponent
{
	GENERATED_BODY()

public:
	UDiegeticTextComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void OnVisibilityChanged() override;

	// Turn to face the player each frame. Off = static world text.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Diegetic Text")
	bool bFacePlayer = true;

	// Yaw-only: the text turns horizontally to follow the player but stays
	// upright. Off = full look-at (tilts toward the camera).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Diegetic Text")
	bool bYawOnly = true;
};
