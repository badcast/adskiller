#include "network.h"


bool UserData::isBOMZH() const
{
    return credits == 0 && vipDays == 0;
}

bool UserData::hasBalance() const
{
    return credits > 0;
}

bool UserData::hasVipAccount() const
{
    return vipDays > 0;
}

bool LabStatusInfo::ready() const
{
    return analyzeStatus == "verified" || analyzeStatus == "part-verify";
}

bool LabStatusInfo::exists() const
{
    return analyzeStatus != "no-exists";
}
