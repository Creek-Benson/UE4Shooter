// Fill out your copyright notice in the Description page of Project Settings.


#include "ShooterAnimInstance.h"
#include "ShooterCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"

UShooterAnimInstance::UShooterAnimInstance() :
	Speed(0.f),
	bIsInAir(false),
	bIsAccelerating(false),
	MovementOffsetYaw(0.f),
	LastMovementOffsetYaw(0.f),
	bAiming(false),
	TurnInPlaceCharacterYaw(0.f),
	TurnInPlaceCharacterYawLastFrame(0.f),
	RootYawOffset(0.f),
	Pitch(0.f),
	bReloading(false),
	OffsetState(EOffsetState::EOS_Hip),
	LeaningCharacterRotation(FRotator(0.f)),
	LeaningCharacterRotationLastFrame(FRotator(0.f)),
	LeaningYawDelta(0.f),
	RecoilWeight(1.f),
	bTurningInPlace(false)
{

}

void UShooterAnimInstance::UpdateAnimationProperties(float DeltaTime)
{
	if (ShooterCharacter == nullptr)
	{
		ShooterCharacter = Cast<AShooterCharacter>(TryGetPawnOwner());
	}

	if (ShooterCharacter)
	{
		// Get the lateral speed of the character from velocity
		FVector Velocity{ ShooterCharacter->GetVelocity() };
		Velocity.Z = 0;
		Speed = Velocity.Size();

		// Is the character in the air?
		bIsInAir = ShooterCharacter->GetCharacterMovement()->IsFalling();

		// Is the character accelerating?
		bIsAccelerating = ShooterCharacter->GetCharacterMovement()->GetCurrentAcceleration().Size() > 0.f;

		FRotator AimRotation = ShooterCharacter->GetBaseAimRotation();
		FRotator MovementRotation = UKismetMathLibrary::MakeRotFromX(ShooterCharacter->GetVelocity());
		MovementOffsetYaw = UKismetMathLibrary::NormalizedDeltaRotator(MovementRotation, AimRotation).Yaw;

		if (ShooterCharacter->GetVelocity().Size() > 0.f)
		{
			LastMovementOffsetYaw = MovementOffsetYaw;
		}

		bAiming = ShooterCharacter->GetAiming();
		bReloading = ShooterCharacter->GetCombatState() == ECombatState::ECS_Reloading;
		bCrouching = ShooterCharacter->GetCrouching();

		if (bReloading)
		{
			OffsetState = EOffsetState::EOS_Reloading;
		}
		else if (bIsInAir)
		{
			OffsetState = EOffsetState::EOS_InAir;
		}
		else if (bAiming)
		{
			OffsetState = EOffsetState::EOS_Aiming;
		}
		else
		{
			OffsetState = EOffsetState::EOS_Hip;
		}

		TurnInPlace();
		Lean(DeltaTime);
	}
}

void UShooterAnimInstance::NativeInitializeAnimation()
{
	ShooterCharacter = Cast<AShooterCharacter>(TryGetPawnOwner());
}

void UShooterAnimInstance::TurnInPlace()
{
	Pitch = ShooterCharacter->GetBaseAimRotation().Pitch;

	// Don't want to turn in place while character is moving
	if (Speed > 0 || bIsInAir)
	{
		RootYawOffset = 0.f;

		TurnInPlaceCharacterYaw = ShooterCharacter->GetActorRotation().Yaw;
		TurnInPlaceCharacterYawLastFrame = TurnInPlaceCharacterYaw;

		RotationCurveLastFrame = 0.f;
		RotationCurve = 0.f;
	}
	else
	{
		TurnInPlaceCharacterYawLastFrame = TurnInPlaceCharacterYaw;
		TurnInPlaceCharacterYaw = ShooterCharacter->GetActorRotation().Yaw;
		const float YawDelta{ TurnInPlaceCharacterYaw - TurnInPlaceCharacterYawLastFrame };

		// Root Yaw Offset, updated and clamped to [-180, 180]
		RootYawOffset = UKismetMathLibrary::NormalizeAxis(RootYawOffset - YawDelta);

		// 1.0 if turning, 0.0 if not
		const float Turning{ GetCurveValue(TEXT("Turning")) };
		if (Turning > 0)
		{
			bTurningInPlace = true;
			RotationCurveLastFrame = RotationCurve;
			RotationCurve = GetCurveValue(TEXT("Rotation"));

			const float DeltaRotation{ RotationCurve - RotationCurveLastFrame };

			RootYawOffset > 0
				? RootYawOffset -= DeltaRotation	// Turning Left
				: RootYawOffset += DeltaRotation;	// Turning Right

			const float AbsRootYawOffset{ FMath::Abs(RootYawOffset) };

			if (AbsRootYawOffset > 90.f)
			{
				const float YawExcess{ AbsRootYawOffset - 90.f };

				RootYawOffset > 0
					? RootYawOffset -= YawExcess
					: RootYawOffset += YawExcess;
			}
		}
		else
		{
			bTurningInPlace = false;
		}
	}

	if (bTurningInPlace)
	{
		if (bReloading)
		{
			RecoilWeight = 1.f;
		}
		else
		{
			RecoilWeight = 0.f;
		}
	}
	else
	{
		if (bCrouching)
		{
			if (bReloading)
			{
				RecoilWeight = 1.f;
			}
			else
			{
				RecoilWeight = 0.1f;
			}
		}
		else
		{
			if (bAiming || bReloading)
			{
				RecoilWeight = 1.f;
			}
			else
			{
				RecoilWeight = 0.5f;
			}
		}
	}
}

void UShooterAnimInstance::Lean(float DeltaTime)
{
	LeaningCharacterRotationLastFrame = LeaningCharacterRotation;
	LeaningCharacterRotation = ShooterCharacter->GetActorRotation();

	const FRotator Delta{ UKismetMathLibrary::NormalizedDeltaRotator(LeaningCharacterRotation, LeaningCharacterRotationLastFrame) };
	const float Target{ Delta.Yaw / DeltaTime };
	const float Interp{ FMath::FInterpTo(LeaningYawDelta, Target, DeltaTime, 6.f) };

	LeaningYawDelta = FMath::Clamp(Interp, -90.f, 90.f);
}


