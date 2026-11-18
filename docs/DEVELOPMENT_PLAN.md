# Development Plan and Git Workflow

## Milestones and commits

Build the project in this order. After each milestone: inspect changes, review the diff,
compile, run the tests, then commit.

| # | Milestone | Files | Suggested commit message |
|---|-----------|-------|--------------------------|
| 1 | Project setup | `CMakeLists.txt` (app target only), `.gitignore`, `README.md`, empty folders | `chore: initialize C++ project with CMake` |
| 2 | Core types and errors | `Types.h/.cpp`, `Exceptions.h`, `Utils.h/.cpp` | `feat: add core types, money and time utilities` |
| 3 | Transaction hierarchy | `Transaction`, `Payment`, `Refund` | `feat: implement transaction class hierarchy` |
| 4 | Validation | `PaymentValidator` | `feat: add payment validation with Luhn check` |
| 5 | Processing | `PaymentProcessor`, `IdGenerator`, `TransactionManager` (create/process) | `feat: add payment processing logic` |
| 6 | Cancel and refunds | `TransactionManager::cancelPayment/refundPayment` | `feat: implement cancellation and refund functionality` |
| 7 | History queries | search, filter, sort in `TransactionManager` | `feat: add transaction history search, filter and sort` |
| 8 | Persistence | `FileRepository`, `data/sample_transactions.csv` | `feat: add transaction persistence` |
| 9 | CLI | `ConsoleApp`, `main.cpp` | `feat: add menu-driven command-line interface` |
| 10 | Unit tests | `tests/*`, test section of `CMakeLists.txt` | `test: add unit tests for validation, processing, refunds and persistence` |
| 11 | Bug fixes found by tests | as needed | `fix: <what was wrong>` |
| 12 | Documentation | `docs/*`, README | `docs: document architecture, UML and testing` |

You can also split milestone 10 so each feature gets its own test commit
(e.g. `test: add refund tests` straight after milestone 6).

## Commands

```bash
# One-time setup
git init
git config user.name  "Your Name"
git config user.email "you@example.com"
git branch -M main

# Before every commit
git status
git diff
cmake --build build
ctest --test-dir build --output-on-failure
git add <files for this milestone>
git commit -m "feat: implement transaction class hierarchy"

# Push (after creating an empty repo on GitHub)
git remote add origin https://github.com/<username>/PayCore.git
git push -u origin main
```

## Conventional Commit types used

| Type | Use for |
|------|---------|
| `feat` | New functionality |
| `fix` | Bug fixes |
| `test` | Adding or changing tests |
| `docs` | Documentation only |
| `refactor` | Code change that does not change behaviour |
| `chore` | Build setup, `.gitignore`, tooling |
