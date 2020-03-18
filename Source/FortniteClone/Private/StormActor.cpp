// Fill out your copyright notice in the Description page of Project Settings.

#include "StormActor.h"
#include "FortniteCloneCharacter.h"
#include "UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"

// Sets default values
AStormActor::AStormActor()
{
	bReplicates = true;
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	Damage=1;
	StormAdvanceStageRate=30.f; //Default every 30 seconds
	StormIncreaseDamageRate=120.f; //Default every 120 seconds
	IsShrinking=false;
	InitialSizeScale=GetActorScale3D(); 
	InitialActorLocation=GetActorLocation();
	//Setting z scale and location of storm seems to improve performance much more noticably than precalculating shrink vectors
	InitialSizeScale.Z=-0.03f; //Scale.Z=0.04f Location.Z=33100.f good values or (okay values -0.02f and 45990.f: with inverted cylinder for bright bottom and dim top instead of vice versa)
	InitialActorLocation.Z=54000.f; //Best so far: -0.03f z scale, 50000.f z location (an okay hotfix for now until the real issue is figured out and solved)
	ScaleDownRate=0.999485f;
	InverseScaleDownRate=0.000515f;
	ScaleHighThreshold=1.f;
	ScaleMidThreshold=0.7f;
	ScaleLowThreshold=0.4f;
	ScaleHighModifier=0.000110f;
	ScaleMidModifier=0.000320f;
	ScaleLowModifier=0.000100f;
	Stage = 0; //Every odd stage it shrinks, even stages player's get a break from it...
	FMath::SRandInit(FPlatformTime::Cycles());

	IConsoleVariable *StormRestartCmd=IConsoleManager::Get().RegisterConsoleVariable(TEXT("StormRestart"),
		2,
		TEXT("AdminCommand: Run 'StormRestart 1' to restart the storm on demand.\n"),
		ECVF_Scalability | ECVF_RenderThreadSafe);

	StormRestartCmd->AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateLambda([&] (IConsoleVariable* Var){ InitializeStorm(); }));

	IConsoleVariable* zStormScale=IConsoleManager::Get().RegisterConsoleVariable(TEXT("z.StormScale"),
		2,
		TEXT("AdminCommand: 'z.StormScale (float value)' to live edit storm z scale\n"),
		ECVF_Scalability | ECVF_RenderThreadSafe);

	zStormScale->AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateLambda([&](IConsoleVariable* Var){ 
		InitialSizeScale.Z=SizeScale.Z=FCString::Atof(*Var->GetString());
		FString LogMsg=FString::Printf(_T("SizeScale.Z=%.08f"),SizeScale.Z);
		GEngine->AddOnScreenDebugMessage(4,5.0f,FColor::Yellow,LogMsg);
	}));

	IConsoleVariable* zStormScale2=IConsoleManager::Get().RegisterConsoleVariable(TEXT("z.StormScaleAfterDecimal"),
		2,
		TEXT("AdminCommand: 'z.StormScaleAfterDecimal (float value)' to live edit storm z scale\n"),
		ECVF_Scalability | ECVF_RenderThreadSafe);

	zStormScale2->AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateLambda([&](IConsoleVariable* Var){
		SizeScale.Z=InitialSizeScale.Z+(FCString::Atof(*Var->GetString()) / 100.f);
		FString LogMsg=FString::Printf(_T("SizeScale.Z after decimal=%.08f"),SizeScale.Z);
		GEngine->AddOnScreenDebugMessage(4,5.0f,FColor::Yellow,LogMsg);
	}));

	IConsoleVariable* zStormLocation=IConsoleManager::Get().RegisterConsoleVariable(TEXT("z.StormLocation"),
		2,
		TEXT("AdminCommand: 'z.StormLocation (float value)' to live edit storm z location\n"),
		ECVF_Scalability | ECVF_RenderThreadSafe);
	
	zStormLocation->AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateLambda([&](IConsoleVariable* Var){ 
		InitialActorLocation.Z=Var->GetFloat();
		SetActorLocation(InitialActorLocation);
	}));
}

