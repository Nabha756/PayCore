#include "paycore/FileRepository.h"

#include <filesystem>
#include <fstream>
#include <utility>

#include "paycore/Exceptions.h"
#include "paycore/Payment.h"
#include "paycore/Refund.h"
#include "paycore/TransactionManager.h"
#include "paycore/Utils.h"

namespace fs = std::filesystem;

namespace paycore {

namespace {

constexpr std::size_t kPaymentFields = 11;
constexpr std::size_t kRefundFields = 9;
const char* const kHeader = "# PayCore transaction data v1 (simulation only)";

}  // namespace

FileRepository::FileRepository(std::string path) : path_(std::move(path)) {
    if (path_.empty()) {
        throw PersistenceError("Data file path cannot be empty");
    }
}

void FileRepository::save(const TransactionManager& manager) const {
    const fs::path target(path_);
    const fs::path temp(path_ + ".tmp");

    std::error_code ec;
    if (target.has_parent_path()) {
        fs::create_directories(target.parent_path(), ec);
        if (ec) {
            throw PersistenceError("Cannot create folder " + target.parent_path().string() +
                                   ": " + ec.message());
        }
    }

    {
        // RAII: the ofstream closes the file automatically at the end of this block,
        // even if an exception is thrown while writing.
        std::ofstream out(temp, std::ios::trunc);
        if (!out) {
            throw PersistenceError("Cannot open " + temp.string() + " for writing");
        }
        out << kHeader << '\n';
        for (const Transaction* t : manager.history()) {
            out << t->serialize() << '\n';  // virtual call: Payment or Refund format
        }
        out.flush();
        if (!out) {
            throw PersistenceError("Error while writing " + temp.string());
        }
    }

    fs::rename(temp, target, ec);
    if (ec) {
        fs::remove(temp, ec);
        throw PersistenceError("Cannot replace " + target.string());
    }
}

LoadReport FileRepository::load(TransactionManager& manager) const {
    LoadReport report;
    manager.clear();

    std::ifstream in(path_);
    if (!in) {
        return report;  // missing file: start with an empty history
    }
    report.fileFound = true;

    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(in, line)) {
        ++lineNumber;
        const std::string trimmed = text::trim(line);
        if (trimmed.empty() || trimmed[0] == '#') continue;

        try {
            std::unique_ptr<Transaction> transaction = parseLine(trimmed);

            // A refund must point to a payment that was loaded earlier.
            if (const auto* refund = dynamic_cast<const Refund*>(transaction.get())) {
                const Transaction* original = manager.findById(refund->originalPaymentId());
                if (original == nullptr || original->type() != TransactionType::Payment) {
                    throw ValidationError("refund references unknown payment " +
                                          refund->originalPaymentId());
                }
            }

            manager.add(std::move(transaction));
            ++report.loaded;
        } catch (const PayCoreError& e) {
            report.errors.push_back("line " + std::to_string(lineNumber) + ": " + e.what());
        }
    }
    return report;
}

std::unique_ptr<Transaction> FileRepository::parseLine(const std::string& line) {
    const std::vector<std::string> f = text::split(line, '|');

    TransactionType type;
    try {
        type = transactionTypeFromString(f.at(0));
    } catch (const std::exception&) {
        throw ValidationError("unknown record type '" + f[0] + "'");
    }

    const std::size_t expected = type == TransactionType::Payment ? kPaymentFields : kRefundFields;
    if (f.size() != expected) {
        throw ValidationError("expected " + std::to_string(expected) + " fields but found " +
                              std::to_string(f.size()));
    }

    try {
        const std::string& id = f[1];
        const std::string& customerId = f[2];
        const Amount amount = text::toInt64(f[3], "amount");
        const TransactionStatus status = statusFromString(f[4]);
        const Timestamp created = text::toInt64(f[5], "createdAt");
        const Timestamp updated = text::toInt64(f[6], "updatedAt");

        if (type == TransactionType::Payment) {
            return std::make_unique<Payment>(id, customerId, amount, paymentMethodFromString(f[7]),
                                             f[8], status, created, updated,
                                             text::toInt64(f[9], "refundedAmount"), f[10]);
        }
        return std::make_unique<Refund>(id, customerId, amount, f[7], f[8], status, created,
                                        updated);
    } catch (const std::invalid_argument& e) {
        // thrown by statusFromString / paymentMethodFromString
        throw ValidationError(e.what());
    }
}

}  // namespace paycore
