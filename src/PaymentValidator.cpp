#include "paycore/PaymentValidator.h"

#include <cctype>
#include <chrono>
#include <ctime>
#include <regex>

#include "paycore/Exceptions.h"
#include "paycore/Utils.h"

namespace paycore {

namespace {

bool allDigits(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

}  // namespace

void PaymentValidator::validateAmount(Amount amount) const {
    if (amount <= 0) {
        throw ValidationError("Amount must be greater than zero");
    }
    if (amount > kMaxAmount) {
        throw ValidationError("Amount exceeds the limit of " + money::format(kMaxAmount) + " INR");
    }
}

void PaymentValidator::validateCustomerId(const std::string& customerId) const {
    // 3-20 characters: letters, digits, '-' or '_'. Example: CUST-1001
    static const std::regex pattern("^[A-Za-z0-9_-]{3,20}$");
    if (!std::regex_match(customerId, pattern)) {
        throw ValidationError("Customer ID must be 3-20 letters, digits, '-' or '_'");
    }
}

void PaymentValidator::validate(const PaymentDetails& details) const {
    const std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &t);
#else
    localtime_r(&t, &local);
#endif
    validate(details, local.tm_year + 1900, local.tm_mon + 1);
}

void PaymentValidator::validate(const PaymentDetails& details, int currentYear,
                                int currentMonth) const {
    switch (details.method) {
        case PaymentMethod::Card:
            validateCardNumber(details.account);
            validateExpiry(details.expiry, currentYear, currentMonth);
            validateCvv(details.cvv);
            break;
        case PaymentMethod::Upi:
            validateUpiId(details.account);
            break;
        case PaymentMethod::Wallet:
            validateWalletNumber(details.account);
            break;
    }
}

void PaymentValidator::validateCardNumber(const std::string& cardNumber) const {
    const std::string digits = normaliseCardNumber(cardNumber);
    if (!allDigits(digits) || digits.size() < 13 || digits.size() > 19) {
        throw ValidationError("Card number must contain 13 to 19 digits");
    }
    if (!passesLuhn(digits)) {
        throw ValidationError("Card number failed the Luhn checksum");
    }
}

void PaymentValidator::validateExpiry(const std::string& expiry, int currentYear,
                                      int currentMonth) const {
    static const std::regex pattern("^(0[1-9]|1[0-2])/([0-9]{2})$");
    std::smatch match;
    if (!std::regex_match(expiry, match, pattern)) {
        throw ValidationError("Expiry must be in MM/YY format");
    }
    const int month = std::stoi(match[1].str());
    const int year = 2000 + std::stoi(match[2].str());
    // A card is valid until the end of its expiry month.
    if (year < currentYear || (year == currentYear && month < currentMonth)) {
        throw ValidationError("Card has expired");
    }
}

void PaymentValidator::validateCvv(const std::string& cvv) const {
    if (!allDigits(cvv) || cvv.size() < 3 || cvv.size() > 4) {
        throw ValidationError("CVV must be 3 or 4 digits");
    }
}

void PaymentValidator::validateUpiId(const std::string& upiId) const {
    // name@handle, for example ravi.k@okaxis
    static const std::regex pattern("^[A-Za-z0-9._-]{2,64}@[A-Za-z]{2,32}$");
    if (!std::regex_match(upiId, pattern)) {
        throw ValidationError("UPI ID must look like name@bank (e.g. ravi@okaxis)");
    }
}

void PaymentValidator::validateWalletNumber(const std::string& mobile) const {
    // Indian mobile numbers: 10 digits starting with 6-9
    if (mobile.size() != 10 || !allDigits(mobile) || mobile[0] < '6') {
        throw ValidationError("Wallet number must be a 10-digit mobile number starting with 6-9");
    }
}

void PaymentValidator::validateText(const std::string& value, const std::string& fieldName) const {
    if (text::trim(value).empty()) {
        throw ValidationError(fieldName + " cannot be empty");
    }
    if (value.size() > kMaxTextLength) {
        throw ValidationError(fieldName + " must be at most " + std::to_string(kMaxTextLength) +
                              " characters");
    }
    if (value.find_first_of("|\r\n") != std::string::npos) {
        throw ValidationError(fieldName + " cannot contain '|' or line breaks");
    }
}

bool PaymentValidator::passesLuhn(const std::string& digits) {
    if (!allDigits(digits)) return false;
    int sum = 0;
    bool doubleIt = false;
    // Walk from the rightmost digit, doubling every second digit.
    for (auto it = digits.rbegin(); it != digits.rend(); ++it) {
        int d = *it - '0';
        if (doubleIt) {
            d *= 2;
            if (d > 9) d -= 9;
        }
        sum += d;
        doubleIt = !doubleIt;
    }
    return sum % 10 == 0;
}

std::string PaymentValidator::normaliseCardNumber(const std::string& cardNumber) {
    std::string digits;
    for (char c : cardNumber) {
        if (c != ' ' && c != '-') digits += c;
    }
    return digits;
}

std::string PaymentValidator::mask(const PaymentDetails& details) {
    switch (details.method) {
        case PaymentMethod::Card: {
            const std::string digits = normaliseCardNumber(details.account);
            const std::string last4 = digits.size() >= 4 ? digits.substr(digits.size() - 4) : digits;
            return "**** **** **** " + last4;
        }
        case PaymentMethod::Upi: {
            const auto at = details.account.find('@');
            const std::string name = details.account.substr(0, at);
            const std::string handle = at == std::string::npos ? "" : details.account.substr(at);
            return name.substr(0, 2) + "***" + handle;
        }
        case PaymentMethod::Wallet: {
            const std::string& m = details.account;
            const std::string last4 = m.size() >= 4 ? m.substr(m.size() - 4) : m;
            return "******" + last4;
        }
    }
    return "****";
}

}  // namespace paycore
