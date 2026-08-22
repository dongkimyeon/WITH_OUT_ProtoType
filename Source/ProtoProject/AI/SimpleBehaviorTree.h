#pragma once

#include "CoreMinimal.h"

// Enemy/Companion 공용 경량 C++ 행동 트리. UE 네이티브 BehaviorTree/Blackboard 대신
// TFunction 기반 Selector/Sequence/Condition/Action 노드를 매 틱 재귀 평가한다.
// TAgent는 각 액터 타입(AEnemyBase, ACompanionNPC 등)으로 인스턴스화한다.
enum class EBTNodeResult : uint8
{
	Succeeded,
	Failed,
	Running
};

template<typename TAgent>
class TBTNode
{
public:
	virtual ~TBTNode() = default;
	virtual EBTNodeResult Tick(TAgent* Agent, float DeltaTime) = 0;
};

// OR: 자식을 순서대로 평가해 Succeeded/Running을 반환하는 첫 자식에서 멈춘다.
template<typename TAgent>
class TBTSelectorNode : public TBTNode<TAgent>
{
public:
	TArray<TSharedPtr<TBTNode<TAgent>>> Children;

	virtual EBTNodeResult Tick(TAgent* Agent, float DeltaTime) override
	{
		for (const TSharedPtr<TBTNode<TAgent>>& Child : Children)
		{
			if (!Child.IsValid())
			{
				continue;
			}

			const EBTNodeResult Result = Child->Tick(Agent, DeltaTime);
			if (Result == EBTNodeResult::Succeeded || Result == EBTNodeResult::Running)
			{
				return Result;
			}
		}

		return EBTNodeResult::Failed;
	}
};

// AND: 자식을 순서대로 평가해 Failed/Running을 반환하는 첫 자식에서 멈춘다.
template<typename TAgent>
class TBTSequenceNode : public TBTNode<TAgent>
{
public:
	TArray<TSharedPtr<TBTNode<TAgent>>> Children;

	virtual EBTNodeResult Tick(TAgent* Agent, float DeltaTime) override
	{
		for (const TSharedPtr<TBTNode<TAgent>>& Child : Children)
		{
			if (!Child.IsValid())
			{
				return EBTNodeResult::Failed;
			}

			const EBTNodeResult Result = Child->Tick(Agent, DeltaTime);
			if (Result == EBTNodeResult::Failed || Result == EBTNodeResult::Running)
			{
				return Result;
			}
		}

		return EBTNodeResult::Succeeded;
	}
};

template<typename TAgent>
class TBTConditionNode : public TBTNode<TAgent>
{
public:
	explicit TBTConditionNode(TFunction<bool(TAgent*)> InCondition)
		: Condition(MoveTemp(InCondition))
	{
	}

	virtual EBTNodeResult Tick(TAgent* Agent, float DeltaTime) override
	{
		return Condition && Condition(Agent) ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
	}

private:
	TFunction<bool(TAgent*)> Condition;
};

template<typename TAgent>
class TBTActionNode : public TBTNode<TAgent>
{
public:
	explicit TBTActionNode(TFunction<EBTNodeResult(TAgent*, float)> InAction)
		: Action(MoveTemp(InAction))
	{
	}

	virtual EBTNodeResult Tick(TAgent* Agent, float DeltaTime) override
	{
		return Action ? Action(Agent, DeltaTime) : EBTNodeResult::Failed;
	}

private:
	TFunction<EBTNodeResult(TAgent*, float)> Action;
};
