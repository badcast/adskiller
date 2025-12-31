#include "NumberPreview.h"

NumberPreview::NumberPreview(std::string number)
{
    int x, y;
    const CountryCodeDB &cdb = CountryCodeDB::instance();
    x = 0;
    // remove all ignoring case [ + - ( ) ] and whitespaces
    auto new_end = std::remove_if(std::begin(number), std::end(number), [&x](auto &item) { return item == '+' || item == '-' || item == '(' || item == ')' || std::isspace(item) || !std::isdigit(item) && (x = 1); });
    // End with no digital number are.
    if(x)
        return;
    number.erase(new_end, number.end());
    std::uint8_t x0 = 0;
    for(x = 0; x < number.size(); ++x)
    {
        if(number[x] != '0')
            break;
        ++x0;
    }

    // str to number and write Null value to high bits.
    _numerics = (std::atoll(number.c_str()) & 0xFFFFFFFFFFFF);
    _numerics |= static_cast<std::uint64_t>(x0) << 48;

    _countryCode = (0);
    if(isGenericNumber())
    {
        auto iter = std::end(cdb._class);
        x = number[x] - '0';
        if(std::end(cdb._localCodes) != std::find(std::begin(cdb._localCodes), std::end(cdb._localCodes), x))
        {
            _numerics |= 1ull << 63;
            iter = std::find_if(
                std::begin(cdb._class),
                std::end(cdb._class),
                [x](const CountryCode &ref)
                {
                    for(int i = 0; i < ref.codes.size(); ++i)
                        if((ref.localCode[i] - '0') == x)
                            return true;
                    return false;
                });
        }
        else
        {
            std::vector<int> _cods;
            x = static_cast<int>((_numerics & 0xFFFFFFFFFFFF) / 10000000000ul);
            if(x < 10)
            {
                y = (_numerics & 0xFFFFFFFFFFFF) / 100000000ul;
                _cods.push_back(y);
                _cods.push_back(y / 10);
            }
            _cods.push_back(x / 10);
            _cods.push_back(x / 100);
            _cods.push_back(x);

            for(y = 0; y < _cods.size() && iter == std::end(cdb._class); ++y)
            {
                x = _cods[y];
                if(x <= 0)
                    continue;
                iter = std::find_if(
                    std::begin(cdb._class),
                    std::end(cdb._class),
                    [x](auto &ref)
                    {
                        for(int i = 0; i < ref.codes.size(); ++i)
                            if(ref.codes[i] == x)
                                return true;
                        return false;
                    });
            }
        }
        if(iter != std::end(cdb._class))
            _countryCode = iter->codes.at(0);
    }
}

std::string NumberPreview::format(int numberFormatFlag) const
{
    const CountryCode ccdb = CountryCodeDB::countryCode(countryCode());
    std::string result;
    std::uint64_t _raw;
    int x, y;
    x = (_numerics >> 48) & 0xFF;
    result.append(x, '0');
    _raw = _numerics & 0xFFFFFFFFFFFF;
    if(isGenericNumber())
    {
        constexpr std::uint8_t vectors[] {3, 3, 3, 2, 2};
        std::array<char[8], 5> _parts;
        std::uint64_t tmp, tmp0;
        std::string _temp;
        for(x = _parts.size() - 1; x > -1; --x)
        {
            int p = std::pow(10, vectors[x]);
            tmp0 = _raw / p;
            tmp = _raw - tmp0 * p;
            _raw = tmp0;
            switch(x)
            {
                case 0:
                    // bit flag is they are Local data.
                    if((_numerics & 0x8000000000000000))
                        tmp = _countryCode;
                    _temp = std::move(((numberFormatFlag & 0x2) != 0) ? ccdb.localCode.c_str() : ("+" + std::to_string(tmp)).c_str());
                    break;
                default:
                    _temp = std::move(numberDouble(tmp, vectors[x]));
                    break;
            }
            std::snprintf(_parts[x], 8, "%s", _temp.c_str());
        }
        result.resize(128);
        y = std::snprintf(result.data(), 128, ((numberFormatFlag & 0x1) == NumberFormat::Beauty ? "%s (%s) %s-%s-%s" : "%s%s%s%s%s"), _parts[0], _parts[1], _parts[2], _parts[3], _parts[4]);
        result.resize(result.size() - 128 + y);
    }
    else
    {
        if(!result.empty() && _raw != 0)
            result += std::to_string(_raw);
    }
    return result;
}

