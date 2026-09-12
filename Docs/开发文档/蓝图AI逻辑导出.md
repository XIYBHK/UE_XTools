# 蓝图 AI 逻辑导出

在内容浏览器选中蓝图，使用“导出蓝图逻辑流”。每个资产写入 `Saved/XTools/BlueprintExports/<资产名>_<完整对象路径SHA1>/`。目录结构如下：

```text
00_START_HERE.md                 # 总目录：选择相关资产目录
<资产>/
├── 00_START_HERE.md             # 资产入口、图索引、外部宏依赖和停止条件
├── 01_Manifest.json             # v2 机器清单：图身份、候选入口与内容摘要
├── 05_Query.py                  # Python 标准库只读查询器
├── 10_Logic/G0001.pseudo.md     # 按图生成、带标签的确定性伪代码
├── 20_Evidence/00_Asset.json    # 完整资产元数据，不含 graphs
├── 20_Evidence/G0001.json       # 对应图的原完整 JSON
└── 90_Full/                     # 原完整 JSON、.ai.md、.md 三种格式
```

入口禁止递归全读或把全部文件塞进上下文。先读总目录和资产入口，按问题选择图的 `10_Logic/Gxxxx.pseudo.md`；证据足够时停止。只有缺少类型、pin 属性或未知节点细节时，才查询同编号的 `20_Evidence/Gxxxx.json`；变量、组件、时间轴或类信息不足时，查询 `20_Evidence/00_Asset.json` 的对应键。`90_Full/` 仅用于全局审计、工具解析或最后查证，不是常规初始上下文。

## 大图按入口或节点读取

在资产目录运行以下命令，需要 Python 3.9+，无需安装依赖或启动 UE。先取得本次导出的图/节点编号，再按问题选择：

```text
python 05_Query.py outline
python 05_Query.py find --query BeginPlay
python 05_Query.py slice --graph G0001 --node N0 --max-nodes 40 --max-chars 16000
python 05_Query.py node --graph G0001 --node N0
python 05_Query.py node --graph G0001 --node N0 --evidence --max-chars 32000
```

`outline` 只读机器清单，列出图及候选入口；候选入口仍可能禁用，不保证实际执行。`find` 在选定图或所有图的事实中搜索，只返回匹配节点摘要，不把原文件回填上下文。`--graph` 接受图编号、完整路径或唯一图名，同名图必须进一步消歧。

`slice` 从指定节点沿执行边取可达节点，再收集上游数据依赖；默认输出带节点标签的伪代码和精确 pin 索引连接，不重复输出完整节点 JSON。循环、共享目标和扇出保留。只作为数据来源纳入的节点标记 `data_dependency`，其中带执行 pin 的节点附加 `requires_prior_execution`，不能据此认定它由当前入口调用，也不继续追踪它的其他执行出口。外部宏、函数和原生节点实现仍未展开。`node --evidence` 才返回单节点原始属性、引脚及相邻边。

默认最多 40 个节点/结果、最终 JSON（含换行）最多 16,000 个 Unicode 字符，字符数不是 token 数或 UTF-8 字节数。截断结果附有 `truncated`、剩余节点/结果数量；子图另外报告跨边界连接总数、最多 8 条样例和省略数。先看结果是否足够回答；不足时沿引用的节点继续查询或缩小问题范围，必要时显式提高预算。`truncated=false` 只表示本次范围内没有预算省略，不代表外部实现已包含或运行时顺序已证明。

`01_Manifest.json` 为每张图记录完整图路径、入口列表、节点数及伪代码/证据文件的 SHA1。查询器仅校验所选图，拒绝内容摘要或图身份不符、路径越界及节点覆盖不一致的文件。摘要用于检测快照混用，不是安全签名；图编号和 `snapshot_id` 也不替代持久对象身份。没有 Python 时仍可按图阅读。

## 阅读边界

函数入口用 `local` 声明局部变量，始终保留原始 `default`。`default_source=explicit` 表示显式序列化值，`type_default` 表示原始值为空、使用类型初始化；非容器数值和布尔类型另给出 `effective_default`（0 或 false）。例如 `local "Val": "real:double" = type_default(0) [raw_default=""]`。结构体、容器等复杂类型保留 `type_default [see_evidence]`，不凭空补零。这些是初始化语义，不代表变量在后续执行中的值。

本图未展开的调用可能已在其他资产目录导出。运行 `python 05_Query.py deps --graph G0029 --node N16`（图和节点按实际清单替换）获取被调图的入口、伪代码和证据相对路径；省略 `--node` 列出该图调用。`node`/`slice` 对调用节点给出此查询提示，`deps` 本身不返回被调实现正文。它只扫描当前资产和同级目录的清单，按完整资产及图路径匹配，优先完整对象路径 SHA1 的规范目录；目标库后来导出也可直接发现，无须重新导出调用方。

查询结果明确区分 `ok`、`native_implementation`、`unresolved`、`not_exported`、`graph_not_exported`、`ambiguous` 和 `invalid_export`；重复历史副本不能唯一定位或文件校验失败时不会猜选。仍受 `--max-nodes`/`--max-chars` 预算限制，发现路径不等于已读取实现。新版 v2 清单通过 `features` 声明 `local_initialization` 与 `dependency_navigation`，旧包需重新导出才能获得新内容。

