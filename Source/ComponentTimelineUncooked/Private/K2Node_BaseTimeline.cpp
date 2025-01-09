// Copyright 2023 Tomasz Klin. All Rights Reserved.

/**
 * 基础时间轴节点实现文件
 * 提供了时间轴节点的基础功能实现
 */

#include "K2Node_BaseTimeline.h"
#include "Engine/TimelineTemplate.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "BlueprintBoundNodeSpawner.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveLinearColor.h"
#include "Curves/CurveVector.h"
#include "K2Node_Composite.h"
#include "K2Node_CallFunction.h"

/**
 * 构造函数
 */
UK2Node_BaseTimeline::UK2Node_BaseTimeline(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

/**
 * 在蓝图中添加新的时间轴
 * @param Blueprint - 目标蓝图
 * @param TimelineVarName - 时间轴变量名称
 * @return 新创建的时间轴模板
 */
UTimelineTemplate* UK2Node_BaseTimeline::AddNewTimeline(UBlueprint* Blueprint, const FName& TimelineVarName)
{
	// 首先查找是否已存在同名的时间轴
	UTimelineTemplate* Timeline = Blueprint->FindTimelineTemplateByVariableName(TimelineVarName);
	if (Timeline != nullptr)
	{
		UE_LOG(LogBlueprint, Log, TEXT("AddNewTimeline: 蓝图 '%s' 中已存在名为 '%s' 的时间轴"), *Blueprint->GetPathName(), *TimelineVarName.ToString());
		return nullptr;
	}
	else
	{
		Blueprint->Modify();
		check(nullptr != Blueprint->GeneratedClass);
		// 使用提供的名称构造新的图表
		const FName TimelineTemplateName = *UTimelineTemplate::TimelineVariableNameToTemplateName(TimelineVarName);
			Timeline = NewObject<UTimelineTemplate>(Blueprint->GeneratedClass, TimelineTemplateName, RF_Transactional);
			Blueprint->Timelines.Add(Timeline);

		// 为任何子蓝图调整变量名称
		FBlueprintEditorUtils::ValidateBlueprintChildVariables(Blueprint, TimelineVarName);

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		return Timeline;
	}
}

/**
 * 检查蓝图是否支持时间轴功能
 */
bool UK2Node_BaseTimeline::DoesSupportTimelines(const UBlueprint* Blueprint) const
{
	return FBlueprintEditorUtils::DoesSupportEventGraphs(Blueprint);
}

/**
 * 获取蓝图中所需的节点名称
 */
FName UK2Node_BaseTimeline::GetRequiredNodeInBlueprint() const
{
	return NAME_None;
}

/**
 * 节点粘贴后的处理
 * 确保时间轴名称唯一，并为该节点分配新的时间轴模板对象
 */
void UK2Node_BaseTimeline::PostPasteNode()
{
	UK2Node::PostPasteNode();

	UBlueprint* Blueprint = GetBlueprint();
	check(Blueprint);

	UTimelineTemplate* OldTimeline = NULL;

	// 查找具有相同UUID的模板
	for (TObjectIterator<UTimelineTemplate> It; It; ++It)
	{
		UTimelineTemplate* Template = *It;
		if (Template->TimelineGuid == TimelineGuid)
		{
			OldTimeline = Template;
			break;
		}
	}

	// 确保TimelineName是唯一的，并为此节点分配新的时间轴模板对象
	TimelineName = FBlueprintEditorUtils::FindUniqueTimelineName(Blueprint);

	if (!OldTimeline)
	{
		if (UTimelineTemplate* Template = AddNewTimeline(Blueprint, TimelineName))
		{
			bAutoPlay = Template->bAutoPlay;
			bLoop = Template->bLoop;
			bReplicated = Template->bReplicated;
			bIgnoreTimeDilation = Template->bIgnoreTimeDilation;
		}
	}
	else
	{
		check(NULL != Blueprint->GeneratedClass);
		Blueprint->Modify();
		const FName TimelineTemplateName = *UTimelineTemplate::TimelineVariableNameToTemplateName(TimelineName);
		UTimelineTemplate* Template = DuplicateObject<UTimelineTemplate>(OldTimeline, Blueprint->GeneratedClass, TimelineTemplateName);
		bAutoPlay = Template->bAutoPlay;
		bLoop = Template->bLoop;
		bReplicated = Template->bReplicated;
		bIgnoreTimeDilation = Template->bIgnoreTimeDilation;
		Template->SetFlags(RF_Transactional);
		Blueprint->Timelines.Add(Template);

		// 修复时间轴轨道以指向正确的位置。复制时，它们仍然关联到旧的蓝图，因为我们没有适当的作用域。注意，我们永远不想修复外部曲线资产引用
		{
			// 处理浮点数轨道
			for (auto TrackIt = Template->FloatTracks.CreateIterator(); TrackIt; ++TrackIt)
			{
				FTTFloatTrack& Track = *TrackIt;
				if (!Track.bIsExternalCurve && Track.CurveFloat && Track.CurveFloat->GetOuter()->IsA(UBlueprint::StaticClass()))
				{
					Track.CurveFloat->Rename(*Template->MakeUniqueCurveName(Track.CurveFloat, Track.CurveFloat->GetOuter()), Blueprint, REN_DontCreateRedirectors);
				}
			}

			// 处理事件轨道
			for (auto TrackIt = Template->EventTracks.CreateIterator(); TrackIt; ++TrackIt)
			{
				FTTEventTrack& Track = *TrackIt;
				if (!Track.bIsExternalCurve && Track.CurveKeys && Track.CurveKeys->GetOuter()->IsA(UBlueprint::StaticClass()))
				{
					Track.CurveKeys->Rename(*Template->MakeUniqueCurveName(Track.CurveKeys, Track.CurveKeys->GetOuter()), Blueprint, REN_DontCreateRedirectors);
				}
			}

			// 处理向量轨道
			for (auto TrackIt = Template->VectorTracks.CreateIterator(); TrackIt; ++TrackIt)
			{
				FTTVectorTrack& Track = *TrackIt;
				if (!Track.bIsExternalCurve && Track.CurveVector && Track.CurveVector->GetOuter()->IsA(UBlueprint::StaticClass()))
				{
					Track.CurveVector->Rename(*Template->MakeUniqueCurveName(Track.CurveVector, Track.CurveVector->GetOuter()), Blueprint, REN_DontCreateRedirectors);
				}
			}

			// 处理线性颜色轨道
			for (auto TrackIt = Template->LinearColorTracks.CreateIterator(); TrackIt; ++TrackIt)
			{
				FTTLinearColorTrack& Track = *TrackIt;
				if (!Track.bIsExternalCurve && Track.CurveLinearColor && Track.CurveLinearColor->GetOuter()->IsA(UBlueprint::StaticClass()))
				{
					Track.CurveLinearColor->Rename(*Template->MakeUniqueCurveName(Track.CurveLinearColor, Track.CurveLinearColor->GetOuter()), Blueprint, REN_DontCreateRedirectors);
				}
			}
		}

		FBlueprintEditorUtils::ValidateBlueprintChildVariables(Blueprint, TimelineName);
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}
}

/**
 * 检查节点是否与目标图表兼容
 */
bool UK2Node_BaseTimeline::IsCompatibleWithGraph(const UEdGraph* TargetGraph) const
{
	if (UK2Node::IsCompatibleWithGraph(TargetGraph))
	{
		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph);
		if (Blueprint)
		{
			const UEdGraphSchema_K2* K2Schema = Cast<UEdGraphSchema_K2>(TargetGraph->GetSchema());
			check(K2Schema);

			const bool bSupportsEventGraphs = FBlueprintEditorUtils::DoesSupportEventGraphs(Blueprint);
			const bool bAllowEvents = (K2Schema->GetGraphType(TargetGraph) == GT_Ubergraph) && bSupportsEventGraphs &&
				(Blueprint->BlueprintType != BPTYPE_MacroLibrary);

			if (bAllowEvents)
			{
				return DoesSupportTimelines(Blueprint);
			}
			else
			{
				bool bCompositeOfUbberGraph = false;

				// 如果复合图表的外部有一个Ubergraph，则允许其包含时间轴
				if (bSupportsEventGraphs && K2Schema->IsCompositeGraph(TargetGraph))
				{
					while (TargetGraph)
					{
						if (UK2Node_Composite* Composite = Cast<UK2Node_Composite>(TargetGraph->GetOuter()))
						{
							TargetGraph = Cast<UEdGraph>(Composite->GetOuter());
						}
						else if (K2Schema->GetGraphType(TargetGraph) == GT_Ubergraph)
						{
							bCompositeOfUbberGraph = true;
							break;
						}
						else
						{
							TargetGraph = Cast<UEdGraph>(TargetGraph->GetOuter());
						}
					}
				}
				return bCompositeOfUbberGraph ? DoesSupportTimelines(Blueprint) : false;
			}
		}
	}

	return false;
}

