# 蓝图 AI 逻辑导出

在内容浏览器选中蓝图，使用“导出蓝图逻辑流”。每个资产写入 `Saved/XTools/BlueprintExports/<资产名>_<完整对象路径SHA1>/`。目录结构如下：

完成弹窗提供“打开目录”“复制绝对路径”“复制 AI 提示词”和“关闭”。“复制 AI 提示词”复制简短阅读说明及 `00_START_HERE.md` 的绝对路径；单个成功资产指向资产入口，多个成功资产指向总入口并列明本次成功目录，避免混入历史包。全部失败或取消且没有成功输出时，复制按钮不可用。

```text
00_START_HERE.md                 # 总目录：选择相关资产目录
<资产>/
├── 00_START_HERE.md             # 资产入口、图索引、外部宏依赖和停止条件
├── 01_Manifest.json             # v2 机器清单：图身份、候选入口与内容摘要
├── 02_ReadingContract.md        # 通用阅读边界与伪代码语法；相同摘要只读一次
├── 05_Query.py                  # Python 标准库只读查询器
├── 10_Logic/G0001.pseudo.md     # 按图生成、带标签的确定性伪代码
├── 20_Evidence/00_Asset.json    # 资产元数据，不含图与宏定义正文
├── 20_Evidence/G0001.json       # 对应图的原完整 JSON
├── 30_Dependencies/M0001.*      # 按需标准宏定义：.pseudo.md 与 .json
└── 90_Full/                     # 原完整 JSON、.ai.md、.md 三种格式
```

默认按问题选择相关内容，避免无目的全量读取；全局审计或证据不足时，可扩大范围、直接读取完整文件或提高查询预算。先读总目录和资产入口；首次使用读取 `02_ReadingContract.md`，本次上下文已读相同 SHA1 的契约可跳过。契约副本随每个资产目录分发，移动单个目录后仍可独立使用；这是阅读去重，不是磁盘去重。按问题选择图的 `10_Logic/Gxxxx.pseudo.md`，证据足够时即可停止。需要精确类型、pin 属性或未知节点细节时，查询同编号的 `20_Evidence/Gxxxx.json`；变量、组件、时间轴或类信息不足时，查询 `20_Evidence/00_Asset.json` 的对应键。`90_Full/` 可供全局审计、工具解析或完整查证，不作为默认初始上下文。阅读路径是建议，文件身份、协议兼容与遗漏范围仍严格校验。

## 大图按入口或节点读取

在资产目录运行以下命令，需要 Python 3.9+，无需安装依赖或启动 UE。先取得本次导出的图/节点编号，再按问题选择：

```text
python 05_Query.py outline
python 05_Query.py outline --include-macros
python 05_Query.py find --query BeginPlay
python 05_Query.py slice --graph G0001 --node N0 --max-nodes 40 --max-chars 16000
python 05_Query.py node --graph G0001 --node N0
python 05_Query.py node --graph G0001 --node N0 --evidence --max-chars 32000
python 05_Query.py assets --graph G0001 --node N0
```

`outline` 从机器清单列出图及候选入口，不读取图正文；候选入口仍可能禁用，不保证实际执行。默认只列所属图，并给出 `macro_definition_count`；加 `--include-macros` 同时列出 M 编号宏定义索引，仍受结果数和字符预算限制。图头的 `logic_bytes`/`evidence_bytes` 为 UTF-8 文件字节量，资产入口也显示这两个数。`find` 在选定图或所有所属图的事实中搜索，只返回匹配节点摘要，不把原文件回填上下文。未指定 `--graph` 时不读取标准宏定义正文；显式选择 `M0001` 才读取对应定义。`--graph` 接受图编号、完整路径或唯一图名，同名图必须进一步消歧。

`slice` 从指定节点沿执行边取可达节点，再收集上游数据依赖；默认输出带节点标签的伪代码和精确 pin 索引连接，不重复输出完整节点 JSON。循环、共享目标和扇出保留。只作为数据来源纳入的节点标记 `data_dependency`，其中带执行 pin 的节点附加 `requires_prior_execution`，不能据此认定它由当前入口调用，也不继续追踪它的其他执行出口。外部宏、函数和原生节点实现仍未展开。`node --evidence` 才返回单节点原始属性、引脚及相邻边。

