// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StormActor.generated.h"

USTRUCT()
struct FStormScaleProgression
{
	GENERATED_BODY()

public:
	float ScaleThreshold;
	float ScaleModifier;

	FStormScaleProgression() { ScaleThreshold=ScaleModifier=0.f; }
	FStormScaleProgression(const float threshold,const float modifier) : ScaleThreshold(threshold),ScaleModifier(modifier) { }
};

USTRUCT()
struct FStormAdvancement
{
	GENERATED_BODY()

private:
	UPROPERTY()
	TArray<FStormScaleProgression> Progressions;

public:
	UPROPERTY()
	float ScaleDownRate;

	UPROPERTY()
	float ExtremelyLowScale;

	FStormAdvancement() { ScaleDownRate=0.999485f; ExtremelyLowScale=0.00000009f; }
	void AutoGenerateThresholds(const float Resolution)
	{
		Empty();
		float ScaleResolution=1.f / Resolution;
		int32 ResolutionI=Resolution;
		for(int32 i=0; i < ResolutionI; i++)
		{
			float NextThreshold=ScaleResolution * (i+1);
			float NextModifier=NextThreshold / 100.f;
			Add(NextThreshold,NextModifier);
		}
	}
	FVector AdvanceStorm(float ScaleX,const float ScaleZ,const float DeltaTime)
	{
		for(auto& progression : Progressions)
		{
			if(ScaleX <= progression.ScaleThreshold)
			{
				ScaleX=FMath::Clamp(ScaleX-(ScaleDownRate * DeltaTime * progression.ScaleModifier),0.f,ScaleX);
				break;
			}
		}
		return FVector(ScaleX,ScaleX,ScaleZ);
	}
	void OutputStormProgression()
	{
		FString LogMsg;
		for(auto& progression : Progressions)
			LogMsg+=FString::Printf(_T("{%.04f, %.04f}, "),progression.ScaleThreshold,progression.ScaleModifier);
		GEngine->AddOnScreenDebugMessage(1,10.f,FColor::Yellow,"StormProgression="+LogMsg);
	}
	void Add(const float threshold,const float modifier)
	{
		for(auto& progression : Progressions)
			if(FMath::IsNearlyEqual(threshold,progression.ScaleThreshold))
				return; //Threshold already exists, don't add it again...
		Progressions.Emplace(FStormScaleProgression(threshold,modifier));
		Sort();
	}
	void Sort()
	{
		Progressions.Sort([](const FStormScaleProgression& L,const FStormScaleProgression& R) -> bool {
			return L.ScaleThreshold < R.ScaleThreshold;
		});
	}
	void Empty()
	{
		Progressions.Empty();
	}
};

UCLASS()
class FORTNITECLONE_API AStormActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AStormActor();
	void InitializeStorm();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(Replicated)
	float Damage;

	UPROPERTY(Replicated)
	bool IsShrinking;

	UPROPERTY(Replicated)
	FVector SizeScale;

	UPROPERTY()
	FVector InitialSizeScale;

	UPROPERTY()
	FVector InitialActorLocation;

	UPROPERTY()
	FTimerHandle StormStageTimerHandle;

	UPROPERTY()
	FTimerHandle StormDamageTimerHandle;

	UPROPERTY()
	float StormAdvanceStageRate;

	UPROPERTY()
	float StormIncreaseDamageRate;

	UPROPERTY()
	FStormAdvancement StormAdvancement;

	UPROPERTY()
	int Stage;

	virtual bool IsSupportedForNetworking() const override
	{
		return true;
	}

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerAdvanceStage();

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerSetNewDamage();

	UFUNCTION(Server,Reliable,WithValidation)
	void ServerStartStorm();
};