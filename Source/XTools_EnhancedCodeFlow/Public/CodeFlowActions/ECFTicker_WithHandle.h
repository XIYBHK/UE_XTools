// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#pragma once

#include "ECFActionBase.h"
#include "ECFTicker_WithHandle.generated.h"

ECF_PRAGMA_DISABLE_OPTIMIZATION

UCLASS()
class XTOOLS_ENHANCEDCODEFLOW_API UECFTicker_WithHandle
    : public UECFActionBase {
  GENERATED_BODY()

  friend class UECFSubsystem;

protected:
  TUniqueFunction<void(float, FECFHandle)> TickFunc;
  TUniqueFunction<void(bool)> CallbackFunc;
  TUniqueFunction<void()> CallbackFunc_NoStopped;
  float TickingTime = 0.f;
  float CurrentTime = 0.f;
  // 单次完成闩锁：用户在 TickFunc 内 Stop 自身会触发 Complete(true)，
  // 同帧若计时满足再走 Complete(false) 会双播回调；先到者生效，后者空操作
  bool bCompletionCalled = false;

  bool Setup(float InTickingTime,
             TUniqueFunction<void(float, FECFHandle)> &&InTickFunc,
             TUniqueFunction<void(bool)> &&InCallbackFunc = nullptr) {
    TickingTime = InTickingTime;
    TickFunc = MoveTemp(InTickFunc);
    CallbackFunc = MoveTemp(InCallbackFunc);

    if (TickFunc && (TickingTime > 0.f || TickingTime == -1.f)) {
      if (TickingTime > 0.f) {
        SetMaxActionTime(TickingTime);
      }

      CurrentTime = 0.f;
      return true;
    } else {
      ensureMsgf(false,
                 TEXT("ECF - Ticker(2) failed to start. Are you sure the "
                      "Ticking time and Ticking Function are set properly?"));
      return false;
    }
  }

  bool Setup(float InTickingTime,
             TUniqueFunction<void(float, FECFHandle)> &&InTickFunc,
             TUniqueFunction<void()> &&InCallbackFunc = nullptr) {
    CallbackFunc_NoStopped = MoveTemp(InCallbackFunc);
    return Setup(InTickingTime, MoveTemp(InTickFunc), [this](bool bStopped) {
      if (CallbackFunc_NoStopped) {
        CallbackFunc_NoStopped();
      }
    });
  }

  void Tick(float DeltaTime) override {
#if STATS
    DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Ticker - Tick"), STAT_ECFDETAILS_TICKER,
                                STATGROUP_ECFDETAILS);
#endif
    TickFunc(DeltaTime, HandleId);
    CurrentTime += DeltaTime;
    if (TickingTime > 0.f && CurrentTime >= TickingTime) {
      MarkAsFinished();
      Complete(false);
    }
  }

  void Complete(bool bStopped) override {
    if (bCompletionCalled) {
      return;
    }
    bCompletionCalled = true;
    // 【防御性编程】：确保 Owner 仍然有效
    if (HasValidOwner() && CallbackFunc) {
      CallbackFunc(bStopped);
    }
    // 注：Owner 已销毁时静默跳过回调，避免崩溃
  }
};

ECF_PRAGMA_ENABLE_OPTIMIZATION
