# 待发布更新 (UNRELEASED)

> 此文档记录尚未发布的功能更新、修复和移除，用于下次版本发布时的描述参考。
> 发布新版本时，将此文件内容合并到 CHANGELOG.md，然后清空此文件（保留说明部分）。

---

## PointSampling

- **修复** 泊松缓存键哈希/相等性契约不一致（改用量化比较）
- **修复** 泊松缓存键缺少 Scale 和 MaxAttempts 字段
- **修复** ApplyTransform 中除零风险（SafeScale 检查）
- **修复** 随机种子参数失效（新增 GeneratePoisson2D/3DFromStream）
- **修复** 纹理采样坐标归一化错误（[0..W] 误当 [-W/2..W/2]）
- **修复** FloatRGBA 格式 FP16/FP32 平台差异（动态判断字节数）
- **修复** 纹理采样数据越界风险（添加大小验证和范围 Clamp）
- **优化** 泊松采样日志级别从 Log 改为 Verbose（减少刷屏）
- **优化** EPoissonCoordinateSpace 枚举文档（详细说明 World/Local/Raw 差异）
- **调整** RenderCore 从 Public 依赖移至 Private（符合 IWYU 原则）
- **调整** 新增 RHI 模块依赖（用于 EPixelFormat 定义）
- **移除** MeshSamplingHelper 中未使用的 SocketRotation 和 SocketTransform 变量

## SortEditor

- **修复** K2Node_SmartSort 提升枚举引脚为变量时第二个引脚消失
- **修复** 提升为变量后排序模式无效（始终使用默认值）
- **修复** Vector数组连接后标题错误显示为"结构体属性排序"
- **重构** 使用统一入口函数替代Switch分支（SortVectorsUnified/SortActorsUnified）
- **优化** 提升为变量时使用 AdvancedView 折叠不常用引脚
- **增加** 排序模式引脚连接状态检测（连接时显示所有可能引脚）
- **增加** 排序模式引脚连接变化监听（PinConnectionListChanged）
- **优化** 动态引脚 Tooltip 更新（描述各引脚适用场景）

## PhysicsAssetOptimizer

- **新增** 物理资产一键优化模块（Editor Only，仅 Win64）
- **新增** 骨骼识别系统（命名规则+拓扑链+几何校验）
- **新增** 编辑器UI集成（物理资产编辑器工具栏"自动优化"按钮）
- **新增** 自动创建父子骨骼约束（使用UE官方API：CreateNewConstraint + SnapTransformsToDefault）
- **新增** 可配置的物理形体规则（FBonePhysicsRule，支持每种骨骼类型独立配置形状）
- **重构** 使用UE官方API创建碰撞形状（CalcBoneVertInfos + CreateCollisionFromBone）
- **重构** 按UE官方最佳实践重建物理形体（清空后重建核心骨骼）
- **优化** 碰撞形状自动对齐骨骼方向（bAutoOrientToBone=true）
- **修复** 武器骨骼误识别为人形骨骼（移除后缀判断依赖）
- **修复** UE5.5+兼容性（TObjectPtr转换、SkeletalBodySetup头文件）

## 质量保证

- **检查** 全面审查 20 个 K2Node 节点，确认无类似引脚消失问题

## CI/CD

- **优化** 移除 Artifact 上传前的手动压缩，直接使用 upload-artifact@v4 自动压缩
- **优化** Release zip 仅在 tag push 时创建（非 tag 构建不再双重压缩）
- **清理** 从仓库移除误提交的 ci日志 目录

---

## 📋 日志格式说明

### 模块分类
按模块名称分为独立章节，每个章节内使用简洁的列表格式记录变更

### 记录格式
```markdown
## 模块名

- 类型 简要描述
- 类型 简要描述
- 类型 简要描述
```

**类型说明**（单字或双字）:
- **增加** / 新增功能
- **优化** / 改进现有功能
- **修复** / 修复问题
- **移除** / 删除功能
- **调整** / 参数或行为调整
- **集成** / 新增模块集成
- **本地化** / 本地化工作
### 内容要求
1. **简洁**：每行记录一个独立变更，描述控制在20字内
2. **清晰**：使用"动词 + 对象 +（效果）"格式
3. **关键信息**：包含重要的参数变化、性能数据、修复的问题
4. **不要**：避免详细技术细节、文件路径、长篇问题分析

### 发布流程
1. 开发过程中持续在此文件顶部添加新记录
2. 版本发布前整理和分类内容
3. 复制内容到 CHANGELOG.md，更新版本号和日期
4. 清空此文件（保留说明部分），准备下一轮开发

---