默认最多 40 个节点/结果、最终 JSON（含换行）最多 16,000 个 Unicode 字符，字符数不是 token 数或 UTF-8 字节数。截断结果附有 `truncated`、剩余节点/结果数量；子图另外报告跨边界连接总数、最多 8 条样例和省略数。先看结果是否足够回答；不足时沿引用的节点继续查询或缩小问题范围，必要时显式提高预算。`truncated=false` 只表示本次范围内没有预算省略，不代表外部实现已包含或运行时顺序已证明。

列表查询最多保留 `max_nodes` 条候选结果，继续扫描以精确计算剩余数量，再按最终字符预算裁剪；`assets` 逐引脚写入同一收集器，不先构造单节点完整引用列表。图证据仍整图解析，因此这不是整个查询的恒定内存保证。`deps` 在单次调用内复用已读取的当前清单与图校验结果，缓存按完整目录、图身份、文件路径、摘要和节点数隔离，仅保存成功/失败，不囤积图正文；下一次调用重新读取和校验。预算、命中顺序、文件损坏提示及阅读自由保持原有含义。

`01_Manifest.json` 为每张图记录完整图路径、入口列表、节点数及伪代码/证据文件的 SHA1。查询器仅校验所选图，拒绝内容摘要或图身份不符、路径越界及节点覆盖不一致的文件。摘要用于检测快照混用，不是安全签名；图编号和 `snapshot_id` 也不替代持久对象身份。没有 Python 时仍可按图阅读。

清单另外声明 `pseudo_format_version=1`、`query={path, protocol_version, sha1}`、`reading_contract={path, version, sha1}`。查询器启动时检查支持的协议版本和契约摘要；执行本包声明的脚本时也检查自身摘要。显式使用另一份兼容查询器读取旧包是允许的，不要求它与旧副本逐字节相同。旧 v2 包缺少新增字段时仍可读取，新协议不支持时明确拒绝。插件资源是查询器的维护来源，随包副本固定在导出时刻；重新导出更新整包，不依赖插件安装路径或环境变量启动器。文件摘要不提供访问控制，直接读文件也不受 CLI 的输出预算约束。

阅读契约与查询器均来自插件 `Resources/BlueprintExport/`，导出时读取并随包保存。纯文案变更可修改 `02_ReadingContract.md` 后重新导出，无需重新编译；语法/协议变化仍须同步代码版本与测试。契约缺失、不可读或为空时拒绝导出并保留原包。`.gitattributes` 仅将该资源目录的 `.py`/`.md` 固定为 LF，减少不同 Git checkout 的字节摘要噪音；SHA1 标识文件字节，兼容性由协议版本决定。UE 5.3 的插件打包器默认包含 `Resources/...`，无需额外安装路径依赖。

`02_ReadingContract.md` 把伪代码块语法作为 v1 契约：每图一个 `text` 代码块，顶格节点头、已知启用状态前缀、缩进延续行和独立注释头。文本内的作者换行、引号及反引号经 JSON 转义，不能伪装为新节点。查询器只解析块边界，操作、参数与连接继续由语义校验器核对。改变块边界或节点头语法需提升伪代码协议版本；重新排版必须同步生成摘要。独立语法测试覆盖 CRLF、转义作者文本、重复节点、未知前缀、未缩进延续行及缺失/重复/未闭合代码块。

## 阅读边界

有分类但没有专用展示模板的节点使用 `classified "kind" "title"`，后接 `semantic:` JSON 行，完整保留该节点已采集的语义字段；参数、数据输出和执行出口继续独立列出。只有没有分类的节点才走 `opaque` 兜底；分类不代表实现已展开或编译器语义完备。清单用 `classified_fallback` 声明这种展示能力，校验器核对种类、标题及完整语义对象，旧包仍按旧规则校验。节点头和缩进延续行语法没有改变，`pseudo_format_version` 保持 1。通用兜底自动适用后续新增分类，不另外维护模板白名单。

