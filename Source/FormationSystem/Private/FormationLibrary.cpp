#include "FormationLibrary.h"
#include "FormationSystemCore.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/Engine.h"

FFormationData UFormationLibrary::CreateSquareFormation(
    FVector CenterLocation,
    FRotator Rotation,
    int32 UnitCount,
    float Spacing,
    int32 RowCount)
{
    FFormationData Formation;
    Formation.FormationType = EFormationType::Square;
    Formation.CenterLocation = CenterLocation;
    Formation.Rotation = Rotation;
    Formation.Spacing = Spacing;

    if (UnitCount <= 0)
    {
        return Formation;
    }

    int32 Rows, Cols;
    if (RowCount > 0)
    {
        Rows = RowCount;
        Cols = FMath::CeilToInt(static_cast<float>(UnitCount) / Rows);
    }
    else
    {
        CalculateOptimalRowsCols(UnitCount, Rows, Cols);
    }

    Formation.Positions.Reserve(UnitCount);

    // 计算起始偏移，使阵型居中
    float StartX = -(Cols - 1) * Spacing * 0.5f;
    float StartY = -(Rows - 1) * Spacing * 0.5f;

    int32 CurrentUnit = 0;
    for (int32 Row = 0; Row < Rows && CurrentUnit < UnitCount; Row++)
    {
        for (int32 Col = 0; Col < Cols && CurrentUnit < UnitCount; Col++)
        {
            FVector Position(
                StartX + Col * Spacing,
                StartY + Row * Spacing,
                0.0f
            );
            Formation.Positions.Add(Position);
            CurrentUnit++;
        }
    }

    Formation.Size = FVector2D(Cols * Spacing, Rows * Spacing);
    return Formation;
}

FFormationData UFormationLibrary::CreateCircleFormation(
    FVector CenterLocation,
    FRotator Rotation,
    int32 UnitCount,
    float Radius,
    float StartAngle,
    bool bClockwise)
{
    FFormationData Formation;
    Formation.FormationType = EFormationType::Circle;
    Formation.CenterLocation = CenterLocation;
    Formation.Rotation = Rotation;
    Formation.Size = FVector2D(Radius * 2.0f, Radius * 2.0f);

    if (UnitCount <= 0)
    {
        return Formation;
    }

    Formation.Positions.Reserve(UnitCount);

    if (UnitCount == 1)
    {
        // 单个单位放在中心
        Formation.Positions.Add(FVector::ZeroVector);
    }
    else
    {
        float AngleStep = 360.0f / UnitCount;
        if (!bClockwise)
        {
            AngleStep = -AngleStep;
        }

        for (int32 i = 0; i < UnitCount; i++)
        {
            float Angle = StartAngle + i * AngleStep;
            float RadianAngle = FMath::DegreesToRadians(Angle);
            
            FVector Position(
                Radius * FMath::Cos(RadianAngle),
                Radius * FMath::Sin(RadianAngle),
                0.0f
            );
            Formation.Positions.Add(Position);
        }
    }

    return Formation;
}

FFormationData UFormationLibrary::CreateLineFormation(
    FVector CenterLocation,
    FRotator Rotation,
    int32 UnitCount,
    float Spacing,
    bool bVertical)
{
    FFormationData Formation;
    Formation.FormationType = EFormationType::Line;
    Formation.CenterLocation = CenterLocation;
    Formation.Rotation = Rotation;
    Formation.Spacing = Spacing;

    if (UnitCount <= 0)
    {
        return Formation;
    }

    Formation.Positions.Reserve(UnitCount);

    // 计算起始偏移，使线形阵型居中
    float StartOffset = -(UnitCount - 1) * Spacing * 0.5f;

    for (int32 i = 0; i < UnitCount; i++)
    {
        FVector Position;
        if (bVertical)
        {
            Position = FVector(0.0f, StartOffset + i * Spacing, 0.0f);
            Formation.Size = FVector2D(0.0f, UnitCount * Spacing);
        }
        else
        {
            Position = FVector(StartOffset + i * Spacing, 0.0f, 0.0f);
            Formation.Size = FVector2D(UnitCount * Spacing, 0.0f);
        }
        Formation.Positions.Add(Position);
    }

    return Formation;
}

