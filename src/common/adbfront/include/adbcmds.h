#pragma once

#if defined(__linux__) || defined(__APPLE__)
constexpr const char AdbFilename[] = "adb";
#elif defined(WIN32)
constexpr const char AdbFilename[] = "adb.exe";
#endif

constexpr auto ExecWaitTime = 10000;

constexpr const char NA[] = "N/A";

constexpr const char PropProductModel[] = "ro.product.model";
constexpr const char PropProductMarketname[] = "ro.product.marketname";
constexpr const char PropProductManufacturer[] = "ro.product.manufacturer";
constexpr const char PropProductMarketingName[] = "ro.config.marketing_name";
constexpr const char PropAndroidVersion[] = "ro.build.version.release";
