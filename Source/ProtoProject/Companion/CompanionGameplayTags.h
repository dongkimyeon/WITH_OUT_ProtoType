// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "NativeGameplayTags.h"

// 이동 명령 대상 액터에 붙이는 태그. 네이티브로 등록해 DefaultGameplayTags.ini 로드 순서 문제 없이
// 정적 초기화 시점부터 안전하게 사용할 수 있게 한다.
PROTOPROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Companion_Interactable);
PROTOPROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Companion_Waypoint);
