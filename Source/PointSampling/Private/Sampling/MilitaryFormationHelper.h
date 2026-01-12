/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "Math/UnrealMathUtility.h"

/**
 * 军事阵型采样辅助类
 *
 * 基于军事战术实践，实现各种经典的部队阵型：
 * - 楔形阵：适用于突破战术
 * - 纵队阵：适用于通过狭窄地形
 * - 横队阵：适用于火力覆盖
 * - V形阵：适用于防御战术
 * - 梯形阵：适用于侧翼攻击
 */
class FMilitaryFormationHelper
{
public:
	/**
	 * 生成楔形阵型 (V形突破阵型)
	 * 特点：尖端向前，部队呈V形展开，便于集中火力突破防线
	 */
	static TArray<FVector> GenerateWedgeFormation(
		int32 PointCount,
		float Spacing,
		float WedgeAngle,
		FRandomStream& RandomStream
	);

	/**
	 * 生成纵队阵型 (单列纵队)
	 * 特点：最小横向宽度，适用于通过桥梁、走廊等狭窄区域
	 */
	static TArray<FVector> GenerateColumnFormation(
		int32 PointCount,
		float Spacing,
		FRandomStream& RandomStream
	);

	/**
	 * 生成横队阵型 (单排横队)
	 * 特点：最大横向火力覆盖，适用于阵地防御或火力压制
	 */
	static TArray<FVector> GenerateLineFormation(
		int32 PointCount,
		float Spacing,
		FRandomStream& RandomStream
	);

	/**
	 * 生成V形阵型 (倒V形防御阵型)
	 * 特点：尖端向后，形成倒V形，便于两翼包抄和后方防御
	 */
	static TArray<FVector> GenerateVeeFormation(
		int32 PointCount,
		float Spacing,
		float VeeAngle,
		FRandomStream& RandomStream
	);

	/**
	 * 生成梯形阵型
	 * @param Direction -1=左梯形, 1=右梯形
	 */
	static TArray<FVector> GenerateEchelonFormation(
		int32 PointCount,
		float Spacing,
		int32 Direction,
		float EchelonAngle,
		FRandomStream& RandomStream
	);
};