FFormationData UFormationLibrary::CreateTriangleFormation(
    FVector CenterLocation,
    FRotator Rotation,
    int32 UnitCount,
    float Spacing,
    bool bInverted)
{
    FFormationData Formation;
    Formation.FormationType = EFormationType::Triangle;
    Formation.CenterLocation = CenterLocation;
    Formation.Rotation = Rotation;
    Formation.Spacing = Spacing;

    if (UnitCount <= 0)
    {
        return Formation;
    }

    Formation.Positions.Reserve(UnitCount);

    // 生成三角形的行分布
    TArray<int32> RowDistribution = GenerateTriangleRowDistribution(UnitCount, bInverted);
    
    int32 MaxUnitsInRow = 0;
    for (int32 UnitsInRow : RowDistribution)
    {
        MaxUnitsInRow = FMath::Max(MaxUnitsInRow, UnitsInRow);
    }

    // 计算阵型尺寸
    float Width = (MaxUnitsInRow - 1) * Spacing;
    float Height = (RowDistribution.Num() - 1) * Spacing;
    Formation.Size = FVector2D(Width, Height);

    // 计算起始Y偏移
    float StartY = -Height * 0.5f;

    int32 CurrentUnit = 0;
    for (int32 Row = 0; Row < RowDistribution.Num() && CurrentUnit < UnitCount; Row++)
    {
        int32 UnitsInThisRow = RowDistribution[Row];
        float RowStartX = -(UnitsInThisRow - 1) * Spacing * 0.5f;
        
        for (int32 Col = 0; Col < UnitsInThisRow && CurrentUnit < UnitCount; Col++)
        {
            FVector Position(
                RowStartX + Col * Spacing,
                StartY + Row * Spacing,
                0.0f
            );
            Formation.Positions.Add(Position);
            CurrentUnit++;
        }
    }

    return Formation;
}

FFormationData UFormationLibrary::CreateArrowFormation(
    FVector CenterLocation,
    FRotator Rotation,
    int32 UnitCount,
    float Spacing)
{
    FFormationData Formation;
    Formation.FormationType = EFormationType::Arrow;
    Formation.CenterLocation = CenterLocation;
    Formation.Rotation = Rotation;
    Formation.Spacing = Spacing;

    if (UnitCount <= 0)
    {
        return Formation;
    }

    Formation.Positions.Reserve(UnitCount);

    // 箭头形状：前端1个，然后逐渐增加
    TArray<int32> RowDistribution;
    int32 RemainingUnits = UnitCount;
    int32 CurrentRowSize = 1;

    // 分配单位到各行
    while (RemainingUnits > 0)
    {
        int32 UnitsInThisRow = FMath::Min(CurrentRowSize, RemainingUnits);
        RowDistribution.Add(UnitsInThisRow);
        RemainingUnits -= UnitsInThisRow;
        CurrentRowSize += 2; // 每行增加2个单位
    }

    // 计算阵型尺寸
    int32 MaxUnitsInRow = RowDistribution.Last();
    float Width = (MaxUnitsInRow - 1) * Spacing;
    float Height = (RowDistribution.Num() - 1) * Spacing;
    Formation.Size = FVector2D(Width, Height);

    // 生成位置（箭头指向前方）
    float StartY = Height * 0.5f; // 箭头尖端在前

    int32 CurrentUnit = 0;
    for (int32 Row = 0; Row < RowDistribution.Num() && CurrentUnit < UnitCount; Row++)
    {
        int32 UnitsInThisRow = RowDistribution[Row];
        float RowStartX = -(UnitsInThisRow - 1) * Spacing * 0.5f;

        for (int32 Col = 0; Col < UnitsInThisRow && CurrentUnit < UnitCount; Col++)
        {
            FVector Position(
                RowStartX + Col * Spacing,
                StartY - Row * Spacing,
                0.0f
            );
            Formation.Positions.Add(Position);
            CurrentUnit++;
        }
    }

    return Formation;
}

