# 资产菜单图标

本组图标用于内容浏览器右键菜单，由 `XTools.AssetEditor.Menu` 样式注册。

| 文件 | 样式键 | 语义 |
| --- | --- | --- |
| FlattenAssets.svg | Assets.Flatten | 多个资产汇入同一文件夹 |
| OrganizeAssets.svg | Assets.OrganizeByType | 资产分流到分类文件夹 |
| NormalizeAssetNames.svg | Assets.NormalizeNames | 自动规范化资产名前缀 A_ |

遵循本地 UE 5.3 菜单及插件现有 Slate 实现：16×16 逻辑尺寸，透明背景、白色单色 SVG，保留约 1 像素边距；线条不小于 1.25 像素。使用 FSlateVectorImageBrush 随 DPI 栅格化，颜色与禁用态交由菜单的 Slate 前景色处理。避免位图放大、渐变、嵌入字体、外部资源和过细装饰。

注册位于内容浏览器菜单扩展初始化，样式在菜单扩展注销后释放。Resources 随插件分发，不使用宿主项目绝对路径。
