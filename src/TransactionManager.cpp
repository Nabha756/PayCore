#include "paycore/TransactionManager.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "paycore/Exceptions.h"
#include "paycore/Payment.h"
#include "paycore/PaymentProcessor.h"
#include "paycore/Refund.h"
#include "paycore/Utils.h"

namespace paycore {

Payment& TransactionManager::createPayment(const std::string& customerId, Amount amount,
                                           const PaymentDetails& details) {
    // Validate everything first so a bad request leaves no partial state behind.
    validator_.validateCustomerId(customerId);
    validator_.validateAmount(amount);
    validator_.validate(details);

    auto payment = std::make_unique<Payment>(ids_.next("PAY"), customerId, amount,
                                             details.method, PaymentValidator::mask(details),
                                             timeutil::now());
    Payment& ref = *payment;
    add(std::move(payment));
    return ref;
}

bool TransactionManager::processPayment(const std::string& paymentId, PaymentProcessor& processor) {
    return processor.process(getPayment(paymentId));
}

void TransactionManager::cancelPayment(const std::string& paymentId) {
    Payment& payment = getPayment(paymentId);
    if (payment.status() != TransactionStatus::Pending) {
        throw InvalidOperationError("Only PENDING payments can be cancelled; " + paymentId +
                                    " is " + toString(payment.status()));
    }
    payment.cancel();
}

Refund& TransactionManager::refundPayment(const std::string& paymentId, Amount amount,
                                          const std::string& reason) {
    validator_.validateText(reason, "Refund reason");
    Payment& payment = getPayment(paymentId);

    // applyRefund checks status and amount and throws before changing anything.
    payment.applyRefund(amount);

    auto refund = std::make_unique<Refund>(ids_.next("REF"), payment.customerId(), amount,
                                           payment.id(), reason, timeutil::now());
    Refund& ref = *refund;
    add(std::move(refund));
    return ref;
}

void TransactionManager::add(std::unique_ptr<Transaction> transaction) {
    if (!transaction) {
        throw InvalidOperationError("Cannot add an empty transaction");
    }
    if (byId_.count(transaction->id()) != 0) {
        throw InvalidOperationError("Duplicate transaction ID " + transaction->id());
    }
    ids_.observe(transaction->id());
    transactions_.push_back(std::move(transaction));
    index(*transactions_.back());
}

void TransactionManager::index(Transaction& transaction) {
    byId_[transaction.id()] = &transaction;
    byCustomer_[transaction.customerId()].insert(transaction.id());
}

void TransactionManager::clear() {
    transactions_.clear();
    byId_.clear();
    byCustomer_.clear();
    ids_.reset();
}

const Transaction* TransactionManager::findById(const std::string& id) const {
    const auto it = byId_.find(id);
    return it == byId_.end() ? nullptr : it->second;
}

const Transaction& TransactionManager::getById(const std::string& id) const {
    const Transaction* found = findById(id);
    if (found == nullptr) {
        throw NotFoundError("No transaction with ID " + id);
    }
    return *found;
}

Payment& TransactionManager::getPayment(const std::string& id) {
    const auto it = byId_.find(id);
    if (it == byId_.end()) {
        throw NotFoundError("No transaction with ID " + id);
    }
    // dynamic_cast checks at runtime that this transaction really is a Payment.
    auto* payment = dynamic_cast<Payment*>(it->second);
    if (payment == nullptr) {
        throw InvalidOperationError(id + " is not a payment");
    }
    return *payment;
}

std::vector<const Transaction*> TransactionManager::findByCustomer(const std::string& customerId) const {
    std::vector<const Transaction*> result;
    const auto it = byCustomer_.find(customerId);
    if (it == byCustomer_.end()) return result;

    for (const std::string& id : it->second) {
        result.push_back(byId_.at(id));
    }
    return result;
}

std::vector<const Transaction*> TransactionManager::filterByStatus(TransactionStatus status) const {
    std::vector<const Transaction*> all = history();
    std::vector<const Transaction*> result;
    std::copy_if(all.begin(), all.end(), std::back_inserter(result),
                 [status](const Transaction* t) { return t->status() == status; });
    return result;
}

std::vector<const Transaction*> TransactionManager::history() const {
    std::vector<const Transaction*> result;
    result.reserve(transactions_.size());
    std::transform(transactions_.begin(), transactions_.end(), std::back_inserter(result),
                   [](const std::unique_ptr<Transaction>& t) { return t.get(); });
    return result;
}

std::vector<const Transaction*> TransactionManager::sortedByAmount(bool ascending) const {
    std::vector<const Transaction*> result = history();
    // stable_sort keeps creation order for equal amounts, so output is predictable.
    std::stable_sort(result.begin(), result.end(),
                     [ascending](const Transaction* a, const Transaction* b) {
                         return ascending ? a->amount() < b->amount() : a->amount() > b->amount();
                     });
    return result;
}

std::vector<const Transaction*> TransactionManager::sortedByTimestamp(bool newestFirst) const {
    std::vector<const Transaction*> result = history();
    std::stable_sort(result.begin(), result.end(),
                     [newestFirst](const Transaction* a, const Transaction* b) {
                         return newestFirst ? a->createdAt() > b->createdAt()
                                            : a->createdAt() < b->createdAt();
                     });
    return result;
}

}  // namespace paycore