`assets` 查询有类型依据、未连接且有效的输入引脚默认资产引用；支持对象、类、软对象、软类及插件挂载路径。默认查询所属图，显式 `--graph Mxxxx` 可查询标准宏定义；`--node` 必须同时指定图。输出包含图、节点、引脚位置、引用类型和 `reference_only` 状态，沿用结果数/字符预算。它不加载资产，也不将普通字符串、容器文本或任意反射属性猜成依赖；返回范围及未解析/未支持候选计数。`complete_asset_graph=false` 明确表示这不是完整资产依赖图，空结果不能证明没有依赖。

`deps` 仍是函数/宏的正向调用导航，不提供项目级反向影响分析；`assets` 补充另一种证据查询，不改变已有调用定位和规范目录优先规则。不支持的协议继续拒绝读取，错误结果同时给出 `field`、`received` 和 `supported`，帮助选择兼容查询器。

函数入口用 `local` 声明局部变量，始终保留原始 `default`。`default_source=explicit` 表示显式序列化值，`type_default` 表示原始值为空、使用类型初始化；非容器数值和布尔类型另给出 `effective_default`（0 或 false）。例如 `local "Val": "real:double" = type_default(0) [raw_default=""]`。结构体、容器等复杂类型保留 `type_default [see_evidence]`，不凭空补零。这些是初始化语义，不代表变量在后续执行中的值。

本图未展开的调用可能已在其他资产目录导出。运行 `python 05_Query.py deps --graph G0029 --node N16`（图和节点按实际清单替换）获取被调图的入口、伪代码和证据相对路径；省略 `--node` 列出该图调用。`node`/`slice` 对调用节点给出此查询提示，`deps` 本身不返回被调实现正文。它只扫描当前资产和同级目录的清单，按完整资产及图路径匹配，优先完整对象路径 SHA1 的规范目录；目标库后来导出也可直接发现，无须重新导出调用方。

查询结果明确区分 `ok`、`native_implementation`、`unresolved`、`not_exported`、`graph_not_exported`、`ambiguous` 和 `invalid_export`；重复历史副本不能唯一定位或文件校验失败时不会猜选。仍受 `--max-nodes`/`--max-chars` 预算限制，发现路径不等于已读取实现。新版 v2 清单通过 `features` 声明 `local_initialization`、`dependency_navigation`、`semantic_hints` 和 `standard_macro_definitions`，旧包需重新导出才能获得新内容。

伪代码是确定性阅读表示，不是可执行程序。`expr` 和 `read` 按需取值，不声明运行时缓存策略；`demand: data_edges=2 consumers=1 [static_direct; not_call_count]` 表示两条直接数据边进入同一个目标节点，不等于两次求值。计数保留静态未执行路径和重路由目标，不作为性能估算。UE 5.3 编译器会按非纯消费者收集纯节点依赖，同一个消费者内会去重；循环、分支和编译后的图仍会影响实际求值。直接内联或添加一次性缓存赋值可能改变读者对求值时机的理解，因此保留节点引用和拓扑。环、共享目标、数据扇出和多入口均保留；未知或特殊节点标记为 `opaque`/`see_evidence`，不能据名称补造实现。

变量的 `binding_origin` 区分 `local`、`self_member`、`external_member` 与 `unresolved`。`component_read` 仅用于反射解析到组件对象属性的读取；`component_binding=object_property` 不表示 SCS 声明，只有通过属性所属蓝图的 SCS 名称及有效 GUID 核对后才标记 `scs_property`，并附 `scs_node_path`、模板及声明 GUID。成员类型本身不证明它是 SCS 组件；声明绑定也不保证运行时指针有效或未被重新赋值。同名局部变量保持局部身份。`function_entry` 的 `local_scope` 给出完整所属图路径，与已有 `local` 初始声明配合，不推断运行时生命周期。

### 标准宏定义的按需读取

导出器收集当前本地引擎 `/Engine/EditorBlueprintResources/StandardMacros.StandardMacros` 中实际引用的宏图及其标准宏依赖，按完整图路径去重，保存到 `30_Dependencies/Mxxxx.pseudo.md` 与同编号 JSON。`01_Manifest.json.macro_definitions` 与所属资产的 `graphs` 分开，原图及节点计数不膨胀。每包最多 64 张定义图、10000 个定义节点，按整图采集；达到上限时 `coverage.macro_dependency_limit_reached=true`，未采集引用继续明确标记，`macro_definition_engine_version` 记录来源引擎版本。