FFormationData UFormationLibrary::CreateSpiralFormation(
    FVector CenterLocation,
    FRotator Rotation,
    int32 UnitCount,
    float Radius,
    float Turns)
{
    FFormationData Formation;
    Formation.FormationType = EFormationType::Spiral;
    Formation.CenterLocation = CenterLocation;
    Formation.Rotation = Rotation;
    Formation.Size = FVector2D(Radius * 2.0f, Radius * 2.0f);

    if (UnitCount <= 0)
    {
        return Formation;
    }

    Formation.Positions.Reserve(UnitCount);

    if (UnitCount == 1)
    {
        Formation.Positions.Add(FVector::ZeroVector);
    }
    else
    {
        float TotalAngle = Turns * 360.0f;
        float AngleStep = TotalAngle / (UnitCount - 1);

        for (int32 i = 0; i < UnitCount; i++)
        {
            float Progress = static_cast<float>(i) / (UnitCount - 1);
            float CurrentRadius = Radius * Progress; // 半径逐渐增大
            float Angle = i * AngleStep;
            float RadianAngle = FMath::DegreesToRadians(Angle);

            FVector Position(
                CurrentRadius * FMath::Cos(RadianAngle),
                CurrentRadius * FMath::Sin(RadianAngle),
                0.0f
            );
            Formation.Positions.Add(Position);
        }
    }

    return Formation;
}

FFormationData UFormationLibrary::CreateSolidCircleFormation(
    FVector CenterLocation,
    FRotator Rotation,
    int32 UnitCount,
    float Radius)
{
    FFormationData Formation;
    Formation.FormationType = EFormationType::SolidCircle;
    Formation.CenterLocation = CenterLocation;
    Formation.Rotation = Rotation;
    Formation.Size = FVector2D(Radius * 2.0f, Radius * 2.0f);

    if (UnitCount <= 0)
    {
        return Formation;
    }

    Formation.Positions.Reserve(UnitCount);

    if (UnitCount == 1)
    {
        Formation.Positions.Add(FVector::ZeroVector);
    }
    else
    {
        // 计算需要多少层圆环
        int32 RemainingUnits = UnitCount;
        float CurrentRadius = 0.0f;
        float RadiusStep = Radius / FMath::Max(1.0f, FMath::Sqrt(static_cast<float>(UnitCount)) * 0.5f);

        // 中心点
        if (RemainingUnits > 0)
        {
            Formation.Positions.Add(FVector::ZeroVector);
            RemainingUnits--;
        }

        // 生成同心圆环
        while (RemainingUnits > 0 && CurrentRadius < Radius)
        {
            CurrentRadius += RadiusStep;

            // 计算这一层可以放多少个单位
            float Circumference = 2.0f * PI * CurrentRadius;
            int32 UnitsInThisRing = FMath::Max(1, FMath::FloorToInt(Circumference / (RadiusStep * 0.8f)));
            UnitsInThisRing = FMath::Min(UnitsInThisRing, RemainingUnits);

            float AngleStep = 360.0f / UnitsInThisRing;

            for (int32 i = 0; i < UnitsInThisRing; i++)
            {
                float Angle = i * AngleStep;
                float RadianAngle = FMath::DegreesToRadians(Angle);

                FVector Position(
                    CurrentRadius * FMath::Cos(RadianAngle),
                    CurrentRadius * FMath::Sin(RadianAngle),
                    0.0f
                );
                Formation.Positions.Add(Position);
                RemainingUnits--;
            }
        }
    }

    return Formation;
}

FFormationData UFormationLibrary::CreateZigzagFormation(
    FVector CenterLocation,
    FRotator Rotation,
    int32 UnitCount,
    float Spacing,
    float ZigzagAmplitude)
{
    FFormationData Formation;
    Formation.FormationType = EFormationType::Zigzag;
    Formation.CenterLocation = CenterLocation;
    Formation.Rotation = Rotation;
    Formation.Spacing = Spacing;

    if (UnitCount <= 0)
    {
        return Formation;
    }

    Formation.Positions.Reserve(UnitCount);

    // 计算折线的总长度和高度
    float TotalLength = (UnitCount - 1) * Spacing;
    Formation.Size = FVector2D(TotalLength, ZigzagAmplitude * 2.0f);

    // 生成折线位置
    float StartX = -TotalLength * 0.5f;

    for (int32 i = 0; i < UnitCount; i++)
    {
        float X = StartX + i * Spacing;

        // 使用正弦波生成Y坐标
        float Progress = static_cast<float>(i) / FMath::Max(1.0f, UnitCount - 1.0f);
        float Y = ZigzagAmplitude * FMath::Sin(Progress * PI * 4.0f); // 4个周期的正弦波

        FVector Position(X, Y, 0.0f);
        Formation.Positions.Add(Position);
    }

    return Formation;
}

