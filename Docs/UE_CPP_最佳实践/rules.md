## C++标准和编译配置
- 使用稳定的C++17标准，除非确实需要C++20特性否则避免使用
- 在Build.cs文件中始终设置 `bEnforceIWYU = true` 强制执行"包含你所使用的"原则
- 开发时默认设置 `bUseUnity = false` 确保代码质量并暴露依赖问题
- 遵循UE标准设置 `bEnableExceptions = false` 和 `bUseRTTI = false`
- 使用 `PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs` 获得最佳编译性能

## UE内置API优先原则
- 始终优先使用UE内置容器而非STL：使用TArray、TMap、TSet而不是std::vector、std::map、std::set
- 使用UE智能指针：TSharedPtr、TUniquePtr、TWeakPtr而不是std智能指针
- 使用UE字符串类型：FString用于通用字符串，FName用于标识符，FText用于本地化文本
- 优先使用UE数学库和算法而不是自定义实现
- 使用UE内存管理模式，避免手动内存管理

## 命名约定
- 严格遵循UE命名约定：
  - 类：普通类使用F前缀，UObject派生类使用U前缀，AActor派生类使用A前缀
  - 接口：接口类使用I前缀
  - 枚举：枚举类型使用E前缀
  - 布尔变量：使用b前缀（如bIsEnabled）
  - 成员变量：使用帕斯卡命名法，不使用前缀
  - 函数：动作使用动词开头，布尔返回值使用Is/Has/Can开头

## 代码组织和依赖关系
- 在头文件中尽可能优先使用前向声明而不是#include
- 使用IWYU原则：每个文件只包含实际使用的内容
- 按顺序组织包含文件：自己的头文件、UE头文件、第三方头文件、标准库
- 保持头文件简洁，将实现细节移到.cpp文件中
- 使用const正确性：当函数不修改对象状态时标记为const

## 性能优化模式
- 在容器中构造对象时使用Emplace()而不是Add()
- 使用TStringBuilder进行高效字符串拼接，而不是重复的FString操作
- 对大对象参数优先使用const引用以避免不必要的拷贝
- 对频繁分配/释放的对象使用对象池
- 当已知大小时预分配容器容量：使用Reserve()方法

## 错误处理模式
- 对可能失败的操作使用TResult<SuccessType, ErrorType>
- 遵循UE模式：简单情况使用bool返回值+out参数
- 对可能不存在的值使用TOptional<T>
- 对不可恢复的程序员错误使用check()（仅调试版本）
- 对可恢复的错误使用ensure()，在开发中触发断点
- 当需要在发布版本中执行表达式时使用verify()

## 内存管理
- 使用RAII模式进行资源管理
- 实现适当的移动构造函数和赋值操作符
- 对UObject派生类使用UE垃圾回收
- 避免原始指针；使用智能指针或UE对象引用
- 对性能关键的临时对象实现对象池

## 线程和并发
- 使用UE的异步任务系统：Async()和AsyncTask()函数
- 简单锁定使用FCriticalSection，读取密集场景使用FRWLock
- 简单线程安全计数器使用原子操作（TAtomic）
- 优先使用不可变数据结构保证线程安全
- 可用时使用UE的线程安全容器

## 插件架构
- 清晰分离模块：Core（运行时）、Editor（仅编辑器）、Tests（测试）
- 使用接口驱动设计，接口类使用I前缀
- 实现适当的模块启动和关闭序列
- 使用UE子系统框架处理全局服务
- 设计事件系统实现模块间松耦合

## 最优插件目录结构
- 使用以下标准目录结构：
  ```
  MyPlugin/
  ├── Source/
  │   ├── MyPluginCore/          # 核心运行时模块
  │   │   ├── Public/
  │   │   │   ├── MyPluginCore.h
  │   │   │   ├── Interfaces/    # 接口定义
  │   │   │   ├── Data/          # 数据结构
  │   │   │   └── Services/      # 服务类
  │   │   ├── Private/
  │   │   └── MyPluginCore.Build.cs
  │   ├── MyPluginEditor/        # 编辑器模块
  │   │   ├── Public/
  │   │   ├── Private/
  │   │   └── MyPluginEditor.Build.cs
  │   └── MyPluginTests/         # 测试模块
  ├── Content/                   # 蓝图和资源
  ├── Config/                    # 配置文件
  ├── Resources/                 # 图标等资源
  └── MyPlugin.uplugin          # 插件描述文件
  ```