伪代码是确定性阅读表示，不是可执行程序。`expr` 和 `read` 按需取值，不假定缓存；执行顺序以 `exit`/callback 引用为准。环、共享目标、数据扇出和多入口均保留。未知或特殊节点标记为 `opaque`/`see_evidence`，不能据名称补造实现。

伪代码保留已连接来源，不用默认值覆盖连线语义；空默认值仍以 serialized 形式保留。外部函数、父类、自定义 K2Node 和外部宏的实现不自动展开；本地导出图集合内的宏才可标记为已包含。完整图 JSON 是逐图事实证据，资产 JSON 是完整资产元数据且不含 `graphs`。

`id`、节点别名和 pin `index` 只在本次快照内定位；陈述引脚来源时同时核对 pin 名称与类型。`90_Full` 中的三个文件保留原有完整 JSON、AI 文本和 Markdown 输出，原 `AIWriter` 不变；新的 `XBlueprintReadPack::Build` 只消费 JSON 生成按需阅读包。

图编号按本次快照分配，不用于跨导出比对；持久定位结合 `asset_path`、图 `path`、`node_guid` 与 pin `id`，无效或重复 GUID 不保证稳定。同一节点同名 pin 在参数声明、数据输出、执行出口及引用中统一附加 `#index` 消歧，包括跨输入/输出方向重名。数据重路由在表达式引用中折叠，但节点仍保留；超过 64 级的重路由保留余下节点引用并提示查证据，不递归展开任意长链。

## 详细事实格式

逐图证据与 `90_Full/<蓝图名>.json` 中对应图对象完全相同；`00_Asset.json` 与完整快照去掉 `graphs` 后相同。完整 JSON 为 schema 1.1，保留原有字段并补充以下信息：

| 位置 | 字段与含义 |
| --- | --- |
| pin.type | 引用、const、弱引用、UObject wrapper、单精度序列化标志、成员引用；Map 的 `value_type_details` |
| pin | `index`、父子 pin GUID 与索引、`orphaned`、默认值忽略/只读、不可连接标志；空 `default` 也保留 |
| node | `class_path`、`semantic_status`、`reflected_properties` |
| graph | `graph_guid`、`unclassified_node_ids`、`truncated`、完整节点/边和静态可达信息 |
| root.coverage | 所属资产图范围、属性提取范围、外部实现未包含、非穷尽语义及外部宏路径 |
| macro semantic | `definition_status` 为 `included`、`external_not_included` 或 `unresolved` |

`classified` 只表示匹配已有提取分支，不保证派生类展开行为完整。`reflected_properties` 保存节点派生类的非 transient、非废弃且可序列化的反射属性，包括固定长度 C++ 数组各元素；值是 UE 文本，不是可执行代码，也不覆盖非反射的原生状态。

完整 `.ai.md` 仍由同一详细 JSON 快照生成，节点/边按行 JSONL 编码；省略画布坐标、重复图内邻接与可达链，图外引用以 `external_links` 保留。详细 JSON 的 `exec_chain` 是静态可达信息，不是运行时顺序；旧 `.md` 仍有摘要限制。这两份都位于 `90_Full`，不作为按需包的初始上下文。

## 写入与历史文件

目录后缀使用完整对象路径的 40 位 SHA1，避免 `/Game/A_B/C/D` 与 `/Game/A/B_C/D` 经扁平化后互相覆盖。覆盖已有目录前核对 `20_Evidence/00_Asset.json` 的 `asset_path`，身份不符或非空目录缺少可验证身份时拒绝写入。多个蓝图陆续导出时，总入口按资产身份去重并优先链接新版目录；旧版扁平目录留作历史备份，不因导出另一个资产而重新进入索引。

输出按文件组暂存、备份后替换，资产入口和总目录入口最后替换；普通写入失败保留或尝试回滚，恢复失败给出保留备份路径。该流程不承诺进程崩溃或断电时的文件系统级原子提交。旧根目录的三个格式文件只有在旧 JSON 能确认由 XTools 生成且资产路径匹配时，才在新索引成功后清理；未进入新索引的历史图文件忽略。

## 版本、测试与验证

导出核心使用 UE 5.3 起已有的 C++ 图对象和反射接口，设计覆盖 UE 5.3–5.8，不依赖 5.8 MCP 或新增蓝图脚本接口。自动化前缀为 `XTools.AssetEditor.BlueprintGraphExporter`，使用 `Scripts/Test-UE53.ps1 -Tests XTools.AssetEditor.BlueprintGraphExporter` 运行。

