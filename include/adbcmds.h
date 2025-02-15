#ifndef ADBCMDS_H
#define ADBCMDS_H


#ifdef __linux__
constexpr const char cmd_adb[] = "adb";
#elif _WIN32
constexpr const char cmd_adb[] = "adb.exe";
#endif

constexpr const char CmdGetProductModel[] = "ro.product.model";
constexpr const char CmdGetProductMarketname[] = "ro.product.marketname";
constexpr const char CmdGetProductManufacturer[] = "ro.product.manufacturer";


#endif // ADBCMDS_H
