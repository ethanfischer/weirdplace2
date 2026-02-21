#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BladderUrgencyComponent.generated.h"

class UCameraComponent;

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
	float VignetteMax = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bladder Urgency")
	float TintStrength = 0.3f;

protected:
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void StartPulse();

	UPROPERTY()
	UCameraComponent* CachedCamera = nullptr;

	FTimerHandle ReminderTimerHandle;
	float PulseElapsed = 0.f;
	bool bIsPulsing = false;
};
