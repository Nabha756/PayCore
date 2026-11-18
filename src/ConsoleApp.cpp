#include "paycore/ConsoleApp.h"

#include <iomanip>
#include <limits>

#include "paycore/Exceptions.h"
#include "paycore/FileRepository.h"
#include "paycore/Payment.h"
#include "paycore/PaymentProcessor.h"
#include "paycore/Refund.h"
#include "paycore/TransactionManager.h"
#include "paycore/Utils.h"

namespace paycore {

ConsoleApp::ConsoleApp(TransactionManager& manager, PaymentProcessor& processor,
                       const FileRepository& repository, std::istream& in, std::ostream& out)
    : manager_(manager), processor_(processor), repository_(repository), in_(in), out_(out) {}

void ConsoleApp::run() {
    out_ << "\n=== PayCore - Payment Processing Simulator ===\n"
         << "(All payments are simulated. Do not enter real card details.)\n";

    try {
        while (true) {
            printMenu();
            const int choice = readChoice("Choose an option (1-9): ", 1, 9);
            try {
                switch (choice) {
                    case 1: createPayment(); break;
                    case 2: processPayment(); break;
                    case 3: viewHistory(); break;
                    case 4: searchTransaction(); break;
                    case 5: filterTransactions(); break;
                    case 6: refundPayment(); break;
                    case 7: cancelPayment(); break;
                    case 8: viewDetails(); break;
                    case 9:
                        if (save()) {
                            out_ << "Goodbye!\n";
                            return;
                        }
                        break;
                }
            } catch (const PayCoreError& e) {
                // Business errors are reported and the menu continues.
                out_ << "Error: " << e.what() << '\n';
            }
        }
    } catch (const InputClosed&) {
        out_ << "\nInput closed. Saving before exit...\n";
        save();
    }
}

void ConsoleApp::printMenu() const {
    out_ << "\n------------------------------\n"
         << " 1. Create payment\n"
         << " 2. Process payment\n"
         << " 3. View transaction history\n"
         << " 4. Search transaction\n"
         << " 5. Filter transactions\n"
         << " 6. Refund payment\n"
         << " 7. Cancel payment\n"
         << " 8. View transaction details\n"
         << " 9. Save and exit\n"
         << "------------------------------\n";
}

void ConsoleApp::createPayment() {
    const std::string customerId = readNonEmpty("Customer ID (e.g. CUST-1001): ");
    const Amount amount = readAmount("Amount in INR (e.g. 1499.50): ");

    out_ << "Payment method: 1. Card  2. UPI  3. Wallet\n";
    PaymentDetails details;
    switch (readChoice("Method (1-3): ", 1, 3)) {
        case 1:
            details.method = PaymentMethod::Card;
            details.account = readNonEmpty("Card number (test card: 4111 1111 1111 1111): ");
            details.expiry = readNonEmpty("Expiry (MM/YY): ");
            details.cvv = readNonEmpty("CVV: ");
            break;
        case 2:
            details.method = PaymentMethod::Upi;
            details.account = readNonEmpty("UPI ID (e.g. ravi@okaxis): ");
            break;
        default:
            details.method = PaymentMethod::Wallet;
            details.account = readNonEmpty("Wallet mobile number: ");
            break;
    }

    const Payment& payment = manager_.createPayment(customerId, amount, details);
    out_ << "Payment created: " << payment.id() << " (status " << toString(payment.status())
         << ")\n";
}

void ConsoleApp::processPayment() {
    const std::string id = readNonEmpty("Payment ID to process: ");
    const bool ok = manager_.processPayment(id, processor_);
    const auto& payment = dynamic_cast<const Payment&>(manager_.getById(id));
    if (ok) {
        out_ << "Payment " << id << " confirmed. Status: COMPLETED\n";
    } else {
        out_ << "Payment " << id << " FAILED: " << payment.failureReason() << '\n';
    }
}

void ConsoleApp::viewHistory() {
    if (manager_.empty()) {
        out_ << "No transactions yet.\n";
        return;
    }
    out_ << "Sort by: 1. Creation order  2. Amount (low-high)  3. Amount (high-low)\n"
         << "         4. Newest first     5. Oldest first\n";
    switch (readChoice("Choice (1-5): ", 1, 5)) {
        case 1: printTable(manager_.history()); break;
        case 2: printTable(manager_.sortedByAmount(true)); break;
        case 3: printTable(manager_.sortedByAmount(false)); break;
        case 4: printTable(manager_.sortedByTimestamp(true)); break;
        default: printTable(manager_.sortedByTimestamp(false)); break;
    }
}

void ConsoleApp::searchTransaction() {
    out_ << "Search by: 1. Transaction ID  2. Customer ID\n";
    if (readChoice("Choice (1-2): ", 1, 2) == 1) {
        const std::string id = readNonEmpty("Transaction ID: ");
        printTable({&manager_.getById(id)});
    } else {
        const std::string customerId = readNonEmpty("Customer ID: ");
        const auto results = manager_.findByCustomer(customerId);
        if (results.empty()) {
            out_ << "No transactions for customer " << customerId << ".\n";
        } else {
            printTable(results);
        }
    }
}

void ConsoleApp::filterTransactions() {
    const TransactionStatus statuses[] = {
        TransactionStatus::Pending,   TransactionStatus::Completed,
        TransactionStatus::Failed,    TransactionStatus::Cancelled,
        TransactionStatus::PartiallyRefunded, TransactionStatus::Refunded};

    for (int i = 0; i < 6; ++i) {
        out_ << ' ' << i + 1 << ". " << toString(statuses[i]) << '\n';
    }
    const TransactionStatus chosen = statuses[readChoice("Status (1-6): ", 1, 6) - 1];
    const auto results = manager_.filterByStatus(chosen);
    if (results.empty()) {
        out_ << "No transactions with status " << toString(chosen) << ".\n";
    } else {
        printTable(results);
    }
}

void ConsoleApp::refundPayment() {
    const std::string id = readNonEmpty("Payment ID to refund: ");
    const auto& payment = manager_.getById(id);
    if (const auto* p = dynamic_cast<const Payment*>(&payment); p != nullptr && p->canBeRefunded()) {
        out_ << "Refundable amount: " << money::format(p->refundableAmount()) << " INR\n";
    }
    const Amount amount = readAmount("Refund amount in INR: ");
    const std::string reason = readNonEmpty("Reason: ");
    const Refund& refund = manager_.refundPayment(id, amount, reason);
    out_ << "Refund " << refund.id() << " of " << money::format(refund.amount())
         << " INR recorded for " << id << ".\n";
}

void ConsoleApp::cancelPayment() {
    const std::string id = readNonEmpty("Payment ID to cancel: ");
    manager_.cancelPayment(id);
    out_ << "Payment " << id << " cancelled.\n";
}

void ConsoleApp::viewDetails() {
    const std::string id = readNonEmpty("Transaction ID: ");
    out_ << '\n' << manager_.getById(id).details();  // virtual call
}

bool ConsoleApp::save() {
    try {
        repository_.save(manager_);
        out_ << "Saved " << manager_.size() << " transaction(s) to " << repository_.path() << '\n';
        return true;
    } catch (const PersistenceError& e) {
        out_ << "Save failed: " << e.what() << '\n';
        return false;
    }
}

void ConsoleApp::printTable(const std::vector<const Transaction*>& rows) const {
    out_ << '\n' << std::left
         << std::setw(12) << "ID" << std::setw(9) << "TYPE" << std::setw(14) << "CUSTOMER"
         << std::right << std::setw(12) << "AMOUNT" << "  " << std::left
         << std::setw(20) << "STATUS" << "CREATED\n"
         << std::string(88, '-') << '\n';
    for (const Transaction* t : rows) {
        out_ << std::left << std::setw(12) << t->id() << std::setw(9) << toString(t->type())
             << std::setw(14) << t->customerId() << std::right << std::setw(12)
             << money::format(t->amount()) << "  " << std::left << std::setw(20)
             << toString(t->status()) << timeutil::format(t->createdAt()) << '\n';
    }
    out_ << rows.size() << " transaction(s)\n";
}

// ---------------- input helpers ----------------

std::string ConsoleApp::readLine(const std::string& prompt) {
    out_ << prompt;
    std::string line;
    if (!std::getline(in_, line)) {
        throw InputClosed{};
    }
    return text::trim(line);
}

std::string ConsoleApp::readNonEmpty(const std::string& prompt) {
    while (true) {
        std::string value = readLine(prompt);
        if (!value.empty()) return value;
        out_ << "Please enter a value.\n";
    }
}

int ConsoleApp::readChoice(const std::string& prompt, int min, int max) {
    while (true) {
        const std::string value = readLine(prompt);
        try {
            std::size_t used = 0;
            const int number = std::stoi(value, &used);
            if (used == value.size() && number >= min && number <= max) {
                return number;
            }
        } catch (const std::exception&) {
            // fall through to the message below
        }
        out_ << "Please enter a number between " << min << " and " << max << ".\n";
    }
}

Amount ConsoleApp::readAmount(const std::string& prompt) {
    while (true) {
        try {
            return money::parse(readLine(prompt));
        } catch (const ValidationError& e) {
            out_ << e.what() << '\n';
        }
    }
}

}  // namespace paycore
