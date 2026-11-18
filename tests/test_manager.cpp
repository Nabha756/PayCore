#include <gtest/gtest.h>

#include <memory>

#include "paycore/Exceptions.h"
#include "paycore/Payment.h"
#include "paycore/PaymentProcessor.h"
#include "paycore/Refund.h"
#include "paycore/TransactionManager.h"

using namespace paycore;

namespace {

PaymentDetails upi() { return PaymentDetails{PaymentMethod::Upi, "ravi@okaxis", "", ""}; }
PaymentDetails cardDetails() {
    return PaymentDetails{PaymentMethod::Card, "4111111111111111", "12/99", "123"};
}

class ManagerTest : public ::testing::Test {
protected:
    TransactionManager manager;
    PaymentProcessor alwaysSucceeds{0.0, 1};
    PaymentProcessor alwaysFails{1.0, 1};
};

}  // namespace

// ---------- creation ----------

TEST_F(ManagerTest, CreatePaymentGeneratesUniqueIds) {
    const Payment& a = manager.createPayment("CUST-1", 1000, upi());
    const Payment& b = manager.createPayment("CUST-1", 2000, upi());
    EXPECT_EQ(a.id(), "PAY-000001");
    EXPECT_EQ(b.id(), "PAY-000002");
    EXPECT_NE(a.id(), b.id());
    EXPECT_EQ(manager.size(), 2u);
    EXPECT_GT(a.createdAt(), 0);
}

TEST_F(ManagerTest, CardNumberIsMaskedNotStored) {
    const Payment& p = manager.createPayment("CUST-1", 1000, cardDetails());
    EXPECT_EQ(p.maskedAccount(), "**** **** **** 1111");
    EXPECT_EQ(p.serialize().find("4111111111111111"), std::string::npos);
}

TEST_F(ManagerTest, InvalidInputCreatesNothing) {
    EXPECT_THROW(manager.createPayment("C", 1000, upi()), ValidationError);
    EXPECT_THROW(manager.createPayment("CUST-1", 0, upi()), ValidationError);
    EXPECT_THROW(manager.createPayment("CUST-1", 1000,
                                       PaymentDetails{PaymentMethod::Upi, "bad", "", ""}),
                 ValidationError);
    EXPECT_TRUE(manager.empty());
}

// ---------- processing ----------

TEST_F(ManagerTest, ProcessSuccessAndFailure) {
    const std::string ok = manager.createPayment("CUST-1", 1000, upi()).id();
    const std::string bad = manager.createPayment("CUST-1", 1000, upi()).id();

    EXPECT_TRUE(manager.processPayment(ok, alwaysSucceeds));
    EXPECT_FALSE(manager.processPayment(bad, alwaysFails));
    EXPECT_EQ(manager.getById(ok).status(), TransactionStatus::Completed);
    EXPECT_EQ(manager.getById(bad).status(), TransactionStatus::Failed);
}

TEST_F(ManagerTest, ProcessUnknownIdThrows) {
    EXPECT_THROW(manager.processPayment("PAY-999999", alwaysSucceeds), NotFoundError);
}

// ---------- cancellation ----------

TEST_F(ManagerTest, CancelPendingPayment) {
    const std::string id = manager.createPayment("CUST-1", 1000, upi()).id();
    manager.cancelPayment(id);
    EXPECT_EQ(manager.getById(id).status(), TransactionStatus::Cancelled);
    EXPECT_THROW(manager.processPayment(id, alwaysSucceeds), InvalidOperationError);
}

TEST_F(ManagerTest, CannotCancelCompletedOrTwice) {
    const std::string id = manager.createPayment("CUST-1", 1000, upi()).id();
    manager.processPayment(id, alwaysSucceeds);
    EXPECT_THROW(manager.cancelPayment(id), InvalidOperationError);

    const std::string id2 = manager.createPayment("CUST-1", 1000, upi()).id();
    manager.cancelPayment(id2);
    EXPECT_THROW(manager.cancelPayment(id2), InvalidOperationError);
}

// ---------- refunds ----------

TEST_F(ManagerTest, FullAndPartialRefunds) {
    const std::string id = manager.createPayment("CUST-1", 10000, upi()).id();
    manager.processPayment(id, alwaysSucceeds);

    const Refund& r1 = manager.refundPayment(id, 4000, "Partial return");
    EXPECT_EQ(r1.id(), "REF-000002");
    EXPECT_EQ(r1.customerId(), "CUST-1");
    EXPECT_EQ(manager.getById(id).status(), TransactionStatus::PartiallyRefunded);

    manager.refundPayment(id, 6000, "Rest returned");
    EXPECT_EQ(manager.getById(id).status(), TransactionStatus::Refunded);
    EXPECT_THROW(manager.refundPayment(id, 1, "Again"), InvalidOperationError);
    EXPECT_EQ(manager.size(), 3u);
}