void AStormActor::InitializeStorm()
{
	if(HasAuthority())
	{
		Damage=1;
		IsShrinking=false;
		Stage=0;
		//move the storm to a random location on the map
		int32 X=FMath::RandRange(-7000 + 10000,60000 - 10000);
		int32 Y=FMath::RandRange(-60000 + 10000,17000 - 10000);
		SetActorLocation(FVector(X,Y,InitialActorLocation.Z));

		SizeScale=InitialSizeScale;
		SetActorScale3D(InitialSizeScale);

		/*
		ScaleTotalCount=131072; //(131072 * 12) == (1024 * 1024 * 1.5) We'll go up to 1.5MB worth of scale downs but it's more than enough and we'll stop early anyway...
		ScaleIndex=0;
		SizeScales.Reset();
		for(int32 i=0; i < ScaleTotalCount; i++)
		{
			//More like the original / uniform scale down
			//ScaleDown.X=(ScaleDown.Y*=ScaleDownRate);

			//Experimental variable shrink down rate (to try and get past frame dip points faster)
			if(ScaleDown.X <= ScaleHighThreshold)
				ScaleDown.X=(ScaleDown.Y*=(ScaleDownRate - ScaleHighModifier));
			else if(ScaleDown.X <= ScaleMidThreshold)
				ScaleDown.X=(ScaleDown.Y*=(ScaleDownRate - ScaleMidModifier));
			else if(ScaleDown.X <= ScaleLowThreshold)
				ScaleDown.X=(ScaleDown.Y*=(ScaleDownRate + ScaleLowModifier));
			
			SizeScales.Emplace(ScaleDown);
			FString LogMsg=FString::Printf(_T("Pre-calculating: ScaleDown.X=%.08f ScaleDown.Y=%.08f ScaleTotalCount=%i"),ScaleDown.X,ScaleDown.Y,i);
			GEngine->AddOnScreenDebugMessage(0,5.0f,FColor::Yellow,LogMsg);
			if(ScaleDown.X <= 0.0000009f && ScaleDown.Y <= 0.0000009f)
			{
				ScaleTotalCount=i+1; //When we have enough to reach basically zero then we're done pre-calculating
				break;
			}
		}
		*/

		GetWorldTimerManager().SetTimer(StormSetupTimerHandle,this,&AStormActor::AdvanceStage,StormAdvanceStageRate,true);
		GetWorldTimerManager().SetTimer(StormDamageTimerHandle,this,&AStormActor::ServerSetNewDamage,StormIncreaseDamageRate,true);
	}
}

// Called when the game starts or when spawned
void AStormActor::BeginPlay()
{
	Super::BeginPlay();
	InitializeStorm();

	/*FString LogMsg = FString("storm actor constructor ") + FString::FromInt(X) + FString(" ") + FString::FromInt(Y);
	UE_LOG(LogMyGame, Warning, TEXT("%s"), *LogMsg);*/
	//after 30 seconds, start shrinking the circle at the last 30 seconds of every 2 and a half minute interval
	//FTimerHandle StormSetupTimerHandle;
	//GetWorldTimerManager().SetTimer(StormSetupTimerHandle, this, &AStormActor::ServerStartStorm, 30.0f, false);
	/*LogMsg = FString("begin play circle ") + FString::FromInt(GetNetMode());
	UE_LOG(LogMyGame, Warning, TEXT("%s"), *LogMsg);*/
	
	/*FString LogMsg = FString("begin play circle ") + FString::FromInt(GetNetMode());
	UE_LOG(LogMyGame, Warning, TEXT("%s"), *LogMsg);*/
	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString("Storm Begin Play ") + FString::FromInt(GetNetMode()));
}

