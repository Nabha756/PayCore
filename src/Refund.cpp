#include "paycore/Refund.h"

#include <sstream>
#include <utility>

#include "paycore/Exceptions.h"

namespace paycore {

Refund::Refund(std::string id, std::string customerId, Amount amount,
               std::string originalPaymentId, std::string reason, Timestamp createdAt)
    : Refund(std::move(id), std::move(customerId), amount, std::move(originalPaymentId),
             std::move(reason), TransactionStatus::Completed, createdAt, createdAt) {}

Refund::Refund(std::string id, std::string customerId, Amount amount,
               std::string originalPaymentId, std::string reason, TransactionStatus status,
               Timestamp createdAt, Timestamp updatedAt)
    : Transaction(std::move(id), std::move(customerId), amount, status, createdAt, updatedAt),
      originalPaymentId_(std::move(originalPaymentId)),
      reason_(std::move(reason)) {
    if (originalPaymentId_.empty()) {
        throw ValidationError("A refund must reference the original payment");
    }
    if (status != TransactionStatus::Completed) {
        throw ValidationError("A refund must have status COMPLETED");
    }
}

bool Refund::isAllowedTransition(TransactionStatus, TransactionStatus) const {
    return false;  // refunds are final once created
}

std::string Refund::details() const {
    std::ostringstream out;
    out << Transaction::details()
        << "Refund for     : " << originalPaymentId_ << '\n'
        << "Reason         : " << reason_ << '\n';
    return out.str();
}

std::string Refund::serialize() const {
    return serializeCommon() + '|' + originalPaymentId_ + '|' + reason_;
}

}  // namespace paycore