## 模块依赖最佳实践
- Core模块依赖保持最小：仅依赖Core、CoreUObject、Engine
- Editor模块依赖Core模块，添加UnrealEd、EditorStyle、ToolMenus等编辑器依赖
- 在Build.cs中设置正确的PCH和IWYU配置：
  ```cpp
  PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
  bEnforceIWYU = true;
  ```
- 使用适当的模块类型：Runtime、Editor、Developer、ThirdParty
- 实现正确的模块加载阶段：PreDefault、Default、PostEngineInit等

## 插件文件组织规范
- 接口定义放在Public/Interfaces/目录下，使用I前缀命名
- 数据结构放在Public/Data/目录下，使用F前缀命名结构体
- 服务类放在Public/Services/目录下，使用U前缀命名UObject派生类
- 私有实现放在Private/目录下，按功能模块分子目录
- 配置文件放在Config/目录下，使用Default前缀命名
- 编辑器资源（图标等）放在Resources/目录下
- 蓝图和内容资源放在Content/目录下

## 插件模块初始化模式
- 实现IModuleInterface接口的StartupModule()和ShutdownModule()方法
- 在StartupModule()中注册子系统、命令、菜单等
- 在ShutdownModule()中清理资源、注销回调等
- 使用IMPLEMENT_MODULE宏正确导出模块
- 为Editor模块实现延迟初始化，避免启动时阻塞

## 插件子系统设计
- 使用UEngineSubsystem作为全局服务的基类
- 使用UEditorSubsystem作为编辑器专用服务的基类
- 实现Initialize()和Deinitialize()方法管理子系统生命周期
- 提供静态Get()方法便于访问子系统实例
- 使用事件委托实现子系统间的松耦合通信

## 配置和设置
- 使用UDeveloperSettings处理编辑器可配置设置
- 使用适当的Config=说明符实现正确的配置文件组织
- 支持开发设置的热重载
- 为所有配置选项提供合理的默认值
- 使用meta标签生成适当的编辑器UI

## 蓝图集成
- 使用UFUNCTION(BlueprintCallable)标记适当的函数
- 对暴露的变量使用UPROPERTY(BlueprintReadOnly/BlueprintReadWrite)
- 提供有意义的Category和DisplayName meta标签
- 对无副作用的函数使用BlueprintPure
- 实现适当的蓝图节点标题和工具提示

## 本地化和国际化
- 对所有用户可见字符串使用LOCTEXT宏
- 在文件开头定义LOCTEXT_NAMESPACE
- 使用FText::Format进行动态文本格式化
- 避免对本地化文本使用字符串拼接
- 支持带参数的适当文本格式化

## 代码文档
- 使用/** */样式为公共API提供清晰的文档注释
- 为函数文档使用@param和@return标签
- 在复杂API文档中包含使用示例
- 记录公共方法的线程安全保证
- 保持文档简洁但完整

## 要避免的反模式
- 当UE等效容器存在时不要使用STL容器
- 当UE系统可以处理时不要手动管理内存
- 不要忽视const正确性
- 不要使用原始指针进行对象所有权管理
- 当UE提供优化版本时不要实现自定义算法
- 没有强有力理由时不要使用C++20特性
- 不要绕过IWYU要求
- 不要在模块间创建循环依赖
- 不要在UE代码中使用异常
- 不要忽视UE编码标准和命名约定

## 测试和调试
- 为关键功能编写自动化测试
- 使用UE内置的性能分析工具（Insights、Stats系统）
- 使用适当的日志类别实现正确的日志记录
- 使用性能计数器监控关键路径
- 使用不同的构建配置进行测试（Debug、Development、Shipping）

## 模块依赖
- 保持Core模块依赖最小化（Core、CoreUObject、Engine）
- Editor模块应该依赖Core模块，而不是相反
- 避免模块间的循环依赖
- 使用适当的加载阶段进行模块初始化
- 记录模块依赖的理由
