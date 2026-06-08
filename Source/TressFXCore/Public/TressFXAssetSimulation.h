//
// Copyright (c) 2016-2025 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#pragma once


#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "TressFXAssetSimulation.generated.h"


USTRUCT(BlueprintType)
struct TRESSFXCORE_API FTressFXGroupsSimulation
{
	GENERATED_BODY()

	FTressFXGroupsSimulation();

	UPROPERTY(EditAnywhere, Category = "Animation")
		FString Name;

	UPROPERTY(EditAnywhere, Category = "Simulation", meta = (ToolTip = "Reset the position when animation starts"))
		bool ResetPositionAtStart = true;

	//UPROPERTY(EditAnywhere, Category = "Simulation", meta = (ToolTip = "Enable the simulation on that group"))
	//bool EnableSimulation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation", meta = (ClampMin = "2", UIMin = "2", UIMax = "8", ToolTip = "Number of vertices from root turn off simulation"))
	int32 NumVerticesFromRootNoSimulation = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gravity", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "0.07", ToolTip = "Gravity Damping Coefficient"))
	float Damping = 0.035f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gravity", meta = (ClampMin = "0.5", UIMin = "0.5", UIMax = "100", ToolTip = "Gravity Acceleration"))
	float GravityMagnitude = 10.0f;

	UPROPERTY(EditAnywhere, Category = "Simulation", meta = (ToolTip = "Enable the global shape constraint on that group"))
	bool EnableGlobalShapeConstraint = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GlobalShapeConstraint", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "1.0", ToolTip = "Global Shape Constraints Stiffness"))
	float GlobalShapeStiffness = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GlobalShapeConstraint", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "1.0", ToolTip = "Global Shape Constraints Range"))
	float GlobalShapeEffectiveRange = 0.5f;

	UPROPERTY(EditAnywhere, Category = "VelocityShockPropagation", meta = (ToolTip = "Enable the velocity shock propagation on that group"))
	bool EnableVelocityShockPropagation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VelocityShockPropagation", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "1.0", ToolTip = "Velocity Shock Propagation Coefficient"))
	float VSPCoefficient = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VelocityShockPropagation", meta = (DisplayName = "VSP Stiffness", ViewMinInput = "0.0", ViewMaxInput = "1.0", ViewMinOutput = "0.0", ViewMaxOutput = "1.0", TimeLineLength = "1.0", XAxisName = "Strand Coordinate (0,1)", YAxisName = "Stiffness", ToolTip = "This curve determines how much the vsp stiffness will be along each strand. \n The X axis range is [0,1], 0 mapping the root and 1 the tip"))
		FRuntimeFloatCurve VSPStiffness;

	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VelocityShockPropagation", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "0.2", ToolTip = "Velocity Shock Propagation Accel Threshold Min"))
	float VSPAccelThresholdMin = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VelocityShockPropagation", meta = (ClampMin = "1.0", UIMin = "1.0", UIMax = "2000.0", ToolTip = "Velocity Shock Propagation Accel Threshold Max"))
	float VSPAccelThresholdMax = 1.4f;

	UPROPERTY(EditAnywhere, Category = "LengthConstraint", meta = (ToolTip = "Enable the length constraint on that group"))
	bool EnableLengthConstraint = true;

	UPROPERTY(EditAnywhere, Category = "LengthConstraint", meta = (ClampMin = "1", UIMin = "1", UIMax = "4", ToolTip = "Number of iterations to solve the length constraints"))
	uint32 LengthConstraintsIterations = 1;

	UPROPERTY(EditAnywhere, Category = "Wind", meta = (ToolTip = "Wind Direction"))
	FVector3f WindDirection = FVector3f(0,1.f,0);

	UPROPERTY(EditAnywhere, Category = "Wind", meta = (ClampMin = "0.0", UIMin = "0.0", ToolTip = "Wind Magnitude"))
	float WindMagnitude = 0;

	UPROPERTY(EditAnywhere, Category = "LocalShapeConstraint", meta = (ToolTip = "Enable the local shape constraint on that group"))
	bool EnableLocalShapeConstraint = true;

	UPROPERTY(EditAnywhere, Category = "LocalShapeConstraint", meta = (DisplayName = "Local Shape Stiffness", ViewMinInput = "0.0", ViewMaxInput = "1.0", ViewMinOutput = "0.0", ViewMaxOutput = "1.0", TimeLineLength = "1.0", XAxisName = "Strand Coordinate (0,1)", YAxisName = "Stiffness", ToolTip = "This curve determines how much the local shape stiffness will be along each strand. \n The X axis range is [0,1], 0 mapping the root and 1 the tip"))
	FRuntimeFloatCurve LocalShapeStiffness;

};

USTRUCT(BlueprintType)
struct FAnimationResetPositionPeriod
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "AnimResetPositionPeriod", BlueprintReadWrite)
		float Start = 0.f;

	UPROPERTY(EditAnywhere, Category = "AnimResetPositionPeriod", BlueprintReadWrite)
		float End = 0.f;
};

USTRUCT(BlueprintType)
struct FAnimationTressFXSimulationSettings
{
	GENERATED_USTRUCT_BODY()

	FAnimationTressFXSimulationSettings()
	{
		TressFXSimulationSettingsName = FString("Default");
	}

	UPROPERTY(EditAnywhere, Category = "AnimTressFXSimulationSettings", BlueprintReadWrite)
		TObjectPtr<class UAnimationAsset> Animation;

	UPROPERTY(EditAnywhere, Category = "AnimTressFXSimulationSettings", BlueprintReadWrite)
		FString TressFXSimulationSettingsName;


	UPROPERTY(EditAnywhere, Category = "AnimTressFXSimulationSettings", BlueprintReadWrite)
		TArray< FAnimationResetPositionPeriod> AnimResetPositionPeriods;
};