TEST_F(ManagerTest, InvalidRefundsLeaveNoTrace) {
    const std::string id = manager.createPayment("CUST-1", 10000, upi()).id();

    EXPECT_THROW(manager.refundPayment(id, 100, "Not processed"), InvalidOperationError);

    manager.processPayment(id, alwaysSucceeds);
    EXPECT_THROW(manager.refundPayment(id, 10001, "Too much"), ValidationError);
    EXPECT_THROW(manager.refundPayment(id, 100, ""), ValidationError);
    EXPECT_THROW(manager.refundPayment(id, 100, "bad|reason"), ValidationError);
    EXPECT_THROW(manager.refundPayment("PAY-999999", 100, "Missing"), NotFoundError);

    EXPECT_EQ(manager.size(), 1u);
    EXPECT_EQ(manager.getById(id).status(), TransactionStatus::Completed);
}

TEST_F(ManagerTest, CannotRefundARefund) {
    const std::string id = manager.createPayment("CUST-1", 10000, upi()).id();
    manager.processPayment(id, alwaysSucceeds);
    const std::string refundId = manager.refundPayment(id, 100, "Partial").id();
    EXPECT_THROW(manager.refundPayment(refundId, 50, "Refund of refund"), InvalidOperationError);
    EXPECT_THROW(manager.cancelPayment(refundId), InvalidOperationError);
}

// ---------- search, filter, sort ----------

TEST_F(ManagerTest, SearchByIdAndCustomer) {
    manager.createPayment("CUST-A", 1000, upi());
    manager.createPayment("CUST-B", 2000, upi());
    manager.createPayment("CUST-A", 3000, upi());

    EXPECT_NE(manager.findById("PAY-000002"), nullptr);
    EXPECT_EQ(manager.findById("PAY-000099"), nullptr);
    EXPECT_THROW(manager.getById("PAY-000099"), NotFoundError);

    const auto forA = manager.findByCustomer("CUST-A");
    ASSERT_EQ(forA.size(), 2u);
    EXPECT_EQ(forA[0]->id(), "PAY-000001");
    EXPECT_EQ(forA[1]->id(), "PAY-000003");
    EXPECT_TRUE(manager.findByCustomer("NOBODY").empty());
}

TEST_F(ManagerTest, FilterByStatus) {
    const std::string a = manager.createPayment("CUST-1", 1000, upi()).id();
    const std::string b = manager.createPayment("CUST-1", 1000, upi()).id();
    manager.createPayment("CUST-1", 1000, upi());
    manager.processPayment(a, alwaysSucceeds);
    manager.cancelPayment(b);

    EXPECT_EQ(manager.filterByStatus(TransactionStatus::Completed).size(), 1u);
    EXPECT_EQ(manager.filterByStatus(TransactionStatus::Cancelled).size(), 1u);
    EXPECT_EQ(manager.filterByStatus(TransactionStatus::Pending).size(), 1u);
    EXPECT_TRUE(manager.filterByStatus(TransactionStatus::Refunded).empty());
}

TEST_F(ManagerTest, SortByAmountAndTimestamp) {
    manager.add(std::make_unique<Payment>("PAY-000001", "C-1", 3000, PaymentMethod::Upi, "x", 300));
    manager.add(std::make_unique<Payment>("PAY-000002", "C-1", 1000, PaymentMethod::Upi, "x", 100));
    manager.add(std::make_unique<Payment>("PAY-000003", "C-1", 2000, PaymentMethod::Upi, "x", 200));

    auto asc = manager.sortedByAmount(true);
    EXPECT_EQ(asc[0]->amount(), 1000);
    EXPECT_EQ(asc[2]->amount(), 3000);

    auto desc = manager.sortedByAmount(false);
    EXPECT_EQ(desc[0]->amount(), 3000);

    auto newest = manager.sortedByTimestamp(true);
    EXPECT_EQ(newest[0]->id(), "PAY-000001");
    auto oldest = manager.sortedByTimestamp(false);
    EXPECT_EQ(oldest[0]->id(), "PAY-000002");

    // history keeps insertion order regardless of sorting
    EXPECT_EQ(manager.history()[0]->id(), "PAY-000001");
}

TEST_F(ManagerTest, EmptyManagerQueriesAreSafe) {
    EXPECT_TRUE(manager.history().empty());
    EXPECT_TRUE(manager.sortedByAmount().empty());
    EXPECT_TRUE(manager.sortedByTimestamp().empty());
    EXPECT_TRUE(manager.filterByStatus(TransactionStatus::Pending).empty());
}

TEST_F(ManagerTest, DuplicateIdRejectedAndIdsContinueAfterLoad) {
    manager.add(std::make_unique<Payment>("PAY-000041", "C-1", 100, PaymentMethod::Upi, "x", 1));
    EXPECT_THROW(
        manager.add(std::make_unique<Payment>("PAY-000041", "C-1", 100, PaymentMethod::Upi, "x", 1)),
        InvalidOperationError);
    EXPECT_EQ(manager.createPayment("CUST-1", 100, upi()).id(), "PAY-000042");
}
