#include "paycore/IdGenerator.h"

#include <iomanip>
#include <sstream>

#include "paycore/Exceptions.h"

namespace paycore {

std::string IdGenerator::next(const std::string& prefix) {
    std::ostringstream out;
    out << prefix << '-' << std::setw(6) << std::setfill('0') << nextNumber_++;
    return out.str();
}

void IdGenerator::observe(const std::string& existingId) {
    // IDs look like PREFIX-000123. Anything else is ignored.
    const auto dash = existingId.rfind('-');
    if (dash == std::string::npos || dash + 1 >= existingId.size()) return;

    const std::string numberPart = existingId.substr(dash + 1);
    for (char c : numberPart) {
        if (c < '0' || c > '9') return;
    }
    if (numberPart.size() > 15) return;  // avoid overflow on malformed IDs

    const long long number = std::stoll(numberPart);
    if (number >= nextNumber_) {
        nextNumber_ = number + 1;
    }
}

}  // namespace paycore