`deps` 优先定位当前包声明的宏定义并验证文件摘要；正文仍需显式 `node/slice --graph Mxxxx` 读取。已声明文件损坏时返回 `invalid_export`，不会悄悄选另一个引擎版本的副本。返回的 `asset_path` 仍是引擎宏资产，`export_owner_asset_path` 表明它保存在当前哪个导出包中。

这不是宏展开或按名称降级。调用节点与定义使用各自的节点编号，按隧道 pin 和类型对应；泛型定义的具体类型需要结合调用实例引脚。不同 Gate/DoOnce 调用保留各自实例，不能因为共用一份定义就合并状态。`tunnel_entry/exit` 表示定义边界，`assign [Variable_is_write_target]` 表示把 `Value` 写入 `Variable` 引用的网络，原始参数、默认值和执行出口均保留。`temp` 是编译器局部临时变量，记录声明类型和 `persistent_savegame` 标记；实际引脚类型另在证据中保留，不据此推断每次调用初始化或完整生命周期。其他引擎原生行为、非标准宏和未支持的节点仍保持证据边界。

伪代码保留已连接来源，不用默认值覆盖连线语义；空默认值仍以 serialized 形式保留。外部函数、父类、自定义 K2Node 和非标准外部宏的实现不自动展开；实际包含的宏由清单与 `definition_status` 标识。完整图 JSON 是逐图事实证据，资产 JSON 排除 `graphs` 与 `macro_definitions` 正文。

`id`、节点别名和 pin `index` 只在本次快照内定位；陈述引脚来源时同时核对 pin 名称与类型。`90_Full` 中的三个文件保留原有完整 JSON、AI 文本和 Markdown 输出，原 `AIWriter` 不变；新的 `XBlueprintReadPack::Build` 只消费 JSON 生成按需阅读包。

图编号按本次快照分配，不用于跨导出比对；持久定位结合 `asset_path`、图 `path`、`node_guid` 与 pin `id`，无效或重复 GUID 不保证稳定。同一节点同名 pin 在参数声明、数据输出、执行出口及引用中统一附加 `#index` 消歧，包括跨输入/输出方向重名。数据重路由在表达式引用中折叠，但节点仍保留；访问达到 64 个来源引用时保留当前引用并提示 `continue_in_evidence`，遇环提示 `data_cycle`。所有原节点/边仍在证据中，不递归展开任意长链。引脚重名计数按节点预先建立，避免每次生成标签重复扫描。

## 详细事实格式

逐图证据与 `90_Full/<蓝图名>.json` 中对应图对象完全相同；`00_Asset.json` 与完整快照去掉 `graphs` 和 `macro_definitions` 后相同。完整 JSON 为 schema 1.1，保留原有字段并补充以下信息：

| 位置 | 字段与含义 |
| --- | --- |
| pin.type | 引用、const、弱引用、UObject wrapper、单精度序列化标志、成员引用；Map 的 `value_type_details` |
| pin | `index`、父子 pin GUID 与索引、`orphaned`、默认值忽略/只读、不可连接标志；空 `default` 也保留 |
| node | `class_path`、`semantic_status`、`reflected_properties` |
| graph | `graph_guid`、`unclassified_node_ids`、`truncated`、完整节点/边和静态可达信息 |
| root.coverage | 所属资产图范围、属性提取范围、非穷尽语义、未包含宏路径、标准宏来源引擎版本及采集上限；`external_implementations_included=false` 表示未承诺所有外部实现 |
| root.macro_definitions | 实际采集的标准宏图对象，与资产自身 graphs 分开 |
| macro semantic | `definition_status` 为 `included`、`dependency_included`、`external_not_included` 或 `unresolved` |
| variable semantic | 成员作用域、反射属性路径、组件属性与 SCS 声明证据 |
| function_entry semantic | `local_scope` 与局部变量初值声明 |
| assignment / temporary_variable semantic | 写目标/值引脚、临时变量声明类型/输出引脚和持久标记 |

`classified` 只表示匹配已有提取分支，不保证派生类展开行为完整。`reflected_properties` 保存节点派生类的非 transient、非废弃且可序列化的反射属性，包括固定长度 C++ 数组各元素；值是 UE 文本，不是可执行代码，也不覆盖非反射的原生状态。