// Called every frame
void AStormActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if(HasAuthority() && IsShrinking)
	{
		//Using precalculated vectors doesn't really seem to help much if at all, I believe the problem mostly lies with the storm asset itself and should likely be simplified in the editor...
		/*if(++ScaleIndex < ScaleTotalCount)
		{
			SetActorScale3D(SizeScales[ScaleIndex]);
			FString LogMsg=FString::Printf(_T("Using Pre-calculated: SizeScale.X=%.08f SizeScale.Y=%.08f ScaleIndex=%i ScaleTotalCount=%i Stage=%i"),SizeScales[ScaleIndex].X,SizeScales[ScaleIndex].Y,ScaleIndex,ScaleTotalCount,Stage);
			GEngine->AddOnScreenDebugMessage(1,5.f,FColor::Green,LogMsg);
		}*/

		//So it doesn't seem like there's much we can do here since it seems like calculating here really isn't that expensive not enough to be the actual issue anyway, so
		//how about incorporating the delta into here so that at least it shrinks in a controlled and predictible way regardless of framerate...
		if(SizeScale.X > 0.0000009f)
		{
			float newScaleXY=(SizeScale.X - (ScaleDownRate * DeltaTime * 0.009f));
			SizeScale=FVector(newScaleXY,newScaleXY,SizeScale.Z);
			SetActorScale3D(SizeScale);

			FString LogMsg=FString::Printf(_T("Using DeltaTime incorporated scaling: Stage=%i SizeScale.XY=%0.8f"),Stage,SizeScale.X);
			GEngine->AddOnScreenDebugMessage(1,5.f,FColor::Yellow,LogMsg);
		}

		float timeElapsed1=GetWorldTimerManager().GetTimerElapsed(StormSetupTimerHandle);
		float timeRemaining1=GetWorldTimerManager().GetTimerRemaining(StormSetupTimerHandle);
		FString timeMsg1=FString::Printf(_T("Storm Stage Advance, timer elapsed (seconds): %.02f, time remaining until next stage (seconds): %.02f"),timeElapsed1,timeRemaining1);
		GEngine->AddOnScreenDebugMessage(2,5.f,FColor::Green,timeMsg1);

		if(StormDamageTimerHandle.IsValid())
		{
			float timeElapsed2=GetWorldTimerManager().GetTimerElapsed(StormDamageTimerHandle);
			float timeRemaining2=GetWorldTimerManager().GetTimerRemaining(StormDamageTimerHandle);
			FString timeMsg2=FString::Printf(_T("Storm Damage Increase, timer elapsed (seconds): %.02f, time remaining until damage increased (seconds): %.02f"),timeElapsed2,timeRemaining2);
			GEngine->AddOnScreenDebugMessage(3,5.f,FColor::Green,timeMsg2);
		}
	}
}

void AStormActor::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AStormActor, Damage);
	DOREPLIFETIME(AStormActor, IsShrinking);
	DOREPLIFETIME(AStormActor, InitialSizeScale);
	DOREPLIFETIME(AStormActor, InitialActorLocation);
	DOREPLIFETIME(AStormActor, SizeScales);
	DOREPLIFETIME(AStormActor, ScaleIndex);
	DOREPLIFETIME(AStormActor, ScaleTotalCount);
}

void AStormActor::AdvanceStage_Implementation()
{		
	//Advance stages until storm is down to almost nothing (no longer visible) or ScaleIndex is outside of precalculated shrinkdowns,
	//Usually by this point either the game has probably ended or we reinitalize the storm and start again...
	if(GetActorScale3D().X <= 0.0000009f && GetActorScale3D().Y <= 0.0000009f) //|| ScaleIndex >= ScaleTotalCount)
		return InitializeStorm();
	//If 1/10th the scale make this the last stage and keep shrinking until there's a winner / end of game, or the storm re-initializes due to being extremely tiny as above
	if(GetActorScale3D().X <= 0.1f && GetActorScale3D().Y <= 0.1f)
		return;

	IsShrinking=((++Stage % 2) != 0) ?  true : false; //Every odd numbered stage advance the shrinking!
}

bool AStormActor::AdvanceStage_Validate() {
	return true;
}

void AStormActor::ServerSetNewDamage_Implementation() {
	Damage++;
}

bool AStormActor::ServerSetNewDamage_Validate() {
	return true;
}

void AStormActor::ServerStartStorm_Implementation()
{
	InitializeStorm();
}

bool AStormActor::ServerStartStorm_Validate()
{
	return true;
}