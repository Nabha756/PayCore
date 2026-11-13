#include "paycore/Utils.h"

#include <cctype>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include "paycore/Exceptions.h"

namespace paycore {

namespace money {

Amount parse(const std::string& input) {
    const std::string s = text::trim(input);
    if (s.empty()) {
        throw ValidationError("Amount is required");
    }

    const auto dot = s.find('.');
    const std::string whole = s.substr(0, dot);
    const std::string fraction = (dot == std::string::npos) ? "" : s.substr(dot + 1);

    auto allDigits = [](const std::string& part) {
        for (char c : part) {
            if (!std::isdigit(static_cast<unsigned char>(c))) return false;
        }
        return true;
    };

    if (whole.empty() || !allDigits(whole) || !allDigits(fraction)) {
        throw ValidationError("Amount must be a positive number like 1500 or 1500.50");
    }
    if (dot != std::string::npos && (fraction.empty() || fraction.size() > 2)) {
        throw ValidationError("Amount can have at most two decimal places");
    }
    if (whole.size() > 12) {  // protects against overflow of int64
        throw ValidationError("Amount is too large");
    }

    Amount paise = std::stoll(whole) * 100;
    if (fraction.size() == 1) paise += (fraction[0] - '0') * 10;
    if (fraction.size() == 2) paise += std::stoll(fraction);
    return paise;
}

std::string format(Amount paise) {
    const bool negative = paise < 0;
    const Amount absolute = negative ? -paise : paise;
    std::ostringstream out;
    out << (negative ? "-" : "") << absolute / 100 << '.'
        << std::setw(2) << std::setfill('0') << absolute % 100;
    return out.str();
}

}  // namespace money

namespace timeutil {

Timestamp now() {
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

std::string format(Timestamp ts) {
    const std::time_t t = static_cast<std::time_t>(ts);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &t);
#else
    localtime_r(&t, &local);
#endif
    std::ostringstream out;
    out << std::put_time(&local, "%Y-%m-%d %H:%M:%S");
    return out.str();
}

}  // namespace timeutil

namespace text {

std::string trim(const std::string& s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

std::vector<std::string> split(const std::string& s, char delimiter) {
    std::vector<std::string> parts;
    std::string current;
    for (char c : s) {
        if (c == delimiter) {
            parts.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }
    parts.push_back(current);
    return parts;
}

long long toInt64(const std::string& s, const std::string& fieldName) {
    try {
        std::size_t used = 0;
        const long long value = std::stoll(s, &used);
        if (used != s.size()) {
            throw std::invalid_argument("trailing characters");
        }
        return value;
    } catch (const std::exception&) {
        throw ValidationError("invalid number in field '" + fieldName + "': '" + s + "'");
    }
}

}  // namespace text

}  // namespace paycore