/**
 * 在编译期间验证节点
 * 检查是否存在所需的初始化节点
 */
void UK2Node_BaseTimeline::ValidateNodeDuringCompilation(FCompilerResultsLog& MessageLog) const
{
	Super::ValidateNodeDuringCompilation(MessageLog);

	UEdGraph* Graph = GetGraph();

	if (Graph)
	{
		FName RequiredNodeName = GetRequiredNodeInBlueprint();
		for (TObjectPtr<class UEdGraphNode> Node : Graph->Nodes)
		{
			UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node.Get());
			if (CallNode && CallNode->GetFunctionName() == RequiredNodeName)
				return;
		}

		// 格式化错误消息
		const FText NodeName = FText::FromString(RequiredNodeName.ToString());
		const FText Message = FText::Format(NSLOCTEXT("UK2Node_BaseTimeline", "MissingInitialization", 
			"蓝图中缺少 '{0}' 节点。你应该在BeginPlay时调用 '{1}' 以使时间轴正常工作。 @@"), NodeName, NodeName);
		MessageLog.Error(*Message.ToString(), this);
	}
}

/**
 * 获取节点的菜单操作
 * 注册节点的创建操作
 */
void UK2Node_BaseTimeline::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	// 操作在特定的对象键下注册；这样如果对象键被修改（或删除），
	// 操作可能需要更新（或删除）...这里我们使用节点的类（所以如果节点
	// 类型消失，那么操作也应该随之消失）
	UClass* ActionKey = GetClass();

	// 为了避免不必要地实例化UBlueprintNodeSpawner，首先
	// 检查注册器是否正在寻找这种类型的操作
	// （可能正在为特定资产重新生成操作，因此
	// 注册器只接受与该资产对应的操作）
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		// 自定义时间轴节点的Lambda函数
		auto CustomizeTimelineNodeLambda = [](UEdGraphNode* NewNode, bool bIsTemplateNode)
		{
			UK2Node_BaseTimeline* TimelineNode = CastChecked<UK2Node_BaseTimeline>(NewNode);

			UBlueprint* Blueprint = TimelineNode->GetBlueprint();
			if (Blueprint != nullptr)
			{
				TimelineNode->TimelineName = FBlueprintEditorUtils::FindUniqueTimelineName(Blueprint);
				if (!bIsTemplateNode && AddNewTimeline(Blueprint, TimelineNode->TimelineName))
				{
					// 设置新时间轴的默认属性
					TimelineNode->bAutoPlay = false;
					TimelineNode->bLoop = false;
					TimelineNode->bReplicated = false;
					TimelineNode->bIgnoreTimeDilation = false;
				}
			}
		};

		NodeSpawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateStatic(CustomizeTimelineNodeLambda);
			ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}