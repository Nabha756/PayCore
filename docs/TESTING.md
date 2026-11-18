# Testing

Unit tests use **GoogleTest**. CMake uses an installed GoogleTest if it finds one,
otherwise it downloads v1.14.0 during the first configure.

## Running

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure      # via CTest
./build/paycore_tests                            # direct, full output
./build/paycore_tests --gtest_filter=ManagerTest.*   # one group only
```

## Test files

| File | Area | What is checked |
|------|------|-----------------|
| `test_validator.cpp` | Payment validation, invalid input | Money parsing (decimals, negatives, overflow, bad text), amount limits, customer ID format, Luhn checksum, card length, expiry (incl. current month, invalid month), CVV, UPI, wallet number, free-text rules, masking |
| `test_transaction.cpp` | Status rules, refunds, polymorphism | New payment is pending, constructor rejects bad data, partial → full refund, refund larger than remaining, refund of pending/failed/cancelled, illegal transitions, restored-data consistency, virtual calls through `unique_ptr<Transaction>` |
| `test_processor.cpp` | Processing, failure simulation | 0% failure always completes, 100% always fails with a reason, same seed gives same results, cannot process twice, invalid failure rate |
| `test_manager.cpp` | Processing, cancellation, refunds, history | Unique IDs, card number never stored, invalid input creates nothing, success/failure, unknown ID, cancel rules, full/partial refunds, invalid refunds leave no trace, refund of a refund, search by ID/customer, status filter, sorting, empty manager, duplicate IDs, IDs continue after load |
| `test_persistence.cpp` | File persistence, edge cases | Save → load round trip (byte-identical records), missing file, folders created on save, 9 kinds of corrupted lines skipped while valid lines load, empty file, load replaces old data, unwritable path throws |

**Total: 51 tests.**

## Strategy

- **Deterministic randomness:** tests create `PaymentProcessor(0.0, seed)` or `PaymentProcessor(1.0, seed)` so results never depend on chance.
- **Deterministic dates:** expiry checks use the `validate(details, year, month)` overload instead of today's date.
- **Isolated files:** each persistence test gets its own temporary folder, which is deleted afterwards.
- **No partial changes:** after each rejected operation, tests check that the transaction count and status did not change.

## Manual test checklist (CLI)

| Step | Expected |
|------|----------|
| Run with no data file | "No data file … starting with empty history" |
| Create payment with card `4111 1111 1111 1112` | "Card number failed the Luhn checksum" |
| Enter amount `abc` or `10.999` | Re-prompted with an explanation |
| Enter menu option `0`, `10` or `x` | "Please enter a number between 1 and 9" |
| Process the same payment twice | Second attempt: "only PENDING payments can be processed" |
| Refund more than the refundable amount | "exceeds refundable amount" |
| Cancel a completed payment | "Only PENDING payments can be cancelled" |
| Edit a line in `data/transactions.csv` to break it, restart | "Skipped corrupted record (line N: …)", other records load |
| Press Ctrl+D (Linux/macOS) / Ctrl+Z then Enter (Windows) | Data is saved before exit |
