#pragma once

#include <cmath>
#include <functional>
#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <sstream>
#include <cctype>
#include <array>
#include <algorithm>

#include <string.h>

#include "classfieldraw.h"

#ifndef CLASSFIELDDATA
#error "Classfield data is not entry"
#endif

class NumberPreview;
class CountryCodeDB;
struct CountryCode;

struct CountryCode
{
    std::vector<std::uint32_t> codes;
    std::string country;
    std::string dialcode;
    std::string localCode;

    CountryCode() = default;
    CountryCode(const CountryCode &) = default;
    CountryCode(CountryCode &&other) : codes(std::move(other.codes)), country(std::move(other.country)), dialcode(std::move(other.dialcode)), localCode(std::move(other.localCode))
    {
    }

    CountryCode &operator=(const CountryCode &other)
    {
        codes = other.codes;
        country = other.country;
        dialcode = other.dialcode;
        localCode = other.localCode;
        return *this;
    }

    CountryCode &operator=(CountryCode &&other)
    {
        codes = std::move(other.codes);
        country = std::move(other.country);
        dialcode = std::move(other.dialcode);
        localCode = std::move(other.localCode);
        return *this;
    }
};

class CountryCodeDB
{
    friend class NumberPreview;

public:
    CountryCodeDB(const CountryCodeDB &) = default;
    CountryCodeDB(CountryCodeDB &&) = default;
    CountryCodeDB &operator=(const CountryCodeDB &) = default;
    CountryCodeDB &operator=(CountryCodeDB &&) = default;
    CountryCodeDB(const char *classFieldRaw);
    inline std::vector<int> localCodes() const
    {
        return _localCodes;
    }

    static const CountryCodeDB &instance();
    static std::string country(int code);
    static std::string dialcode(int code);
    static CountryCode countryCode(int code);

private:
    std::vector<int> _localCodes;
    std::vector<CountryCode> _class;

    static const CountryCodeDB __instance;
};

enum NumberFormat : int
{
    Beauty = 0,
    Compact = 1,

    Global = 0,
    Local = 2
};

class NumberPreview
{
public:
    NumberPreview(std::string phoneNumber);

    std::string format(int numberFormatFlag = NumberFormat::Beauty) const;

    bool isGenericNumber() const;
    bool isEmpty() const;

    std::string country() const;
    std::string dialCode() const;
    int countryCode() const;

private:
    std::uint64_t _numerics;
    int _countryCode;
    std::string numberDouble(std::uint32_t num, std::uint32_t levels) const;
};