完整 `.ai.md` 仍由同一详细 JSON 快照生成，节点/边按行 JSONL 编码；省略画布坐标、重复图内邻接与可达链，图外引用以 `external_links` 保留。详细 JSON 的 `exec_chain` 是静态可达信息，不是运行时顺序；旧 `.md` 仍有摘要限制。这两份都位于 `90_Full`，不作为按需包的初始上下文。

## 写入与历史文件

目录后缀使用完整对象路径的 40 位 SHA1，避免 `/Game/A_B/C/D` 与 `/Game/A/B_C/D` 经扁平化后互相覆盖。覆盖已有目录前核对 `20_Evidence/00_Asset.json` 的 `asset_path`，身份不符或非空目录缺少可验证身份时拒绝写入。多个蓝图陆续导出时，总入口按资产身份去重并优先链接新版目录；旧版扁平目录留作历史备份，不因导出另一个资产而重新进入索引。

输出按文件组暂存、备份后替换，资产入口和总目录入口最后替换；普通写入失败保留或尝试回滚，恢复失败给出保留备份路径。该流程不承诺进程崩溃或断电时的文件系统级原子提交。旧根目录的三个格式文件只有在旧 JSON 能确认由 XTools 生成且资产路径匹配时，才在新索引成功后清理；未进入新索引的历史图文件忽略。

## 版本、测试与验证

导出核心使用 UE 5.3 起已有的 C++ 图对象和反射接口，设计覆盖 UE 5.3–5.8，不依赖 5.8 MCP 或新增蓝图脚本接口。自动化前缀为 `XTools.AssetEditor.BlueprintGraphExporter`，使用 `Scripts/Test-UE53.ps1 -Tests XTools.AssetEditor.BlueprintGraphExporter` 运行。

2026-09-14 O4/O5 性能优化后最新验证为 15 项 UE 自动化、41 项 Python 回归全部通过。源图序列化按所属节点缓存 pin 索引，包含图外节点链接；单次资产导出的 JSON 与旧 Markdown 共享图枚举/排序结果，缓存均不跨导出保留。18 个实际资产重新导出通过源图对照，305 份伪代码及 18 份旧 Markdown 与冻结副本逐字节一致；15 份图证据的差异仅涉及 34 个未连接 PromotableOperator.ErrorTolerance 的 pin GUID，其余字段、顺序、索引、连线和默认值一致。5557 个源资产文件哈希不变。

1256 对实际包查询的输出和退出码完全一致，另有 216 组命令/预算/损坏目标组合对照。deps 图校验次数 680→589；本机交替执行的单轮总查询耗时为 17.663→17.039 秒，不作为普遍加速承诺。证据见宿主 `Saved/XTools/BlueprintExportValidation/performance-review-20260914/report.md`，UE 报告为 `Saved/Automation/Reports/BlueprintPerformance-review-20260914-r3/`。以下为此前各阶段的验证记录。

UE 5.3.2 Editor Development 编译及 14 项自动化全部通过；组件临时蓝图测试触发 1 条 AssetRegistry `/Engine/Transient` 不存在的警告。ReadPack 测试覆盖局部变量初始化及作用域、组件属性与 SCS 绑定、同名局部变量隔离、标准宏去重/调用实例保留、赋值和临时变量、纯节点多边进入同一消费者，以及原有连接、回调、执行环、摘要和文件写入保护；新增全部 70 种当前分类及未来分类的渲染、数据重路由 64 级边界和环回归。另有 36 项 Python 查询/语义回归全部通过，包括本包标准宏及跨库导航、重复身份/损坏证据拒绝、旧包兼容、布尔序列化大小写及语义标记篡改检测，以及协议类型/版本、阅读契约和脚本摘要、宏索引预算与伪代码块语法、分类事实篡改和有类型依据的默认资产引用，运行 `python -B -m unittest discover -s Scripts -p "test_blueprint_*.py"`。UE 5.4–5.8 尚未执行此次改动的构建验证。

