// Copyright (c) 2026 Damian Nowakowski. All rights reserved.

#pragma once

#include "ECFActionBase.h"
#include "ECFDoNoMoreThanXTime.generated.h"

ECF_PRAGMA_DISABLE_OPTIMIZATION

UCLASS()
class XTOOLS_ENHANCEDCODEFLOW_API UECFDoNoMoreThanXTime
    : public UECFActionBase {
  GENERATED_BODY()

  friend class UECFSubsystem;

protected:
  TUniqueFunction<void()> ExecFunc;
  float LockTime = 0.f;
  float CurrentTime = 0.f;
  int32 ExecsEnqueued = 0;
  int32 MaxExecsEnqueued = 0;

  bool Setup(TUniqueFunction<void()> &&InExecFunc, float InTime,
             int32 InMaxExecsEnqueued) {
    ExecFunc = MoveTemp(InExecFunc);
    LockTime = InTime;
    MaxExecsEnqueued = InMaxExecsEnqueued;

    if (ExecFunc && LockTime > 0 && MaxExecsEnqueued > 0) {
      return true;
    } else {
#if ECF_LOGS
      UE_LOG(LogECF, Error, TEXT("ECF - [%s] Do No More Than X Times failed to start. Check the lock time, queue limit, and callback."), *Settings.Label);
#endif
      return false;
    }
  }

  void Init() override {
    CurrentTime = 0.f;
    ExecsEnqueued = 0;
    // 【防御性编程】：确保 Owner 仍然有效
    if (HasValidOwner() && ExecFunc) {
      ExecFunc();
    }
  }

  bool Reset(bool bCallUpdate) override {
    CurrentTime = 0.f;
    ExecsEnqueued = 0;
    if (bCallUpdate && HasValidOwner() && ExecFunc) {
      ExecFunc();
    }
    return true;
  }

  void RetriggeredInstancedAction() override {
    if (CurrentTime < LockTime) {
      if (ExecsEnqueued < MaxExecsEnqueued) {
        ExecsEnqueued++;
      }
    } else {
      CurrentTime = 0;
      // 【防御性编程】：确保 Owner 仍然有效
      if (HasValidOwner() && ExecFunc) {
        ExecFunc();
      }
    }
  }

  void Tick(float DeltaTime) override {
#if STATS
    DECLARE_SCOPE_CYCLE_COUNTER(TEXT("DoNoMoreThanXTime - Tick"),
                                STAT_ECFDETAILS_DONOMORETHANXTIMES,
                                STATGROUP_ECFDETAILS);
#endif
#if ECF_INSIGHT_PROFILING
    TRACE_CPUPROFILER_EVENT_SCOPE("ECF - DoNoMoreThanXTime Tick");
#endif
    // 队列为空且当前锁窗已结束时，动作自然完成，避免永久常驻 Tick。
    if ((ExecsEnqueued <= 0) && (CurrentTime >= LockTime)) {
      MarkAsFinished();
      return;
    }

    if (CurrentTime < LockTime) {
      CurrentTime += DeltaTime;
    }

    if ((ExecsEnqueued > 0) && (CurrentTime >= LockTime)) {
      CurrentTime = 0;
      ExecsEnqueued--;
      // 【防御性编程】：确保 Owner 仍然有效
      if (HasValidOwner() && ExecFunc) {
        ExecFunc();
      }
    }
  }
};

ECF_PRAGMA_ENABLE_OPTIMIZATION
