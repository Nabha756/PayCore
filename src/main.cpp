#include <exception>
#include <iostream>
#include <string>

#include "paycore/ConsoleApp.h"
#include "paycore/FileRepository.h"
#include "paycore/PaymentProcessor.h"
#include "paycore/TransactionManager.h"

// Usage: paycore [data-file]
// Default data file: data/transactions.csv (relative to where you run it).
int main(int argc, char* argv[]) {
    const std::string dataFile = argc > 1 ? argv[1] : "data/transactions.csv";

    try {
        paycore::TransactionManager manager;
        paycore::FileRepository repository(dataFile);

        const paycore::LoadReport report = repository.load(manager);
        if (!report.fileFound) {
            std::cout << "No data file at " << dataFile << " - starting with empty history.\n";
        } else {
            std::cout << "Loaded " << report.loaded << " transaction(s) from " << dataFile << ".\n";
        }
        for (const std::string& error : report.errors) {
            std::cout << "  Skipped corrupted record (" << error << ")\n";
        }

        paycore::PaymentProcessor processor(0.15);  // 15% of payments fail (simulated)
        paycore::ConsoleApp app(manager, processor, repository);
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
