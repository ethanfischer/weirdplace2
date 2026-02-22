#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BladderUrgencyComponent.generated.h"

class UCameraComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class WEIRDPLACE2_API UBladderUrgencyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBladderUrgencyComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bladder Urgency")
	float ReminderInterval = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bladder Urgency")
	float PulseDuration = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bladder Urgency")
	float PulseIntensity = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bladder Urgency|Visual")
	TObjectPtr<UMaterialInterface> UrgencyVignetteMaterial = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bladder Urgency|Audio")
	TObjectPtr<USoundBase> UrgencySound = nullptr;

protected:
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	bool InitializeVignetteMaterial();
	void SetVignetteIntensity(float Value);
	void ResetLegacyPostProcessOverrides();
	void StartPulse();

	UPROPERTY()
	UCameraComponent* CachedCamera = nullptr;

	UPROPERTY()
	UMaterialInstanceDynamic* UrgencyVignetteMID = nullptr;

	FTimerHandle ReminderTimerHandle;
	float PulseElapsed = 0.f;
	bool bIsPulsing = false;
};
