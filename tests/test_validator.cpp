#include <gtest/gtest.h>

#include "paycore/Exceptions.h"
#include "paycore/PaymentValidator.h"
#include "paycore/Utils.h"

using namespace paycore;

namespace {

PaymentDetails card(const std::string& number, const std::string& expiry, const std::string& cvv) {
    return PaymentDetails{PaymentMethod::Card, number, expiry, cvv};
}

}  // namespace

// ---------- money parsing ----------

TEST(MoneyTest, ParsesWholeAndDecimalAmounts) {
    EXPECT_EQ(money::parse("1500"), 150000);
    EXPECT_EQ(money::parse("1500.5"), 150050);
    EXPECT_EQ(money::parse("1500.05"), 150005);
    EXPECT_EQ(money::parse("  0.01 "), 1);
}

TEST(MoneyTest, RejectsInvalidAmounts) {
    EXPECT_THROW(money::parse(""), ValidationError);
    EXPECT_THROW(money::parse("abc"), ValidationError);
    EXPECT_THROW(money::parse("-50"), ValidationError);
    EXPECT_THROW(money::parse("10.999"), ValidationError);
    EXPECT_THROW(money::parse("10."), ValidationError);
    EXPECT_THROW(money::parse(".50"), ValidationError);
    EXPECT_THROW(money::parse("1e5"), ValidationError);
    EXPECT_THROW(money::parse("9999999999999"), ValidationError);  // overflow guard
}

TEST(MoneyTest, FormatsPaise) {
    EXPECT_EQ(money::format(150005), "1500.05");
    EXPECT_EQ(money::format(1), "0.01");
    EXPECT_EQ(money::format(0), "0.00");
}

// ---------- amount and customer ----------

TEST(ValidatorTest, AmountLimits) {
    PaymentValidator v;
    EXPECT_NO_THROW(v.validateAmount(1));
    EXPECT_NO_THROW(v.validateAmount(PaymentValidator::kMaxAmount));
    EXPECT_THROW(v.validateAmount(0), ValidationError);
    EXPECT_THROW(v.validateAmount(-100), ValidationError);
    EXPECT_THROW(v.validateAmount(PaymentValidator::kMaxAmount + 1), ValidationError);
}

TEST(ValidatorTest, CustomerIdFormat) {
    PaymentValidator v;
    EXPECT_NO_THROW(v.validateCustomerId("CUST-1001"));
    EXPECT_NO_THROW(v.validateCustomerId("abc"));
    EXPECT_THROW(v.validateCustomerId("ab"), ValidationError);
    EXPECT_THROW(v.validateCustomerId("CUST 1001"), ValidationError);
    EXPECT_THROW(v.validateCustomerId("CUST|1001"), ValidationError);
    EXPECT_THROW(v.validateCustomerId(std::string(21, 'A')), ValidationError);
}

// ---------- cards ----------

TEST(ValidatorTest, LuhnChecksum) {
    EXPECT_TRUE(PaymentValidator::passesLuhn("4111111111111111"));
    EXPECT_TRUE(PaymentValidator::passesLuhn("5555555555554444"));
    EXPECT_FALSE(PaymentValidator::passesLuhn("4111111111111112"));
    EXPECT_FALSE(PaymentValidator::passesLuhn("41111x1111111111"));
    EXPECT_FALSE(PaymentValidator::passesLuhn(""));
}

TEST(ValidatorTest, ValidCardPasses) {
    PaymentValidator v;
    EXPECT_NO_THROW(v.validate(card("4111 1111 1111 1111", "12/30", "123"), 2026, 11));
    EXPECT_NO_THROW(v.validate(card("4111-1111-1111-1111", "11/26", "1234"), 2026, 11));
}

TEST(ValidatorTest, InvalidCardNumberFails) {
    PaymentValidator v;
    EXPECT_THROW(v.validateCardNumber("4111111111111112"), ValidationError);  // bad checksum
    EXPECT_THROW(v.validateCardNumber("411111"), ValidationError);            // too short
    EXPECT_THROW(v.validateCardNumber("4111abcd11111111"), ValidationError);  // letters
}

TEST(ValidatorTest, ExpiryRules) {
    PaymentValidator v;
    EXPECT_NO_THROW(v.validateExpiry("11/26", 2026, 11));  // valid until end of month
    EXPECT_THROW(v.validateExpiry("10/26", 2026, 11), ValidationError);
    EXPECT_THROW(v.validateExpiry("12/25", 2026, 1), ValidationError);
    EXPECT_THROW(v.validateExpiry("13/30", 2026, 1), ValidationError);
    EXPECT_THROW(v.validateExpiry("00/30", 2026, 1), ValidationError);
    EXPECT_THROW(v.validateExpiry("1/30", 2026, 1), ValidationError);
    EXPECT_THROW(v.validateExpiry("12/2030", 2026, 1), ValidationError);
}

TEST(ValidatorTest, CvvRules) {
    PaymentValidator v;
    EXPECT_NO_THROW(v.validateCvv("123"));
    EXPECT_NO_THROW(v.validateCvv("1234"));
    EXPECT_THROW(v.validateCvv("12"), ValidationError);
    EXPECT_THROW(v.validateCvv("12345"), ValidationError);
    EXPECT_THROW(v.validateCvv("12a"), ValidationError);
}

// ---------- UPI and wallet ----------

TEST(ValidatorTest, UpiRules) {
    PaymentValidator v;
    EXPECT_NO_THROW(v.validate(PaymentDetails{PaymentMethod::Upi, "ravi.k@okaxis", "", ""}, 2026, 1));
    EXPECT_THROW(v.validateUpiId("raviokaxis"), ValidationError);
    EXPECT_THROW(v.validateUpiId("@okaxis"), ValidationError);
    EXPECT_THROW(v.validateUpiId("ravi@"), ValidationError);
    EXPECT_THROW(v.validateUpiId("ravi@ok1"), ValidationError);
}

TEST(ValidatorTest, WalletRules) {
    PaymentValidator v;
    EXPECT_NO_THROW(v.validateWalletNumber("9876543210"));
    EXPECT_THROW(v.validateWalletNumber("1234567890"), ValidationError);  // must start 6-9
    EXPECT_THROW(v.validateWalletNumber("98765"), ValidationError);
    EXPECT_THROW(v.validateWalletNumber("98765abcde"), ValidationError);
}

TEST(ValidatorTest, TextRules) {
    PaymentValidator v;
    EXPECT_NO_THROW(v.validateText("Item damaged", "Reason"));
    EXPECT_THROW(v.validateText("   ", "Reason"), ValidationError);
    EXPECT_THROW(v.validateText("a|b", "Reason"), ValidationError);
    EXPECT_THROW(v.validateText("line\nbreak", "Reason"), ValidationError);
    EXPECT_THROW(v.validateText(std::string(101, 'x'), "Reason"), ValidationError);
}

TEST(ValidatorTest, MaskingHidesSensitiveData) {
    EXPECT_EQ(PaymentValidator::mask(card("4111 1111 1111 1234", "12/30", "123")),
              "**** **** **** 1234");
    EXPECT_EQ(PaymentValidator::mask(PaymentDetails{PaymentMethod::Upi, "ravi.k@okaxis", "", ""}),
              "ra***@okaxis");
    EXPECT_EQ(PaymentValidator::mask(PaymentDetails{PaymentMethod::Wallet, "9876543210", "", ""}),
              "******3210");
}
