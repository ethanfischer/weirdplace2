#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;

namespace ItemGlow
{
	// The shared self-illumination overlay (M_ItemDarkGlow) so dropped / inserted /
	// notification item meshes read on the game's dark floor. Single home for the
	// asset path + the missing-asset error. Returns null (and logs once per call
	// site) if the material can't be found. Callers store it in their own UPROPERTY
	// and apply it via SetOverlayMaterial.
	WEIRDPLACE2_API UMaterialInterface* GetItemGlowMaterial();
}
