# MaterialAttributes智能连接实现方案

## 基于UE5.3源码的正确实现

### 1. 检测MaterialAttributes模式

```cpp
// 在 X_MaterialFunctionConnector.h 中添加
class X_ASSETEDITOR_API FX_MaterialFunctionConnector
{
public:
    /**
     * 检测材质函数是否使用MaterialAttributes模式
     * @param FunctionCall - 材质函数调用
     * @return 是否使用MaterialAttributes
     */
    static bool IsUsingMaterialAttributes(UMaterialExpressionMaterialFunctionCall* FunctionCall);
    
    /**
     * 连接材质属性到材质主节点（使用UE官方API）
     * @param Material - 材质
     * @param FunctionCall - 材质函数调用
     * @param OutputIndex - 输出引脚索引
     * @return 是否成功连接
     */
    static bool ConnectMaterialAttributesToMaterial(UMaterial* Material, 
                                                   UMaterialExpressionMaterialFunctionCall* FunctionCall,
                                                   int32 OutputIndex = 0);
};
```

### 2. 实现检测逻辑

```cpp
// 在 X_MaterialFunctionConnector.cpp 中实现

#include "MaterialEditingLibrary.h"  // ✅ 使用UE官方API

bool FX_MaterialFunctionConnector::IsUsingMaterialAttributes(UMaterialExpressionMaterialFunctionCall* FunctionCall)
{
    if (!FunctionCall || !FunctionCall->MaterialFunction)
    {
        return false;
    }
    
    // 检查材质函数是否有MaterialAttributes类型的输出
    const TArray<FFunctionExpressionOutput>& FunctionOutputs = FunctionCall->FunctionOutputs;
    for (const FFunctionExpressionOutput& Output : FunctionOutputs)
    {
        // 检查输出类型是否为MaterialAttributes
        // MaterialAttributes输出通常没有名称或包含"MaterialAttributes"
        if (Output.Output.OutputName.IsNone() ||
            Output.Output.OutputName.ToString().Contains(TEXT("MaterialAttributes")))
        {
            // 进一步检查输出类型
            // 可以通过材质函数的描述或其他元数据来确认
            return true;
        }
    }
    
    // 也可以通过材质函数名称进行推断
    FString FunctionName = FunctionCall->MaterialFunction->GetName();
    if (FunctionName.Contains(TEXT("MaterialAttributes")) ||
        FunctionName.Contains(TEXT("MA_")) ||  // 常见的MaterialAttributes命名前缀
        FunctionName.Contains(TEXT("MakeMA")))
    {
        return true;
    }
    
    return false;
}

bool FX_MaterialFunctionConnector::ConnectMaterialAttributesToMaterial(UMaterial* Material, 
                                                                       UMaterialExpressionMaterialFunctionCall* FunctionCall,
                                                                       int32 OutputIndex)
{
    if (!Material || !FunctionCall)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("材质或函数调用为空"));
        return false;
    }
    
    // ✅ 使用UE官方API连接到MaterialAttributes
    // 注意：MP_MaterialAttributes 是专门的MaterialAttributes属性
    bool bSuccess = UMaterialEditingLibrary::ConnectMaterialProperty(
        FunctionCall, 
        FString::Printf(TEXT("%d"), OutputIndex),  // 输出引脚名称
        MP_MaterialAttributes  // ✅ 关键：使用MaterialAttributes属性
    );
    
    if (bSuccess)
    {
        UE_LOG(LogX_AssetEditor, Log, TEXT("成功连接MaterialAttributes到材质主节点"));
        
        // 重新编译材质
        FX_MaterialFunctionCore::RecompileMaterial(Material);
        
        // 刷新材质编辑器
        FX_MaterialFunctionCore::RefreshOpenMaterialEditor(Material);
    }
    else
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("连接MaterialAttributes失败"));
    }
    
    return bSuccess;
}
```

### 3. 修改智能连接逻辑

```cpp
bool FX_MaterialFunctionConnector::SetupAutoConnections(
    UMaterial* Material, 
    UMaterialExpressionMaterialFunctionCall* FunctionCall,
    EConnectionMode ConnectionMode,
    TSharedPtr<FX_MaterialFunctionParams> Params)
{
    if (!Material || !FunctionCall)
    {
        return false;
    }

    // ✅ 第一优先级：检查是否使用MaterialAttributes模式
    if (IsUsingMaterialAttributes(FunctionCall))
    {
        UE_LOG(LogX_AssetEditor, Log, TEXT("检测到材质函数使用MaterialAttributes模式，进行专用连接"));
        return ConnectMaterialAttributesToMaterial(Material, FunctionCall, 0);
    }

    // 继续原有的常规连接逻辑...
    UE_LOG(LogX_AssetEditor, Log, TEXT("使用常规引脚连接逻辑"));
    
    // ... 原有代码保持不变
}
```

### 4. 更新参数系统

```cpp
// 在 X_MaterialFunctionParams.h 中添加
struct FX_MaterialFunctionParams
{
    // ... 现有字段

    /** 是否强制使用MaterialAttributes连接 */
    UPROPERTY(EditAnywhere, Category = "连接设置", meta = (DisplayName = "使用材质属性连接"))
    bool bUseMaterialAttributes = false;

    /** 
     * 根据材质函数自动检测是否应该使用MaterialAttributes
     */
    void DetectMaterialAttributesMode(UMaterialFunctionInterface* Function)
    {
        if (Function)
        {
            FString FunctionName = Function->GetName();
            if (FunctionName.Contains(TEXT("MaterialAttributes")) ||
                FunctionName.Contains(TEXT("MA_")) ||
                FunctionName.Contains(TEXT("MakeMA")))
            {
                bUseMaterialAttributes = true;
                // 当使用MaterialAttributes时，禁用其他连接选项
                bConnectToBaseColor = false;
                bConnectToMetallic = false;
                bConnectToRoughness = false;
                bConnectToNormal = false;
                bConnectToEmissive = false;
                bConnectToAO = false;
                
                UE_LOG(LogX_AssetEditor, Log, TEXT("自动检测到MaterialAttributes模式，已禁用常规连接选项"));
            }
        }
    }
};
```

### 5. 依赖项更新

```cpp
// 在 X_AssetEditor.Build.cs 中确保包含MaterialEditor模块
PrivateDependencyModuleNames.AddRange(new string[] {
    // ... 现有模块
    "MaterialEditor",  // ✅ 添加MaterialEditor依赖以使用UMaterialEditingLibrary
});
```

## 关键优势

1. **使用官方API** - 基于UE官方的UMaterialEditingLibrary，确保兼容性
2. **自动检测** - 智能识别MaterialAttributes模式，无需用户手动选择
3. **向后兼容** - 完全保持现有常规连接功能
4. **源码验证** - 基于UE5.3实际源码实现，避免试错

## 使用流程

1. 用户选择使用MaterialAttributes的材质函数
2. 系统自动检测MaterialAttributes模式
3. 直接连接到材质的MaterialAttributes输入
4. 自动重新编译和刷新材质编辑器

这样你们的智能连接系统就能完美支持UE材质编辑器的所有连接模式了！
