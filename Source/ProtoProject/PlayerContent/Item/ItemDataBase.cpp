// Fill out your copyright notice in the Description page of Project Settings.


#include "ItemDataBase.h"

FText UItemDataBase::GetContextActionText() const
{
    switch (GetContextAction())
    {
        case EItemContextAction::Equip:
            return NSLOCTEXT("Item", "ContextAction_Equip", "장착");
        case EItemContextAction::Use:
            return NSLOCTEXT("Item", "ContextAction_Use", "사용");
        default:
            return FText::GetEmpty();
    }
}

