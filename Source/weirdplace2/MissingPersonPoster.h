#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MissingPersonPoster.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

// A "MISSING PERSON" poster of Seneca, stapled to the telephone pole. A
// standalone actor (NOT parented to the gated telephone scene) so it stays
// present regardless of the SeenTornadoWarning gate. Placeholder look: paper
// backing + diegetic text + a silhouette photo block; a real Seneca-head
// render can be dropped onto the Photo slot later.
UCLASS()
class WEIRDPLACE2_API AMissingPersonPoster : public AActor
{
	GENERATED_BODY()

public:
	AMissingPersonPoster();

	// World-space outward normal of the poster face (for camera framing in tests).
	FVector GetPosterForward() const;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Poster")
	UStaticMeshComponent* Paper;

	UPROPERTY(VisibleAnywhere, Category = "Poster")
	UStaticMeshComponent* Photo;

	UPROPERTY(VisibleAnywhere, Category = "Poster")
	UTextRenderComponent* HeaderText;

	UPROPERTY(VisibleAnywhere, Category = "Poster")
	UTextRenderComponent* NameText;
};