FFormationData UFormationLibrary::CreateCustomFormation(
    FVector CenterLocation,
    FRotator Rotation,
    const TArray<FVector>& RelativePositions)
{
    FFormationData Formation;
    Formation.FormationType = EFormationType::Custom;
    Formation.CenterLocation = CenterLocation;
    Formation.Rotation = Rotation;
    Formation.Positions = RelativePositions;

    // 计算包围盒尺寸
    if (RelativePositions.Num() > 0)
    {
        FBox Bounds(RelativePositions[0], RelativePositions[0]);
        for (const FVector& Pos : RelativePositions)
        {
            Bounds += Pos;
        }
        FVector Size = Bounds.GetSize();
        Formation.Size = FVector2D(Size.X, Size.Y);
    }

    return Formation;
}

FFormationData UFormationLibrary::GetCurrentFormationFromActors(
    const TArray<AActor*>& Units,
    FVector& CenterLocation)
{
    FFormationData Formation;
    Formation.FormationType = EFormationType::Custom;

    if (Units.Num() == 0)
    {
        CenterLocation = FVector::ZeroVector;
        return Formation;
    }

    // 计算中心位置
    FVector Sum = FVector::ZeroVector;
    TArray<FVector> WorldPositions;
    WorldPositions.Reserve(Units.Num());

    for (AActor* Unit : Units)
    {
        if (IsValid(Unit))
        {
            FVector Location = Unit->GetActorLocation();
            WorldPositions.Add(Location);
            Sum += Location;
        }
    }

    if (WorldPositions.Num() == 0)
    {
        CenterLocation = FVector::ZeroVector;
        return Formation;
    }

    CenterLocation = Sum / WorldPositions.Num();
    Formation.CenterLocation = CenterLocation;

    // 计算相对位置
    Formation.Positions.Reserve(WorldPositions.Num());
    for (const FVector& WorldPos : WorldPositions)
    {
        Formation.Positions.Add(WorldPos - CenterLocation);
    }

    // 计算包围盒尺寸
    if (Formation.Positions.Num() > 0)
    {
        FBox Bounds(Formation.Positions[0], Formation.Positions[0]);
        for (const FVector& Pos : Formation.Positions)
        {
            Bounds += Pos;
        }
        FVector Size = Bounds.GetSize();
        Formation.Size = FVector2D(Size.X, Size.Y);
    }

    return Formation;
}

FBox UFormationLibrary::GetFormationBounds(const FFormationData& Formation)
{
    return Formation.GetAABB();
}

FFormationData UFormationLibrary::ScaleFormation(const FFormationData& Formation, float Scale)
{
    FFormationData ScaledFormation = Formation;

    // 缩放所有位置
    for (FVector& Position : ScaledFormation.Positions)
    {
        Position *= Scale;
    }

    // 缩放尺寸和间距
    ScaledFormation.Size *= Scale;
    ScaledFormation.Spacing *= Scale;

    return ScaledFormation;
}

FFormationData UFormationLibrary::RotateFormation(const FFormationData& Formation, FRotator AdditionalRotation)
{
    FFormationData RotatedFormation = Formation;
    RotatedFormation.Rotation = UKismetMathLibrary::ComposeRotators(Formation.Rotation, AdditionalRotation);
    return RotatedFormation;
}

FFormationData UFormationLibrary::MoveFormation(const FFormationData& Formation, FVector NewCenterLocation)
{
    FFormationData MovedFormation = Formation;
    MovedFormation.CenterLocation = NewCenterLocation;
    return MovedFormation;
}

