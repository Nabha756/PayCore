# PayCore – Object-Oriented Payment Processing Simulator

PayCore is a command-line payment processing simulator written in C++17. It creates,
processes, cancels and refunds payments, keeps a searchable transaction history and
saves everything to a text file between runs.

> **Simulation only.** No real money is moved and no real gateway is contacted.
> Do not enter real card details. Use test cards such as `4111 1111 1111 1111`.

## Features

- Create payments by card, UPI or wallet, with input validation (Luhn check, expiry, CVV, UPI ID format, mobile number)
- Process payments through a simulated gateway that fails a configurable share of payments (15% by default) with a random decline reason
- Cancel pending payments; make full or partial refunds of completed payments
- Unique transaction IDs (`PAY-000001`, `REF-000002`) with created/updated timestamps
- View the history sorted by creation order, amount or timestamp
- Search by transaction ID or customer ID; filter by status
- Save to and load from `data/transactions.csv`; missing files and corrupted lines are handled
- 51 GoogleTest unit tests

## Requirements

| Tool | Version |
|------|---------|
| C++ compiler | GCC 9+, Clang 10+ or MSVC 2019+ (C++17) |
| CMake | 3.16+ |
| Git | any recent version |
| Internet (first build only) | CMake downloads GoogleTest automatically |

## Build and run

```bash
# Configure and build (from the project root)
cmake -S . -B build
cmake --build build

# Run the app (run from the project root so data/ is found)
./build/paycore                    # Linux / macOS
.\build\Debug\paycore.exe          # Windows (Visual Studio generator)

# Optional: start with sample data
cp data/sample_transactions.csv data/transactions.csv

# Or use any data file
./build/paycore path/to/file.csv
```

To build without tests (no internet needed): `cmake -S . -B build -DPAYCORE_BUILD_TESTS=OFF`

## Run the tests

```bash
cmake --build build
ctest --test-dir build --output-on-failure
# or run the test binary directly for full GoogleTest output
./build/paycore_tests
```

See [docs/TESTING.md](docs/TESTING.md) for what each test file covers.

## Menu

```
 1. Create payment             6. Refund payment
 2. Process payment            7. Cancel payment
 3. View transaction history   8. View transaction details
 4. Search transaction         9. Save and exit
 5. Filter transactions
```

Typical flow: **1** create a payment → **2** process it (it may fail, by design) →
**6** refund part of it → **3** view history → **9** save. If input ends unexpectedly
(Ctrl+D / Ctrl+Z), the app saves before exiting.

## Project structure

```
PayCore/
├── CMakeLists.txt
├── include/paycore/   header files (class declarations)
├── src/               implementation files + main.cpp
├── tests/             GoogleTest unit tests
├── data/              sample_transactions.csv (transactions.csv is created at runtime)
└── docs/              architecture, UML, testing and development plan
```

## Documentation

- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md): requirements, class design, UML diagram, data structure choices, C++ concepts used
- [docs/TESTING.md](docs/TESTING.md): test strategy and coverage
- [docs/DEVELOPMENT_PLAN.md](docs/DEVELOPMENT_PLAN.md): milestones and the commit plan

## Data file format

One transaction per line, fields separated by `|`. Amounts are stored in paise (integer), timestamps in Unix seconds.

```
PAYMENT|id|customerId|amount|status|createdAt|updatedAt|method|maskedAccount|refundedAmount|failureReason
REFUND|id|customerId|amount|status|createdAt|updatedAt|originalPaymentId|reason
```

Only masked account numbers are stored. Full card numbers and CVVs are never saved.
