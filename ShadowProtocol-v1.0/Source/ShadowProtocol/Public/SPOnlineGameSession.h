#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameSession.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "SPOnlineGameSession.generated.h"

/** Dedicated-server OSS lifecycle. Backend redemption remains the admission authority. */
UCLASS()
class SHADOWPROTOCOL_API ASPOnlineGameSession : public AGameSession
{
    GENERATED_BODY()
public:
    ASPOnlineGameSession();
    virtual void RegisterServer() override;
    bool IsAcceptingAdmissions() const;
    bool StartProtocolSession();
    void EndProtocolSession();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    enum class EOperation : uint8 { None, Create, Start, End };
    IOnlineSessionPtr Sessions;
    FDelegateHandle CreateHandle;
    FDelegateHandle StartHandle;
    FDelegateHandle EndHandle;
    EOperation Operation = EOperation::None;
    double OperationDeadline = 0.0;
    bool bOwnsSession = false;
    bool bFailed = false;
    bool bClosing = false;
    void BeginOperation(EOperation Next);
    void FailSession();
    void ClearDelegates();
    void OnCreated(FName Name, bool bSuccess);
    void OnStarted(FName Name, bool bSuccess);
    void OnEnded(FName Name, bool bSuccess);
};