FFormationData UFormationLibrary::ResizeFormation(const FFormationData& Formation, int32 NewUnitCount)
{
    if (NewUnitCount <= 0)
    {
        FFormationData EmptyFormation = Formation;
        EmptyFormation.Positions.Empty();
        return EmptyFormation;
    }

    // 根据阵型类型重新生成
    switch (Formation.FormationType)
    {
        case EFormationType::Square:
            return CreateSquareFormation(Formation.CenterLocation, Formation.Rotation, NewUnitCount, Formation.Spacing);

        case EFormationType::Circle:
        {
            float Radius = Formation.Size.X * 0.5f;
            return CreateCircleFormation(Formation.CenterLocation, Formation.Rotation, NewUnitCount, Radius);
        }

        case EFormationType::Line:
        {
            bool bVertical = Formation.Size.Y > Formation.Size.X;
            return CreateLineFormation(Formation.CenterLocation, Formation.Rotation, NewUnitCount, Formation.Spacing, bVertical);
        }

        case EFormationType::Triangle:
            return CreateTriangleFormation(Formation.CenterLocation, Formation.Rotation, NewUnitCount, Formation.Spacing);

        case EFormationType::Arrow:
            return CreateArrowFormation(Formation.CenterLocation, Formation.Rotation, NewUnitCount, Formation.Spacing);

        case EFormationType::Spiral:
        {
            float Radius = Formation.Size.X * 0.5f;
            return CreateSpiralFormation(Formation.CenterLocation, Formation.Rotation, NewUnitCount, Radius);
        }

        case EFormationType::SolidCircle:
        {
            float Radius = Formation.Size.X * 0.5f;
            return CreateSolidCircleFormation(Formation.CenterLocation, Formation.Rotation, NewUnitCount, Radius);
        }

        case EFormationType::Zigzag:
            return CreateZigzagFormation(Formation.CenterLocation, Formation.Rotation, NewUnitCount, Formation.Spacing);

        case EFormationType::Custom:
        default:
        {
            // 对于自定义阵型，按比例调整
            FFormationData ResizedFormation = Formation;
            int32 OriginalCount = Formation.Positions.Num();

            if (NewUnitCount > OriginalCount)
            {
                // 增加单位：复制现有位置并添加随机偏移
                ResizedFormation.Positions.Reserve(NewUnitCount);
                for (int32 i = OriginalCount; i < NewUnitCount; i++)
                {
                    int32 SourceIndex = i % OriginalCount;
                    FVector NewPos = Formation.Positions[SourceIndex];
                    // 添加小的随机偏移避免重叠
                    NewPos += FVector(
                        FMath::RandRange(-50.0f, 50.0f),
                        FMath::RandRange(-50.0f, 50.0f),
                        0.0f
                    );
                    ResizedFormation.Positions.Add(NewPos);
                }
            }
            else
            {
                // 减少单位：保留前N个位置
                ResizedFormation.Positions.SetNum(NewUnitCount);
            }

            return ResizedFormation;
        }
    }
}

void UFormationLibrary::DrawFormationDebug(
    const UObject* WorldContext,
    const FFormationData& Formation,
    float Duration,
    FLinearColor Color,
    float Thickness)
{
    UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull);
    if (!World)
    {
        return;
    }

    TArray<FVector> WorldPositions = Formation.GetWorldPositions();
    FColor DrawColor = Color.ToFColor(true);

    // 绘制每个位置点
    for (const FVector& Position : WorldPositions)
    {
        DrawDebugSphere(World, Position, 25.0f, 8, DrawColor, false, Duration, 0, Thickness);
    }

    // 绘制阵型中心
    DrawDebugSphere(World, Formation.CenterLocation, 40.0f, 12, FColor::White, false, Duration, 0, Thickness + 1.0f);

    // 绘制包围盒
    FBox Bounds = Formation.GetAABB();
    DrawDebugBox(World, Bounds.GetCenter(), Bounds.GetExtent(), DrawColor, false, Duration, 0, Thickness);

    // 根据阵型类型绘制特殊标记
    switch (Formation.FormationType)
    {
        case EFormationType::Circle:
        {
            float Radius = Formation.Size.X * 0.5f;
            DrawDebugCircle(World, Formation.CenterLocation, Radius, 32, DrawColor, false, Duration, 0, Thickness);
            break;
        }
        case EFormationType::Line:
        {
            if (WorldPositions.Num() >= 2)
            {
                DrawDebugLine(World, WorldPositions[0], WorldPositions.Last(), DrawColor, false, Duration, 0, Thickness);
            }
            break;
        }
    }
}

bool UFormationLibrary::ValidateFormationData(const FFormationData& Formation, FString& ErrorMessage)
{
    ErrorMessage.Empty();

    if (Formation.Positions.Num() == 0)
    {
        ErrorMessage = TEXT("阵型位置数组为空");
        return false;
    }

    if (Formation.Spacing <= 0.0f)
    {
        ErrorMessage = TEXT("阵型间距必须大于0");
        return false;
    }

    if (Formation.Size.X < 0.0f || Formation.Size.Y < 0.0f)
    {
        ErrorMessage = TEXT("阵型尺寸不能为负数");
        return false;
    }

    return true;
}

