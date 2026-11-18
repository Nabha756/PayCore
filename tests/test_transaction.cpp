#include <gtest/gtest.h>

#include <memory>

#include "paycore/Exceptions.h"
#include "paycore/Payment.h"
#include "paycore/Refund.h"

using namespace paycore;

namespace {

Payment makePending(Amount amount = 10000) {
    return Payment("PAY-000001", "CUST-1", amount, PaymentMethod::Card, "**** **** **** 1111", 1000);
}

}  // namespace

TEST(PaymentTest, NewPaymentIsPending) {
    Payment p = makePending();
    EXPECT_EQ(p.status(), TransactionStatus::Pending);
    EXPECT_EQ(p.type(), TransactionType::Payment);
    EXPECT_EQ(p.refundedAmount(), 0);
    EXPECT_FALSE(p.canBeRefunded());
}

TEST(PaymentTest, ConstructorRejectsBadData) {
    EXPECT_THROW(Payment("", "CUST-1", 100, PaymentMethod::Card, "x", 1), ValidationError);
    EXPECT_THROW(Payment("PAY-1", "", 100, PaymentMethod::Card, "x", 1), ValidationError);
    EXPECT_THROW(Payment("PAY-1", "CUST-1", 0, PaymentMethod::Card, "x", 1), ValidationError);
    EXPECT_THROW(Payment("PAY-1", "CUST-1", -5, PaymentMethod::Card, "x", 1), ValidationError);
}

TEST(PaymentTest, CompleteThenRefundInParts) {
    Payment p = makePending(10000);
    p.markCompleted();
    EXPECT_TRUE(p.canBeRefunded());

    p.applyRefund(3000);
    EXPECT_EQ(p.status(), TransactionStatus::PartiallyRefunded);
    EXPECT_EQ(p.refundableAmount(), 7000);

    p.applyRefund(7000);
    EXPECT_EQ(p.status(), TransactionStatus::Refunded);
    EXPECT_EQ(p.refundableAmount(), 0);
    EXPECT_FALSE(p.canBeRefunded());
}

TEST(PaymentTest, RefundLargerThanRemainingIsRejected) {
    Payment p = makePending(10000);
    p.markCompleted();
    EXPECT_THROW(p.applyRefund(10001), ValidationError);
    EXPECT_THROW(p.applyRefund(0), ValidationError);
    EXPECT_EQ(p.status(), TransactionStatus::Completed);  // unchanged after failure
    EXPECT_EQ(p.refundedAmount(), 0);
}

TEST(PaymentTest, CannotRefundPendingFailedOrCancelled) {
    Payment pending = makePending();
    EXPECT_THROW(pending.applyRefund(100), InvalidOperationError);

    Payment failed = makePending();
    failed.markFailed("declined");
    EXPECT_THROW(failed.applyRefund(100), InvalidOperationError);
    EXPECT_EQ(failed.failureReason(), "declined");

    Payment cancelled = makePending();
    cancelled.cancel();
    EXPECT_THROW(cancelled.applyRefund(100), InvalidOperationError);
}

TEST(PaymentTest, InvalidStatusTransitionsThrow) {
    Payment p = makePending();
    p.markCompleted();
    EXPECT_THROW(p.cancel(), InvalidOperationError);
    EXPECT_THROW(p.markCompleted(), InvalidOperationError);
    EXPECT_THROW(p.markFailed("x"), InvalidOperationError);

    Payment c = makePending();
    c.cancel();
    EXPECT_THROW(c.cancel(), InvalidOperationError);
    EXPECT_THROW(c.markCompleted(), InvalidOperationError);
}

TEST(PaymentTest, RestoreConstructorChecksConsistency) {
    // refunded amount larger than payment
    EXPECT_THROW(Payment("PAY-1", "C-1", 100, PaymentMethod::Upi, "x", TransactionStatus::Refunded,
                         1, 2, 200, ""),
                 ValidationError);
    // refunds recorded but status says completed
    EXPECT_THROW(Payment("PAY-1", "C-1", 100, PaymentMethod::Upi, "x", TransactionStatus::Completed,
                         1, 2, 50, ""),
                 ValidationError);
    // updated before created
    EXPECT_THROW(Payment("PAY-1", "C-1", 100, PaymentMethod::Upi, "x", TransactionStatus::Pending,
                         5, 4, 0, ""),
                 ValidationError);
    EXPECT_NO_THROW(Payment("PAY-1", "C-1", 100, PaymentMethod::Upi, "x",
                            TransactionStatus::PartiallyRefunded, 1, 2, 40, ""));
}

TEST(RefundTest, RefundIsCompletedAndFinal) {
    Refund r("REF-000002", "CUST-1", 500, "PAY-000001", "Damaged item", 1000);
    EXPECT_EQ(r.status(), TransactionStatus::Completed);
    EXPECT_EQ(r.type(), TransactionType::Refund);
    EXPECT_EQ(r.originalPaymentId(), "PAY-000001");
    EXPECT_THROW(Refund("REF-1", "C-1", 500, "", "x", 1), ValidationError);
}

TEST(PolymorphismTest, BasePointerCallsDerivedBehaviour) {
    std::unique_ptr<Transaction> a =
        std::make_unique<Payment>("PAY-000001", "C-1", 100, PaymentMethod::Upi, "ra***@okaxis", 1);
    std::unique_ptr<Transaction> b =
        std::make_unique<Refund>("REF-000002", "C-1", 50, "PAY-000001", "Late delivery", 2);

    EXPECT_EQ(a->type(), TransactionType::Payment);
    EXPECT_EQ(b->type(), TransactionType::Refund);
    EXPECT_EQ(a->serialize().rfind("PAYMENT|", 0), 0u);
    EXPECT_EQ(b->serialize().rfind("REFUND|", 0), 0u);
    EXPECT_NE(a->details().find("Method"), std::string::npos);
    EXPECT_NE(b->details().find("Late delivery"), std::string::npos);
}
