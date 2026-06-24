#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "WeirdplacePlayerController.generated.h"

// Player controller whose sole job is to install UWeirdplaceCheatManager as the
// cheat manager class, which is what makes the dev console commands (SkipTo /
// SkipToSmoking / GiveItem) available. The engine only instantiates the cheat
// manager when cheats are enabled — automatic in PIE standalone, non-shipping —
// so this carries no cost in shipping builds.
UCLASS()
class WEIRDPLACE2_API AWeirdplacePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AWeirdplacePlayerController();
};