float UFormationLibrary::CalculateTransitionCost(
    const FFormationData& FromFormation,
    const FFormationData& ToFormation,
    EFormationTransitionMode TransitionMode)
{
    if (FromFormation.Positions.Num() != ToFormation.Positions.Num())
    {
        return -1.0f; // 无效的比较
    }

    TArray<FVector> FromPositions = FromFormation.GetWorldPositions();
    TArray<FVector> ToPositions = ToFormation.GetWorldPositions();

    float TotalCost = 0.0f;

    switch (TransitionMode)
    {
        case EFormationTransitionMode::OptimizedAssignment:
        {
            // 计算相对位置成本
            FBox FromAABB(FromPositions[0], FromPositions[0]);
            FBox ToAABB(ToPositions[0], ToPositions[0]);

            for (const FVector& Pos : FromPositions) FromAABB += Pos;
            for (const FVector& Pos : ToPositions) ToAABB += Pos;

            FVector FromSize = FromAABB.GetSize();
            FVector ToSize = ToAABB.GetSize();

            FromSize.X = FMath::Max(FromSize.X, 1.0f);
            FromSize.Y = FMath::Max(FromSize.Y, 1.0f);
            FromSize.Z = FMath::Max(FromSize.Z, 1.0f);
            ToSize.X = FMath::Max(ToSize.X, 1.0f);
            ToSize.Y = FMath::Max(ToSize.Y, 1.0f);
            ToSize.Z = FMath::Max(ToSize.Z, 1.0f);

            for (int32 i = 0; i < FromPositions.Num(); i++)
            {
                FVector RelativeFrom = (FromPositions[i] - FromAABB.Min) / FromSize;
                FVector RelativeTo = (ToPositions[i] - ToAABB.Min) / ToSize;
                TotalCost += FVector::Dist(RelativeFrom, RelativeTo);
            }
            break;
        }
        case EFormationTransitionMode::SimpleAssignment:
        {
            // 计算绝对距离成本
            for (int32 i = 0; i < FromPositions.Num(); i++)
            {
                TotalCost += FVector::Dist(FromPositions[i], ToPositions[i]);
            }
            break;
        }
        case EFormationTransitionMode::DirectMapping:
        default:
        {
            // 直接映射成本
            for (int32 i = 0; i < FromPositions.Num(); i++)
            {
                TotalCost += FVector::Dist(FromPositions[i], ToPositions[i]);
            }
            break;
        }
    }

    return TotalCost;
}

void UFormationLibrary::CalculateOptimalRowsCols(int32 UnitCount, int32& OutRows, int32& OutCols)
{
    if (UnitCount <= 0)
    {
        OutRows = OutCols = 0;
        return;
    }

    // 尝试创建尽可能接近正方形的阵型
    float SqrtCount = FMath::Sqrt(static_cast<float>(UnitCount));
    OutCols = FMath::CeilToInt(SqrtCount);
    OutRows = FMath::CeilToInt(static_cast<float>(UnitCount) / OutCols);

    // 优化：如果可能，调整行列数使其更均匀
    if (OutRows * (OutCols - 1) >= UnitCount)
    {
        OutCols--;
    }
}

TArray<int32> UFormationLibrary::GenerateTriangleRowDistribution(int32 UnitCount, bool bInverted)
{
    TArray<int32> RowDistribution;

    if (UnitCount <= 0)
    {
        return RowDistribution;
    }

    // 计算三角形的行数
    int32 Rows = 1;
    int32 TotalUnits = 0;

    // 找到能容纳所有单位的最小行数
    while (TotalUnits < UnitCount)
    {
        TotalUnits += Rows;
        if (TotalUnits < UnitCount)
        {
            Rows++;
        }
    }

    // 生成每行的单位数
    RowDistribution.Reserve(Rows);
    int32 RemainingUnits = UnitCount;

    for (int32 Row = 0; Row < Rows; Row++)
    {
        int32 UnitsInThisRow;

        if (bInverted)
        {
            // 倒三角：从多到少
            UnitsInThisRow = FMath::Min(RemainingUnits, Rows - Row);
        }
        else
        {
            // 正三角：从少到多
            UnitsInThisRow = FMath::Min(RemainingUnits, Row + 1);
        }

        RowDistribution.Add(UnitsInThisRow);
        RemainingUnits -= UnitsInThisRow;

        if (RemainingUnits <= 0)
        {
            break;
        }
    }

    return RowDistribution;
}
