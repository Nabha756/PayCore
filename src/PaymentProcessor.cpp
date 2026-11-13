#include "paycore/PaymentProcessor.h"

#include "paycore/Exceptions.h"
#include "paycore/Payment.h"

namespace paycore {

PaymentProcessor::PaymentProcessor(double failureRate, unsigned int seed)
    : rng_(seed),
      failureRate_(failureRate),
      declineReasons_{"Insufficient funds (simulated)",
                      "Bank server timeout (simulated)",
                      "Declined by issuing bank (simulated)",
                      "Suspected fraud check failed (simulated)"} {
    if (failureRate < 0.0 || failureRate > 1.0) {
        throw ValidationError("Failure rate must be between 0.0 and 1.0");
    }
}

bool PaymentProcessor::process(Payment& payment) {
    if (payment.status() != TransactionStatus::Pending) {
        throw InvalidOperationError("Payment " + payment.id() + " is " +
                                    toString(payment.status()) + "; only PENDING payments can be processed");
    }

    std::bernoulli_distribution fails(failureRate_);
    if (fails(rng_)) {
        std::uniform_int_distribution<std::size_t> pick(0, declineReasons_.size() - 1);
        payment.markFailed(declineReasons_[pick(rng_)]);
        return false;
    }

    payment.markCompleted();
    return true;
}

}  // namespace paycore