2026-09-14 实现审查修复后，18 个资产的 255 张所属图和 50 份宏定义通过源图对照；1249 次原有查询及 305 次资产引用查询通过。实际 191 个节点使用分类兜底并逐一核对语义对象，20 个未分类节点仍为 opaque。原 BP_子弹包的 G0002/N67 可被新版查询器定位到爆炸特效引用。305 份伪代码总字节数由 1,718,095 增至 1,808,352，默认 slice 截断由 44 增至 45，均明确标记。5557 个源资产文件哈希不变。报告位于宿主项目 `Saved/XTools/BlueprintExportValidation/implementation-review-20260914/review-report.md`；这不是完整依赖图、运行时验证或模型准确率评测。

### 现有项目资产验证

2026-09-14 阅读建议与提示词按钮优化后，12 项 UE 自动化通过，新增单资产/多资产/无成功输出及中文空格路径的提示词用例。同一组 18 个资产重新导出，255 张所属图和 50 份标准宏定义通过源图对照；18 次随包 outline 查询通过，两个资源副本与模板字节及清单摘要一致，均为 LF。5557 个源资产文件哈希未变。证据位于宿主项目 `Saved/XTools/BlueprintExportValidation/prompt-review-20260914/`，编译/自动化报告为 `Saved/Automation/Reports/BlueprintPrompt-20260914/`。按钮已编译并接入现有剪贴板 API，尚未进行桌面弹窗点击与剪贴板粘贴的人工验收。

2026-09-14 DeepSeek 审查优化后，同一组 18 个资产重新导出通过：255 张所属图及 50 份标准宏定义通过源图对照，1249 次实际包查询及 21 次新增协议/宏索引查询通过。305 份图伪代码的 SHA1 与改动前一致；跨次加载的源图清单中 34 个 pin ID 不同（导出前后清单各记录一次，共 68 处），同时观测到 15 份证据摘要变化。旧证据正文未另行备份，不能穷尽归因摘要差异，也不能宣称跨进程完整快照逐字节稳定。本次每个资产导出前后内存状态/dirty 一致，5557 个源资产文件 SHA256 无变化。

18 个资产入口由 155616 字节降至 68076 字节，通用契约为 3438 字节；若本次上下文跨资产只读一次相同契约，入口加契约合计 71514 字节，比旧入口合计减少 54.04%。这是这组样本的导航阅读字节量，不是 token 测量或图逻辑压缩率；单个小资产首次读取入口加契约可能增加，完整包仍保留副本。证据见宿主项目 `Saved/XTools/BlueprintExportValidation/deepseek-review-20260914/`；编译/自动化报告为 `Saved/Automation/Reports/BlueprintDeepSeek-review-20260914/`。

开发构建提供 `XTools.BlueprintExport.ValidateAssets /Game/Folder/BP_Name ...`，加载指定资产、调用相同导出后端，并独立记录 UE 图对象的节点、引脚、连接、导出前后内存状态及 package dirty 标志，不编译或保存源资产。清单位于 `Saved/XTools/BlueprintExportValidation/source-inventory.json`。

无界面运行使用 `-ExecCmds="XTools.BlueprintExport.ValidateAssets /Game/Folder/BP_Name" -XToolsBlueprintValidationExit -unattended -nop4 -nosplash -NullRHI`。随后运行：

```text
python Scripts/validate_blueprint_ai_export.py <source-inventory.json> --report <validation.json>
```

校验器同时支持旧单层目录及 ReadPack v1/v2，比较独立源图、详细 JSON、完整 AI JSONL，以及逐图/资产证据。除节点覆盖外，还从完整图推导并校验伪代码的操作目标、启用状态、参数来源/默认值、执行出口、回调和数据输出；v2 同时检查机器清单、图身份及内容摘要。该检查针对导出语法和图事实，不验证 UE 运行时行为。

2026-09-14 IR 审查修复后，重新导出 17 个迁入库及 MasterField：255 张所属图、6525 节点、20609 引脚、8224 连接通过；50 份按包去重的标准宏定义（12 种完整宏路径，637 节点、1678 引脚、851 连接）也通过独立引擎源图对照，导出前后内存图和 dirty 状态一致。1249 次实际包查询通过，覆盖 264 次标准宏导航、505 个局部声明及 99 处 SCS 绑定；默认预算下 44 个 slice、4 个 deps 显式截断。Content 的 5557 个源资产文件前后 SHA256 一致。证据位于宿主项目 `Saved/XTools/BlueprintExportValidation/ir-review-20260914/`，编译与自动化报告为 `Saved/Automation/Reports/BlueprintIR-review-20260914-r6/`。已知 WorldContext 断言不在本轮处理范围。

