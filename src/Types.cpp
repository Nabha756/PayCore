#include "paycore/Types.h"

#include <stdexcept>

namespace paycore {

std::string toString(TransactionType type) {
    switch (type) {
        case TransactionType::Payment: return "PAYMENT";
        case TransactionType::Refund:  return "REFUND";
    }
    return "UNKNOWN";
}

std::string toString(TransactionStatus status) {
    switch (status) {
        case TransactionStatus::Pending:           return "PENDING";
        case TransactionStatus::Completed:         return "COMPLETED";
        case TransactionStatus::Failed:            return "FAILED";
        case TransactionStatus::Cancelled:         return "CANCELLED";
        case TransactionStatus::PartiallyRefunded: return "PARTIALLY_REFUNDED";
        case TransactionStatus::Refunded:          return "REFUNDED";
    }
    return "UNKNOWN";
}

std::string toString(PaymentMethod method) {
    switch (method) {
        case PaymentMethod::Card:   return "CARD";
        case PaymentMethod::Upi:    return "UPI";
        case PaymentMethod::Wallet: return "WALLET";
    }
    return "UNKNOWN";
}

TransactionType transactionTypeFromString(const std::string& text) {
    if (text == "PAYMENT") return TransactionType::Payment;
    if (text == "REFUND") return TransactionType::Refund;
    throw std::invalid_argument("unknown transaction type '" + text + "'");
}

TransactionStatus statusFromString(const std::string& text) {
    if (text == "PENDING") return TransactionStatus::Pending;
    if (text == "COMPLETED") return TransactionStatus::Completed;
    if (text == "FAILED") return TransactionStatus::Failed;
    if (text == "CANCELLED") return TransactionStatus::Cancelled;
    if (text == "PARTIALLY_REFUNDED") return TransactionStatus::PartiallyRefunded;
    if (text == "REFUNDED") return TransactionStatus::Refunded;
    throw std::invalid_argument("unknown status '" + text + "'");
}

PaymentMethod paymentMethodFromString(const std::string& text) {
    if (text == "CARD") return PaymentMethod::Card;
    if (text == "UPI") return PaymentMethod::Upi;
    if (text == "WALLET") return PaymentMethod::Wallet;
    throw std::invalid_argument("unknown payment method '" + text + "'");
}

}  // namespace paycore
