#include <gtest/gtest.h>

#include "paycore/Exceptions.h"
#include "paycore/Payment.h"
#include "paycore/PaymentProcessor.h"

using namespace paycore;

namespace {

Payment pending() {
    return Payment("PAY-000001", "CUST-1", 5000, PaymentMethod::Upi, "ra***@okaxis", 1000);
}

}  // namespace

TEST(ProcessorTest, ZeroFailureRateAlwaysCompletes) {
    PaymentProcessor processor(0.0, 42);
    for (int i = 0; i < 20; ++i) {
        Payment p = pending();
        EXPECT_TRUE(processor.process(p));
        EXPECT_EQ(p.status(), TransactionStatus::Completed);
    }
}

TEST(ProcessorTest, FullFailureRateAlwaysFailsWithReason) {
    PaymentProcessor processor(1.0, 42);
    Payment p = pending();
    EXPECT_FALSE(processor.process(p));
    EXPECT_EQ(p.status(), TransactionStatus::Failed);
    EXPECT_FALSE(p.failureReason().empty());
}

TEST(ProcessorTest, SameSeedGivesSameResults) {
    PaymentProcessor a(0.5, 7);
    PaymentProcessor b(0.5, 7);
    for (int i = 0; i < 20; ++i) {
        Payment p1 = pending();
        Payment p2 = pending();
        EXPECT_EQ(a.process(p1), b.process(p2));
    }
}

TEST(ProcessorTest, CannotProcessTwice) {
    PaymentProcessor processor(0.0, 1);
    Payment p = pending();
    processor.process(p);
    EXPECT_THROW(processor.process(p), InvalidOperationError);
}

TEST(ProcessorTest, RejectsInvalidFailureRate) {
    EXPECT_THROW(PaymentProcessor(-0.1, 1), ValidationError);
    EXPECT_THROW(PaymentProcessor(1.5, 1), ValidationError);
}
