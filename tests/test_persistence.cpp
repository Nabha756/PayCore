#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "paycore/Exceptions.h"
#include "paycore/FileRepository.h"
#include "paycore/Payment.h"
#include "paycore/PaymentProcessor.h"
#include "paycore/Refund.h"
#include "paycore/TransactionManager.h"

using namespace paycore;
namespace fs = std::filesystem;

namespace {

// Gives every test its own temporary folder and removes it afterwards (RAII).
class PersistenceTest : public ::testing::Test {
protected:
    void SetUp() override {
        const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
        dir = fs::temp_directory_path() / (std::string("paycore_test_") + info->name());
        fs::remove_all(dir);
        fs::create_directories(dir);
    }
    void TearDown() override { fs::remove_all(dir); }

    std::string file(const std::string& name) const { return (dir / name).string(); }

    void writeFile(const std::string& path, const std::string& content) const {
        std::ofstream out(path);
        out << content;
    }

    fs::path dir;
};

PaymentDetails upi() { return PaymentDetails{PaymentMethod::Upi, "ravi@okaxis", "", ""}; }

}  // namespace

TEST_F(PersistenceTest, SaveAndLoadRoundTrip) {
    TransactionManager original;
    PaymentProcessor ok(0.0, 1);
    PaymentProcessor fail(1.0, 1);

    const std::string p1 = original.createPayment("CUST-1", 10000, upi()).id();
    const std::string p2 = original.createPayment("CUST-2", 5000, upi()).id();
    const std::string p3 = original.createPayment("CUST-1", 2500, upi()).id();
    original.processPayment(p1, ok);
    original.processPayment(p2, fail);
    original.cancelPayment(p3);
    original.refundPayment(p1, 4000, "Partial return");

    FileRepository repo(file("data.csv"));
    repo.save(original);

    TransactionManager loaded;
    const LoadReport report = repo.load(loaded);
    EXPECT_TRUE(report.fileFound);
    EXPECT_TRUE(report.errors.empty());
    ASSERT_EQ(report.loaded, 4u);
    ASSERT_EQ(loaded.size(), original.size());

    // Every record is saved back identically.
    const auto a = original.history();
    const auto b = loaded.history();
    for (std::size_t i = 0; i < a.size(); ++i) {
        EXPECT_EQ(a[i]->serialize(), b[i]->serialize());
    }

    const auto& payment = dynamic_cast<const Payment&>(loaded.getById(p1));
    EXPECT_EQ(payment.status(), TransactionStatus::PartiallyRefunded);
    EXPECT_EQ(payment.refundedAmount(), 4000);
    EXPECT_FALSE(dynamic_cast<const Payment&>(loaded.getById(p2)).failureReason().empty());

    // New IDs continue after the loaded ones.
    EXPECT_EQ(loaded.createPayment("CUST-3", 100, upi()).id(), "PAY-000005");
}

TEST_F(PersistenceTest, MissingFileGivesEmptyHistory) {
    TransactionManager manager;
    FileRepository repo(file("does_not_exist.csv"));
    const LoadReport report = repo.load(manager);
    EXPECT_FALSE(report.fileFound);
    EXPECT_EQ(report.loaded, 0u);
    EXPECT_TRUE(manager.empty());
}

TEST_F(PersistenceTest, SaveCreatesMissingFolders) {
    TransactionManager manager;
    manager.createPayment("CUST-1", 100, upi());
    FileRepository repo(file("nested/folder/data.csv"));
    repo.save(manager);
    EXPECT_TRUE(fs::exists(file("nested/folder/data.csv")));
    EXPECT_FALSE(fs::exists(file("nested/folder/data.csv.tmp")));
}

TEST_F(PersistenceTest, CorruptedLinesAreSkippedValidOnesKept) {
    const std::string path = file("corrupt.csv");
    writeFile(path,
              "# comment line\n"
              "\n"
              "PAYMENT|PAY-000001|CUST-1|10000|COMPLETED|100|200|UPI|ra***@okaxis|0|\n"
              "PAYMENT|PAY-000002|CUST-1|abc|COMPLETED|100|200|UPI|x|0|\n"           // bad amount
              "PAYMENT|PAY-000003|CUST-1|500|DONE|100|200|UPI|x|0|\n"                // bad status
              "PAYMENT|PAY-000004|CUST-1|500|COMPLETED|100\n"                         // too few fields
              "BITCOIN|PAY-000005|CUST-1|500|COMPLETED|100|200|UPI|x|0|\n"           // bad type
              "PAYMENT|PAY-000006|CUST-1|500|COMPLETED|100|200|CHEQUE|x|0|\n"        // bad method
              "PAYMENT|PAY-000007|CUST-1|500|REFUNDED|100|200|UPI|x|100|\n"          // inconsistent
              "REFUND|REF-000008|CUST-1|100|COMPLETED|300|300|PAY-999999|reason\n"   // orphan refund
              "PAYMENT|PAY-000001|CUST-1|10000|COMPLETED|100|200|UPI|x|0|\n"         // duplicate ID
              "garbage line without separators\n"
              "PAYMENT|PAY-000009|CUST-2|2500|PENDING|150|150|WALLET|******3210|0|\n");

    TransactionManager manager;
    const LoadReport report = FileRepository(path).load(manager);

    EXPECT_TRUE(report.fileFound);
    EXPECT_EQ(report.loaded, 2u);
    EXPECT_EQ(report.errors.size(), 9u);
    EXPECT_NE(manager.findById("PAY-000001"), nullptr);
    EXPECT_NE(manager.findById("PAY-000009"), nullptr);
    EXPECT_EQ(manager.findById("PAY-000002"), nullptr);
    // error messages mention the line number
    EXPECT_EQ(report.errors[0].rfind("line 4:", 0), 0u);
}

TEST_F(PersistenceTest, EmptyFileLoadsNothing) {
    const std::string path = file("empty.csv");
    writeFile(path, "");
    TransactionManager manager;
    const LoadReport report = FileRepository(path).load(manager);
    EXPECT_TRUE(report.fileFound);
    EXPECT_EQ(report.loaded, 0u);
    EXPECT_TRUE(report.errors.empty());
}

TEST_F(PersistenceTest, LoadReplacesExistingData) {
    const std::string path = file("data.csv");
    writeFile(path, "PAYMENT|PAY-000001|CUST-1|100|PENDING|1|1|UPI|x|0|\n");

    TransactionManager manager;
    manager.createPayment("CUST-9", 999, upi());
    manager.createPayment("CUST-9", 999, upi());
    FileRepository(path).load(manager);
    EXPECT_EQ(manager.size(), 1u);
    EXPECT_TRUE(manager.findByCustomer("CUST-9").empty());
}

TEST_F(PersistenceTest, SaveToInvalidLocationThrows) {
    // A path whose "parent folder" is actually a file cannot be created.
    const std::string blocker = file("blocker");
    writeFile(blocker, "x");
    TransactionManager manager;
    FileRepository repo(blocker + "/data.csv");
    EXPECT_THROW(repo.save(manager), PersistenceError);
}

TEST(FileRepositoryTest, EmptyPathRejected) {
    EXPECT_THROW(FileRepository(""), PersistenceError);
}