inline bool NumberPreview::isGenericNumber() const
{
    return (_numerics & 0x8000000000000000) != 0 || _numerics <= 9999999999999ul && _numerics >= 10000000000ul;
}

inline bool NumberPreview::isEmpty() const
{
    return _numerics == 0;
}

inline std::string NumberPreview::country() const
{
    return CountryCodeDB::country(_countryCode);
}

inline std::string NumberPreview::dialCode() const
{
    return CountryCodeDB::dialcode(_countryCode);
}

inline int NumberPreview::countryCode() const
{
    return _countryCode;
}

std::string NumberPreview::numberDouble(std::uint32_t num, std::uint32_t levels) const
{
    std::string retval;
    while(levels > 0 && num < std::pow(10, --levels))
        retval += '0';
    if(num > 0)
        retval += std::to_string(num);
    return retval;
}

CountryCodeDB::CountryCodeDB(const char *classFieldRaw)
{
    const char *p = classFieldRaw, *pLeft;
    int x, y, l;
    _class.clear();
    while((pLeft = strchr(p, '\n')) != nullptr)
    {
        CountryCode cIn {};
        l = static_cast<int>(pLeft - p);
        for(x = 0, y = 0; x < l; ++x)
        {
            if(p[x] == ';' || x == l - 1)
            {
                // Country Code
                if(p[y] == 'C')
                {
                    cIn.country = std::move(std::string(p + y + 2, x - y - 2));
                }
                // Number
                else if(p[y] == 'c')
                {
                    std::string _t;
                    std::istringstream stream(std::move(std::string(p + y + 2, x - y - 2)));
                    while(std::getline(stream, _t, ','))
                    {
                        auto new_end = std::remove_if(std::begin(_t), std::end(_t), std::bind(std::equal_to<char>(), std::placeholders::_1, '-'));
                        _t.erase(new_end, std::end(_t));
                        cIn.codes.push_back(std::atoi(_t.c_str()));
                    }
                }
                // Dial code
                else if(p[y] == 'd')
                {
                    cIn.dialcode = std::move(std::string(p + y + 2, x - y - 2));
                }
                // Local code
                else if(p[y] == 'l')
                {
                    cIn.localCode = std::move(std::string(p + y + 2, x - y - 1));
                }
                y = x + 1;
            }
        }
        x = std::atoi(cIn.localCode.c_str());
        y = cIn.localCode.empty();
        _class.push_back(std::move(cIn));
        p = pLeft + 1;
        if(!y && x != 1 && std::end(_localCodes) == std::find(std::begin(_localCodes), std::end(_localCodes), x))
            _localCodes.push_back(x);
    }

    auto _sqr = [](const std::vector<std::uint32_t> &v) -> std::uint32_t
    {
        std::uint32_t retval = 0;
        for(std::uint32_t val : v)
        {
            retval += val * val;
        }
        return retval;
    };
    std::sort(std::begin(_class), std::end(_class), [_sqr](auto &lhs, auto &rhs) { return _sqr(lhs.codes) < _sqr(rhs.codes); });
}

const CountryCodeDB CountryCodeDB::__instance {CLASSFIELDDATA};

const CountryCodeDB &CountryCodeDB::instance()
{
    return CountryCodeDB::__instance;
}

inline std::string CountryCodeDB::country(int code)
{
    return countryCode(code).country;
}

inline std::string CountryCodeDB::dialcode(int code)
{
    return countryCode(code).dialcode;
}

inline CountryCode CountryCodeDB::countryCode(int code)
{
    CountryCode cc {};
    const CountryCodeDB &cdb = instance();

    auto iter = std::find_if(
        std::begin(cdb._class),
        std::end(cdb._class),
        [code](auto &ref)
        {
            for(int i = 0; i < ref.codes.size(); ++i)
                if(ref.codes[i] == code)
                    return true;
            return false;
        });
    if(iter != std::end(cdb._class))
    {
        cc = *iter;
    }
    else
    {
        cc.country = "Unknown";
        cc.dialcode = "UNKNOWN";
        cc.codes.push_back(static_cast<std::uint32_t>(0));
    }
    return cc;
}