该轮独立上下文读取者从两个资产入口出发，通过 `deps` 找到 MengAdvancedLib 的 `M0004`，还原 IsValid 原生判断调用和两个隧道出口；从 MasterField `G0002/N87` 识别 PlaneVolume 的 SCS 声明，并正确解释 `demand` 不等于求值次数。模型未直接读取原始 JSON、源码或 `90_Full`，查询器内部仍按需读取清单/证据进行校验。这是三个定向问题的理解回归，不是通用准确率或运行时验证。

2026-09-12 使用 v2 重新导出 18 个现有蓝图，对照 44 张图、1422 个节点、4554 个 pin、1510 条边全部通过；387 个 `.uasset/.umap` 前后 SHA256 无变化。总入口仅有这 18 个资产的新版链接。44 张图均通过导出查询器校验，93 次离线查询检查通过；直接修改 MasterField 伪代码的执行目标或默认值，绕过摘要校验后仍被语义检查拒绝。

v2 总入口 2,878 字节、18 个资产入口合计 65,018 字节、44 份伪代码合计 251,122 字节；原完整 AI 文本合计 8,195,668 字节。这些主要阅读文本合计约为完整 AI 文本的 3.89%，来自将元数据和精确属性移到按需证据，不是无损压缩率或 token 成本测量，完整目录的磁盘体积反而增加。

MasterField EventGraph 有 290 个节点，完整图伪代码为 42,718 UTF-8 字节。按 `CE_Trigger`、Tick、BeginPlay 三个入口使用默认预算，首批分别返回 40 个节点、11,413 / 10,619 / 11,774 字节，仍有 186 / 200 / 198 个相关节点未返回；三个结果均明确标为截断，不可将首批当成完整入口逻辑。此轮报告、源文件检查和查询样本位于宿主项目 `Saved/XTools/BlueprintExportValidation/readpack-v2/`。

### 理解评测边界

2026-09-13 针对迁入函数库暴露的局部默认值及跨库导航问题修复后，重新导出 17 个库（含 MengAdvancedLib），245 张图、5402 节点、16995 引脚、6986 连接全部通过独立源图对照；501 个局部变量声明均接受初值来源校验。753 次实际导出包查询通过，包含跨资产导航；默认预算下 36 个 slice 和 1 个 deps 结果显式截断。Content 中 5557 个 `.uasset/.umap` 的前后 SHA256 一致。记录位于宿主项目 `Saved/XTools/BlueprintExportValidation/logic-fix-20260913/`。本轮不处理已知 WorldContext 断言，也不将结构校验等同于运行时验证。

该修复的独立上下文复测中，读取者只从导出入口开始，沿 `deps` 找到 `MengSimpleLib/G0045`，读取两张伪代码及必要节点证据，首次回答即确认两个累计变量初值为 0，并还原权重抽取的随机阈值、累计比较和返回当前键。未读取 `90_Full`、源码及历史答案。记录见上述目录 `independent-readback.md` 和 `repair-report.md`，仅作为这一缺陷案例的理解回归。

此前完整 `.ai.md` 的单轮理解诊断中，新版小样本 11/12 题完整正确、旧浏览摘要 4/12 题完整正确，MasterField 的 6 题全部正确；一次 `Value_5/Value_6` 来源误读在程序化核验后自行纠正，原成绩未覆盖。该评测针对此前完整格式，不能用作此次按需阅读包的理解成绩。运行时正确性、通用准确率和其他 UE 版本能力须分别验证。

本轮另给独立上下文代理总目录与 6 个问题。代理按入口读取 4 个资产索引、5 张图，仅为精确引脚信息查询 1 份图证据，未读取 `90_Full`；初次 5 题事实与证据完整，另 1 题宏结论正确但只引用通用边界说明，证据不足。据此在资产入口加入实际外部宏路径列表，定向复核后确认具体证据，原答卷保持不变。完整记录位于宿主项目 `Saved/XTools/BlueprintExportValidation/readpack-report.md`，不是通用模型准确率测试。