UE 5.3.2 Editor Development 编译及 9 项自动化全部通过、无警告。ReadPack 测试覆盖局部变量初始化、索引/名字不连续和重复 GUID 的精确数据来源、已连接与忽略默认值、共享回调、两类异步节点、执行环、作者文本转义、未知/禁用节点、同名 pin 消歧、清单内容摘要及只读输出；文件测试覆盖写入回滚、碰撞路径隔离、身份不符拒绝和交替导出时根索引去重。另有 21 项 Python 查询/语义回归全部通过，包括跨库函数/宏导航、后续导出发现、规范目录优先、重复副本歧义及损坏证据拒绝，运行 `python -B -m unittest discover -s Scripts -p "test_blueprint_*.py"`。UE 5.4–5.8 尚未执行此次改动的构建验证。

### 现有项目资产验证

开发构建提供 `XTools.BlueprintExport.ValidateAssets /Game/Folder/BP_Name ...`，加载指定资产、调用相同导出后端，并独立记录 UE 图对象的节点、引脚、连接、导出前后内存状态及 package dirty 标志，不编译或保存源资产。清单位于 `Saved/XTools/BlueprintExportValidation/source-inventory.json`。

无界面运行使用 `-ExecCmds="XTools.BlueprintExport.ValidateAssets /Game/Folder/BP_Name" -XToolsBlueprintValidationExit -unattended -nop4 -nosplash -NullRHI`。随后运行：

```text
python Scripts/validate_blueprint_ai_export.py <source-inventory.json> --report <validation.json>
```

校验器同时支持旧单层目录及 ReadPack v1/v2，比较独立源图、详细 JSON、完整 AI JSONL，以及逐图/资产证据。除节点覆盖外，还从完整图推导并校验伪代码的操作目标、启用状态、参数来源/默认值、执行出口、回调和数据输出；v2 同时检查机器清单、图身份及内容摘要。该检查针对导出语法和图事实，不验证 UE 运行时行为。

2026-09-12 使用 v2 重新导出 18 个现有蓝图，对照 44 张图、1422 个节点、4554 个 pin、1510 条边全部通过；387 个 `.uasset/.umap` 前后 SHA256 无变化。总入口仅有这 18 个资产的新版链接。44 张图均通过导出查询器校验，93 次离线查询检查通过；直接修改 MasterField 伪代码的执行目标或默认值，绕过摘要校验后仍被语义检查拒绝。

v2 总入口 2,878 字节、18 个资产入口合计 65,018 字节、44 份伪代码合计 251,122 字节；原完整 AI 文本合计 8,195,668 字节。这些主要阅读文本合计约为完整 AI 文本的 3.89%，来自将元数据和精确属性移到按需证据，不是无损压缩率或 token 成本测量，完整目录的磁盘体积反而增加。

MasterField EventGraph 有 290 个节点，完整图伪代码为 42,718 UTF-8 字节。按 `CE_Trigger`、Tick、BeginPlay 三个入口使用默认预算，首批分别返回 40 个节点、11,413 / 10,619 / 11,774 字节，仍有 186 / 200 / 198 个相关节点未返回；三个结果均明确标为截断，不可将首批当成完整入口逻辑。此轮报告、源文件检查和查询样本位于宿主项目 `Saved/XTools/BlueprintExportValidation/readpack-v2/`。

### 理解评测边界

2026-09-13 针对迁入函数库暴露的局部默认值及跨库导航问题修复后，重新导出 17 个库（含 MengAdvancedLib），245 张图、5402 节点、16995 引脚、6986 连接全部通过独立源图对照；501 个局部变量声明均接受初值来源校验。753 次实际导出包查询通过，包含跨资产导航；默认预算下 36 个 slice 和 1 个 deps 结果显式截断。Content 中 5557 个 `.uasset/.umap` 的前后 SHA256 一致。记录位于宿主项目 `Saved/XTools/BlueprintExportValidation/logic-fix-20260913/`。本轮不处理已知 WorldContext 断言，也不将结构校验等同于运行时验证。

该修复的独立上下文复测中，读取者只从导出入口开始，沿 `deps` 找到 `MengSimpleLib/G0045`，读取两张伪代码及必要节点证据，首次回答即确认两个累计变量初值为 0，并还原权重抽取的随机阈值、累计比较和返回当前键。未读取 `90_Full`、源码及历史答案。记录见上述目录 `independent-readback.md` 和 `repair-report.md`，仅作为这一缺陷案例的理解回归。

此前完整 `.ai.md` 的单轮理解诊断中，新版小样本 11/12 题完整正确、旧浏览摘要 4/12 题完整正确，MasterField 的 6 题全部正确；一次 `Value_5/Value_6` 来源误读在程序化核验后自行纠正，原成绩未覆盖。该评测针对此前完整格式，不能用作此次按需阅读包的理解成绩。运行时正确性、通用准确率和其他 UE 版本能力须分别验证。

本轮另给独立上下文代理总目录与 6 个问题。代理按入口读取 4 个资产索引、5 张图，仅为精确引脚信息查询 1 份图证据，未读取 `90_Full`；初次 5 题事实与证据完整，另 1 题宏结论正确但只引用通用边界说明，证据不足。据此在资产入口加入实际外部宏路径列表，定向复核后确认具体证据，原答卷保持不变。完整记录位于宿主项目 `Saved/XTools/BlueprintExportValidation/readpack-report.md`，不是通用模型准确率测试。
