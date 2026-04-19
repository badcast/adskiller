#include "network.h"

bool UserDataInfo::isNotValidBalance() const
{
    return credits == 0 && vipDays == 0;
}

bool UserDataInfo::hasBalance() const
{
    return credits > 0;
}

bool UserDataInfo::hasVipAccount() const
{
    return vipDays > 0;
}

void UserDataInfo::cleanExceptLoginPass()
{
    QString lg = login;
    QString ps = pass;

    (*this) = {};
    login = lg;
    pass = ps;
}

bool LabStatusInfo::ready() const
{
    return analyzeStatus == "verified" || analyzeStatus == "part-verify";
}

bool LabStatusInfo::exists() const
{
    return analyzeStatus != "no-exists";
}
