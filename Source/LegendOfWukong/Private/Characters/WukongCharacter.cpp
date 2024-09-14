// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/WukongCharacter.h"
#include "Components/StatsComponent.h"
#include "Components/Combat/BlockComponent.h"
#include "Components/Combat/CombatComponent.h"
#include "Components/Combat/LockOnComponent.h"
#include "Components/Combat/TraceComponent.h"
#include "EAttribute.h"


#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"

AWukongCharacter::AWukongCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	LockonComp = CreateDefaultSubobject<ULockOnComponent>(TEXT("LockonComp"));
	CombatComp = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComp"));
	TraceComp = CreateDefaultSubobject<UTraceComponent>(TEXT("TraceComp"));
	BlockComp = CreateDefaultSubobject<UBlockComponent>(TEXT("BlockComp"));
	StatsComp = CreateDefaultSubobject<UStatsComponent>(TEXT("StatsComp"));
}

void AWukongCharacter::BeginPlay()
{
	Super::BeginPlay();
	WukongAnim = Cast<UWukongAnimInstance>(GetMesh()->GetAnimInstance());
}

float AWukongCharacter::ApplyDamage()
{
	return StatsComp->Attributes[EAttribute::Strength];
}

float AWukongCharacter::GetPercentage(EAttribute Current, EAttribute Max)
{
	return StatsComp->Attributes[Current] / StatsComp->Attributes[Max];
}

void AWukongCharacter::HandleDeath()
{
	PlayAnimMontage(DeadthAnimMontage);
	DisableInput(GetController<APlayerController>());
}

void AWukongCharacter::FinishPadAnim()
{
	bIsPad = false;
}

bool AWukongCharacter::CanTakeDamage()
{
	if(bIsPad)
	{
		return false;
	}

	return true;
}

void AWukongCharacter::ReduceStamina(float Amount)
{
	StatsComp->Attributes[EAttribute::Stamina] -= Amount;
	StatsComp->Attributes[EAttribute::Stamina] = UKismetMathLibrary::FClamp(StatsComp->Attributes[EAttribute::Stamina], 0,StatsComp->Attributes[EAttribute::MaxStamina] );
	bCanRestore = false;

	FLatentActionInfo FunctionInfo(0, 100, TEXT("EnableStore"), this);
	
	UKismetSystemLibrary::RetriggerableDelay(
		GetWorld(),
		StaminaDelayDuration,
		FunctionInfo
	);
	
	StatsComp->OnUpdateStaminaDelegate.Broadcast(
		GetPercentage(EAttribute::Stamina, EAttribute::MaxStamina));
}

bool AWukongCharacter::HasEnoughStamina(float Cost)
{
	return StatsComp->Attributes[EAttribute::Stamina] >= Cost;
}

void AWukongCharacter::AutoEndLock(AActor* Actor)
{
	if(LockonComp->TargetActor != Actor)
	{
		return;
	}

	LockonComp->EndLockOn();
}

void AWukongCharacter::EnableStore()
{
	bCanRestore = true;
}

void AWukongCharacter::RestoreStamina()
{
	if(bCanRestore)
	{
		return;
	}
	
	StatsComp->Attributes[EAttribute::Stamina] = UKismetMathLibrary::FInterpTo_Constant(
		StatsComp->Attributes[EAttribute::Stamina],
		StatsComp->Attributes[EAttribute::MaxStamina],
		GetWorld()->DeltaTimeSeconds,
		StaminaRestoreRate
		);
	StatsComp->OnUpdateStaminaDelegate.Broadcast(
		GetPercentage(EAttribute::Stamina, EAttribute::MaxStamina));
}

void AWukongCharacter::Sprint()
{
	if(!HasEnoughStamina(SprintCost))
	{
		Walk();
		return;
	}

	if(GetCharacterMovement()->Velocity.Equals(FVector::ZeroVector, 1))
	{
		return;
	}

	GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
	ReduceStamina(SprintCost);
}

void AWukongCharacter::Walk()
{
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
}

void AWukongCharacter::Pad()
{
	if (bIsPad || !HasEnoughStamina(PadCost))
	{
		return;
	}

	bIsPad = true;
	ReduceStamina(PadCost);

	UE_LOG(LogTemp, Warning, TEXT("Started Pad Animation."));

	FVector Direction = (GetCharacterMovement()->Velocity.Length() < 1) ? GetActorForwardVector() : GetLastMovementInputVector();
	FRotator Rotation = UKismetMathLibrary::MakeRotFromX(Direction);
	SetActorRotation(Rotation);

	UE_LOG(LogTemp, Warning, TEXT("Character rotation set."));

	float Duration = PlayAnimMontage(PadAnimMontage);
	FTimerHandle PadTimerHandle;

	GetWorldTimerManager().SetTimer(PadTimerHandle, this, &AWukongCharacter::FinishPadAnim, Duration, false);
}

void AWukongCharacter::ReduceHealth(float Amount)
{
	if(StatsComp->Attributes[EAttribute::Health] <= 0 )
	{
		return;
	}

	if(!CanTakeDamage())
	{
		return;
	}
	
	StatsComp->Attributes[EAttribute::Health] -= Amount;
	StatsComp->Attributes[EAttribute::Health] = UKismetMathLibrary::FClamp(StatsComp->Attributes[EAttribute::Health], 0,StatsComp->Attributes[EAttribute::MaxHealth]);

	StatsComp->OnUpdateHealthUIDelegate.Broadcast(
		GetPercentage(EAttribute::Health, EAttribute::MaxHealth));

	if(StatsComp->Attributes[EAttribute::Health] == 0 )
	{
		
		StatsComp->OnZeroHealthDelegate.Broadcast(true);
	}
}


