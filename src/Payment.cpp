#include "paycore/Payment.h"

#include <sstream>
#include <utility>

#include "paycore/Exceptions.h"
#include "paycore/Utils.h"

namespace paycore {

Payment::Payment(std::string id, std::string customerId, Amount amount, PaymentMethod method,
                 std::string maskedAccount, Timestamp createdAt)
    : Payment(std::move(id), std::move(customerId), amount, method, std::move(maskedAccount),
              TransactionStatus::Pending, createdAt, createdAt, 0, "") {}

Payment::Payment(std::string id, std::string customerId, Amount amount, PaymentMethod method,
                 std::string maskedAccount, TransactionStatus status, Timestamp createdAt,
                 Timestamp updatedAt, Amount refundedAmount, std::string failureReason)
    : Transaction(std::move(id), std::move(customerId), amount, status, createdAt, updatedAt),
      method_(method),
      maskedAccount_(std::move(maskedAccount)),
      refundedAmount_(refundedAmount),
      failureReason_(std::move(failureReason)) {
    // Make sure restored data is internally consistent.
    if (refundedAmount_ < 0 || refundedAmount_ > this->amount()) {
        throw ValidationError("Refunded amount must be between 0 and the payment amount");
    }
    const bool hasRefunds = refundedAmount_ > 0;
    const bool refundStatus = status == TransactionStatus::PartiallyRefunded ||
                              status == TransactionStatus::Refunded;
    if (hasRefunds != refundStatus) {
        throw ValidationError("Refunded amount does not match status " + toString(status));
    }
    if (status == TransactionStatus::Refunded && refundedAmount_ != this->amount()) {
        throw ValidationError("A REFUNDED payment must be refunded in full");
    }
    if (status == TransactionStatus::PartiallyRefunded && refundedAmount_ == this->amount()) {
        throw ValidationError("A PARTIALLY_REFUNDED payment cannot be refunded in full");
    }
}

bool Payment::canBeRefunded() const noexcept {
    const bool rightStatus = status() == TransactionStatus::Completed ||
                             status() == TransactionStatus::PartiallyRefunded;
    return rightStatus && refundableAmount() > 0;
}

void Payment::markCompleted() {
    changeStatus(TransactionStatus::Completed);
}

void Payment::markFailed(const std::string& reason) {
    changeStatus(TransactionStatus::Failed);
    failureReason_ = reason;
}

void Payment::cancel() {
    changeStatus(TransactionStatus::Cancelled);
}

void Payment::applyRefund(Amount refundAmount) {
    if (!canBeRefunded()) {
        throw InvalidOperationError("Payment " + id() + " is " + toString(status()) +
                                    " and cannot be refunded");
    }
    if (refundAmount <= 0) {
        throw ValidationError("Refund amount must be positive");
    }
    if (refundAmount > refundableAmount()) {
        throw ValidationError("Refund amount " + money::format(refundAmount) +
                              " exceeds refundable amount " + money::format(refundableAmount()));
    }

    const Amount newTotal = refundedAmount_ + refundAmount;
    changeStatus(newTotal == amount() ? TransactionStatus::Refunded
                                      : TransactionStatus::PartiallyRefunded);
    refundedAmount_ = newTotal;  // only updated after the status change succeeded
}

bool Payment::isAllowedTransition(TransactionStatus from, TransactionStatus to) const {
    using S = TransactionStatus;
    switch (from) {
        case S::Pending:
            return to == S::Completed || to == S::Failed || to == S::Cancelled;
        case S::Completed:
        case S::PartiallyRefunded:
            return to == S::PartiallyRefunded || to == S::Refunded;
        case S::Failed:
        case S::Cancelled:
        case S::Refunded:
            return false;  // final states
    }
    return false;
}

std::string Payment::details() const {
    std::ostringstream out;
    out << Transaction::details()  // reuse the shared part from the base class
        << "Method         : " << toString(method_) << '\n'
        << "Account        : " << maskedAccount_ << '\n'
        << "Refunded (INR) : " << money::format(refundedAmount_) << '\n'
        << "Refundable     : " << money::format(canBeRefunded() ? refundableAmount() : 0) << '\n';
    if (!failureReason_.empty()) {
        out << "Failure reason : " << failureReason_ << '\n';
    }
    return out.str();
}

std::string Payment::serialize() const {
    std::ostringstream out;
    out << serializeCommon() << '|' << toString(method_) << '|' << maskedAccount_ << '|'
        << refundedAmount_ << '|' << failureReason_;
    return out.str();
}

}  // namespace paycore
