#include "paycore/Transaction.h"

#include <sstream>
#include <utility>

#include "paycore/Exceptions.h"
#include "paycore/Utils.h"

namespace paycore {

Transaction::Transaction(std::string id, std::string customerId, Amount amount,
                         TransactionStatus status, Timestamp createdAt, Timestamp updatedAt)
    : id_(std::move(id)),
      customerId_(std::move(customerId)),
      amount_(amount),
      status_(status),
      createdAt_(createdAt),
      updatedAt_(updatedAt) {
    if (id_.empty()) throw ValidationError("Transaction ID cannot be empty");
    if (customerId_.empty()) throw ValidationError("Customer ID cannot be empty");
    if (amount_ <= 0) throw ValidationError("Transaction amount must be positive");
    if (updatedAt_ < createdAt_) throw ValidationError("Updated time is before created time");
}

void Transaction::changeStatus(TransactionStatus next) {
    if (!isAllowedTransition(status_, next)) {
        throw InvalidOperationError("Cannot change " + id_ + " from " + toString(status_) +
                                    " to " + toString(next));
    }
    status_ = next;
    // Never move time backwards, even if the system clock does.
    const Timestamp current = timeutil::now();
    updatedAt_ = current > updatedAt_ ? current : updatedAt_;
}

std::string Transaction::details() const {
    std::ostringstream out;
    out << "Transaction ID : " << id_ << '\n'
        << "Type           : " << toString(type()) << '\n'
        << "Customer ID    : " << customerId_ << '\n'
        << "Amount (INR)   : " << money::format(amount_) << '\n'
        << "Status         : " << toString(status_) << '\n'
        << "Created        : " << timeutil::format(createdAt_) << '\n'
        << "Last updated   : " << timeutil::format(updatedAt_) << '\n';
    return out.str();
}

std::string Transaction::serializeCommon() const {
    std::ostringstream out;
    out << toString(type()) << '|' << id_ << '|' << customerId_ << '|' << amount_ << '|'
        << toString(status_) << '|' << createdAt_ << '|' << updatedAt_;
    return out.str();
}

}  // namespace paycore
