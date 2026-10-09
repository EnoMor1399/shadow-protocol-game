#include "SPBuildInfoLibrary.h"

namespace ShadowProtocolBuild
{
    static const TCHAR* ReleaseVersion = TEXT("1.0.1");
    static const TCHAR* NetworkBuildId = TEXT("SP-1.0.1");
    static const TCHAR* ContentRevision = TEXT("EMBASSY-PROTOCOL-101");
}

FString USPBuildInfoLibrary::GetReleaseVersion()
{
    return ShadowProtocolBuild::ReleaseVersion;
}

FString USPBuildInfoLibrary::GetNetworkBuildId()
{
    return ShadowProtocolBuild::NetworkBuildId;
}

FString USPBuildInfoLibrary::GetContentRevision()
{
    return ShadowProtocolBuild::ContentRevision;
}

bool USPBuildInfoLibrary::IsNetworkBuildCompatible(const FString& RemoteBuildId, const bool bRankedMatch)
{
    const bool bExactMatch = RemoteBuildId.Equals(ShadowProtocolBuild::NetworkBuildId, ESearchCase::CaseSensitive);
    if (bRankedMatch)
    {
        return bExactMatch;
    }

    // v1.0.1 keeps casual/dev compatibility conservative too. This branch is
    // explicit so a future server-driven compatibility window can be added
    // without changing Blueprint call sites.
    return bExactMatch;
}
