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
	FStormScaleProgression(float threshold,float modifier) : ScaleThreshold(threshold),ScaleModifier(modifier) { }
};

USTRUCT()
struct FStormAdvancement
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<FStormScaleProgression> Progressions;

	UPROPERTY()
	float ScaleDownRate;

	UPROPERTY()
	float ExtremelyLowScale;

	FStormAdvancement() { ScaleDownRate=0.999485f; ExtremelyLowScale=0.00000009f; }
	void AutoGenerateThresholds(float Resolution)
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
	FVector AdvanceStorm(float CurrentScaleX,float CurrentScaleZ,float DeltaTime)
	{
		float NewScaleX=1.f;
		for(auto& progression : Progressions)
		{
			if(CurrentScaleX <= progression.ScaleThreshold)
			{
				NewScaleX=CurrentScaleX;
				if(CurrentScaleX > ExtremelyLowScale)
					NewScaleX-=(ScaleDownRate * DeltaTime * progression.ScaleModifier);
					
				FString LogMsg=FString::Printf(_T("CurrentScaleX <= %.08f ScaleModifier=%.08f NewScaleX=%.08f"),progression.ScaleThreshold,progression.ScaleModifier,NewScaleX);
				GEngine->AddOnScreenDebugMessage(0,5.f,FColor::Green,LogMsg);
				break;
			}
		}
		return FVector(NewScaleX,NewScaleX,CurrentScaleZ);
	}
	void Add(float threshold,float modifier)
	{
		auto p=FStormScaleProgression(threshold,modifier);
		for(auto& progression : Progressions)
		{
			if(p.ScaleThreshold==progression.ScaleThreshold)
				return; //Threshold already exists, don't add it again...
		}
		Progressions.Emplace(p);
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

	UPROPERTY()
	FTimerHandle StormStageTimerHandle;
	
	UPROPERTY()
	FTimerHandle StormDamageTimerHandle;

	UPROPERTY(Replicated)
	float Damage;

	UPROPERTY(Replicated)
	float StormAdvanceStageRate;

	UPROPERTY(Replicated)
	float StormIncreaseDamageRate;

	UPROPERTY(Replicated)
	bool IsShrinking;

	UPROPERTY(Replicated)
	FVector InitialSizeScale;
	
	UPROPERTY(Replicated)
	FVector InitialActorLocation;

	UPROPERTY(Replicated)
	FVector SizeScale;

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
