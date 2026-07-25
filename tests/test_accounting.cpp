// Tests for the double-entry accounting posting in finalizeInvoice(),
// voidInvoice(), and recordPayment().
//
// Phase 2 status: the four former "characterization" tests (which used to
// document bugs by asserting the buggy behavior) have been FLIPPED to assert
// the corrected behavior. New positive tests cover voiding, payments, and
// item_type classification.
//
// Reference (core/db_manager.cpp):
//   finalizeInvoice posts:
//     Accounts Receivable +invoice_total     (DEBIT — was Checking, audit C2)
//     Parts Income          -parts_retail     (CREDIT)
//     Labor Income          -(labor+supplies) (CREDIT)
//     Sales Tax Liability   -tax_liability    (CREDIT, now ROUNDED — audit H1)
//     COGS                  +parts_cost_cents (DEBIT)
//     Parts Inventory       -parts_cost_cents (CREDIT)
//     [if prepayment>0]:
//       Customer Deposits   -prepayment_cents (CREDIT/relieve liability — audit C3)
//       Accounts Receivable -prepayment_cents (CREDIT)
//   Idempotency: refuses if posted_tx_id != 0 (audit C6).

#include "test_runner.h"
#include "db_manager.h"

#include <map>
#include <memory>

using namespace tuxrepair;

// Map of account name -> balance (in cents) computed from all transactions.
static std::map<std::string, int64_t> account_balances(DBManager& db) {
    std::map<std::string, int64_t> out;
    for (const auto& t : db.getTransactions()) {
        for (const auto& s : t.splits) {
            out[s.account_name] += s.amount;
        }
    }
    return out;
}

// Sum of every split across every transaction. Must always be 0 in a correct
// double-entry system.
static int64_t total_split_sum(DBManager& db) {
    int64_t sum = 0;
    for (const auto& t : db.getTransactions()) {
        for (const auto& s : t.splits) sum += s.amount;
    }
    return sum;
}

// Helper: seed customer + vehicle + invoice with the given items, return inv_id.
// Does NOT finalize.
static int seed_invoice(DBManager& db, const std::vector<InvoiceItem>& items) {
    Customer c;
    c.first_name = "Test";
    c.last_name = "Customer";
    int cid = db.insertCustomer(c);

    Vehicle v;
    v.customer_id = cid;
    v.license_plate = "FIN0001";
    int vid = db.insertVehicle(v);

    int inv_id = db.createInvoice(cid, vid, "Invoice", 100, "2026-07-19");
    db.saveInvoiceItems(inv_id, items);
    return inv_id;
}

// Helper: seed + finalize. tax_rate_bps in basis points (825 = 8.25%).
static int seed_and_finalize(DBManager& db,
                             const std::vector<InvoiceItem>& items,
                             int64_t parts_cost_cents,
                             int tax_rate_bps = 0,
                             bool supplies_removed = false) {
    int inv_id = seed_invoice(db, items);
    bool ok = db.finalizeInvoice(inv_id, parts_cost_cents, tax_rate_bps, supplies_removed);
    return ok ? inv_id : -1;
}

// Builds a simple parts-only invoice: one part, qty*N, unit_price in cents.
static int finalize_parts_only(DBManager& db, double qty, int64_t unit_price_cents,
                               int64_t parts_cost_cents, int tax_rate_bps = 0) {
    InvoiceItem p;
    p.part_number = "P1";
    p.description = "Part";
    p.quantity = qty;
    p.unit_price = unit_price_cents;
    p.item_type = "Part";        // classification key (audit H3)
    p.specification = "Part";    // legacy field, no longer used for classification
    p.taxable = true;
    return seed_and_finalize(db, {p}, parts_cost_cents, tax_rate_bps);
}

// =============================================================================
// Stable / correct behavior — must keep passing through all phases
// =============================================================================

bool finalize_invoice_returns_true_on_success() {
    auto db = std::make_shared<DBManager>();
    ASSERT_TRUE(db->open(":memory:"));
    int inv_id = finalize_parts_only(*db, 1.0, 1000, 600);
    ASSERT_GE(inv_id, 1);
    return true;
}

bool finalize_invoice_posts_balanced_splits() {
    // Debits == Credits — the irreducible invariant of double-entry.
    auto db = std::make_shared<DBManager>();
    ASSERT_TRUE(db->open(":memory:"));
    finalize_parts_only(*db, 2.0, 1000, 700);  // $20 part, $7 cost
    ASSERT_EQ(total_split_sum(*db), 0);
    return true;
}

bool finalize_invoice_parts_income_correct() {
    auto db = std::make_shared<DBManager>();
    ASSERT_TRUE(db->open(":memory:"));
    finalize_parts_only(*db, 2.0, 1000, 700);  // parts retail = 2000 cents
    auto bal = account_balances(*db);
    ASSERT_EQ(bal["Parts Income"], -2000);
    return true;
}

bool finalize_invoice_cogs_and_inventory_balance() {
    auto db = std::make_shared<DBManager>();
    ASSERT_TRUE(db->open(":memory:"));
    finalize_parts_only(*db, 1.0, 1000, 600);  // cost = 600 cents
    auto bal = account_balances(*db);
    ASSERT_EQ(bal["Cost of Goods Sold Expense"], 600);
    ASSERT_EQ(bal["Parts Inventory Asset"], -600);
    return true;
}

bool finalize_invoice_supplies_clamped() {
    // Tiny labor: 5% = 5 cents, clamped up to 200-cent minimum.
    auto db = std::make_shared<DBManager>();
    ASSERT_TRUE(db->open(":memory:"));

    InvoiceItem l;
    l.part_number = "L1";
    l.description = "Labor";
    l.quantity = 1.0;
    l.unit_price = 100;       // $1 labor -> supplies = 5c, clamped to 200c
    l.item_type = "Labor";
    int inv_id = seed_and_finalize(*db, {l}, 0, 0);
    (void)inv_id;

    auto bal = account_balances(*db);
    // Labor Income credit includes the bundled supplies charge:
    // -(labor + supplies) = -(100 + 200) = -300
    ASSERT_EQ(bal["Labor Income"], -300);
    // Accounts Receivable debit = labor + supplies = 100 + 200 = 300
    // (audit C2 fix: was Checking Asset)
    ASSERT_EQ(bal["Accounts Receivable"], 300);
    ASSERT_EQ(bal["Checking Asset"], 0);  // unchanged until a payment lands
    return true;
}

// =============================================================================
// Phase 2 fixes — the four former characterization tests, now asserting the
// CORRECTED behavior.
// =============================================================================

bool finalize_debits_AR_not_checking() {
    // audit C2 fix: finalization debits Accounts Receivable, not Checking.
    auto db = std::make_shared<DBManager>();
    ASSERT_TRUE(db->open(":memory:"));
    finalize_parts_only(*db, 1.0, 1000, 600, 0);  // parts=1000c, cost=600c, no tax
    // invoice_total = 1000.

    auto bal = account_balances(*db);
    ASSERT_EQ(bal["Accounts Receivable"], 1000);  // debited (was wrongly Checking)
    ASSERT_EQ(bal["Checking Asset"], 0);          // unchanged
    return true;
}

bool finalize_posts_prepayment_consumption() {
    // audit C3 fix: prepayment is posted. Deposit liability is consumed and
    // A/R is reduced by the prepaid amount at finalize.
    auto db = std::make_shared<DBManager>();
    ASSERT_TRUE(db->open(":memory:"));

    // Build an invoice with a prepayment, then finalize.
    int inv_id = seed_invoice(*db, {[]{
        InvoiceItem p; p.part_number="P1"; p.description="Part";
        p.quantity=1.0; p.unit_price=1000; p.item_type="Part";
        p.specification="Part"; p.taxable=true; return p;
    }()});
    ASSERT_GE(inv_id, 1);

    // Record a $5 deposit BEFORE finalize. This is a pre-finalize deposit:
    // debit Checking +500, credit Customer Deposits -500, bump prepayment_cents.
    ASSERT_TRUE(db->recordPayment(inv_id, 500, "Cash", "dep-001"));

    // Finalize. invoice_total = 1000 (parts) + 0 tax = 1000. With a 500c
    // prepayment, finalize also posts the deposit-consumption pair.
    ASSERT_TRUE(db->finalizeInvoice(inv_id, 600, 0, false));

    auto bal = account_balances(*db);
    // Checking got the deposit: +500.
    ASSERT_EQ(bal["Checking Asset"], 500);
    // Customer Deposits: -500 (deposit) + -500 (consumption at finalize, which
    // relieves the liability by debiting it). Net = 0. The deposit is consumed.
    ASSERT_EQ(bal["Customer Deposits"], 0);
    // A/R: +1000 (invoice_total) - 500 (prepayment reduces receivable) = +500.
    ASSERT_EQ(bal["Accounts Receivable"], 500);
    return true;
}

bool finalize_is_idempotent() {
    // audit C6 fix: a second finalize refuses and does NOT double-post.
    auto db = std::make_shared<DBManager>();
    ASSERT_TRUE(db->open(":memory:"));
    int inv_id = finalize_parts_only(*db, 1.0, 1000, 600, 0);
    ASSERT_GE(inv_id, 1);

    // Second finalize must fail (return false) and post nothing new.
    bool second_ok = db->finalizeInvoice(inv_id, 600, 0, false);
    ASSERT_FALSE(second_ok);

    // Single posting, unchanged.
    auto bal = account_balances(*db);
    ASSERT_EQ(bal["Parts Income"], -1000);
    ASSERT_EQ(bal["Accounts Receivable"], 1000);
    return true;
}

bool finalize_rounds_tax_to_nearest_cent() {
    // audit H1 fix: tax rounds to nearest cent, not truncates.
    auto db = std::make_shared<DBManager>();
    ASSERT_TRUE(db->open(":memory:"));

    // parts = 1005c. tax_rate = 875 bps (8.75%). expected tax = 1005 * 0.0875
    //   = 87.9375c. Rounded = 88c. (Old behavior truncated to 87c.)
    InvoiceItem p;
    p.part_number = "P1";
    p.description = "Part";
    p.quantity = 1.0;
    p.unit_price = 1005;
    p.item_type = "Part";
    p.specification = "Part";
    p.taxable = true;
    seed_and_finalize(*db, {p}, 0, 875);

    auto bal = account_balances(*db);
    ASSERT_EQ(bal["Sales Tax Liability"], -88);  // rounded, was -87
    return true;
}

// =============================================================================
// New Phase 2 tests — payments, voiding, classification
// =============================================================================

bool record_payment_pre_finalize_posts_deposit() {
    // Pre-finalize money: Customer Deposit, not A/R. Bumps prepayment_cents.
    auto db = std::make_shared<DBManager>();
    ASSERT_TRUE(db->open(":memory:"));

    int inv_id = seed_invoice(*db, {[]{
        InvoiceItem p; p.part_number="P1"; p.description="Part";
        p.quantity=1.0; p.unit_price=1000; p.item_type="Part";
        p.specification="Part"; p.taxable=true; return p;
    }()});

    ASSERT_TRUE(db->recordPayment(inv_id, 500, "Cash", "dep-001"));

    auto bal = account_balances(*db);
    ASSERT_EQ(bal["Checking Asset"], 500);
    ASSERT_EQ(bal["Customer Deposits"], -500);
    ASSERT_EQ(bal["Accounts Receivable"], 0);  // no A/R movement pre-finalize

    // prepayment_cents was bumped so the UI can show it against the balance.
    Invoice inv;
    ASSERT_TRUE(db->getInvoice(inv_id, inv));
    ASSERT_EQ(inv.prepayment_cents, 500);
    return true;
}

bool record_payment_post_finalize_posts_AR() {
    // Post-finalize money: credits A/R.
    auto db = std::make_shared<DBManager>();
    ASSERT_TRUE(db->open(":memory:"));

    int inv_id = finalize_parts_only(*db, 1.0, 1000, 600, 0);
    ASSERT_GE(inv_id, 1);
    // After finalize: A/R = +1000.

    ASSERT_TRUE(db->recordPayment(inv_id, 400, "Check", "1001"));

    auto bal = account_balances(*db);
    ASSERT_EQ(bal["Checking Asset"], 400);      // payment went to cash
    ASSERT_EQ(bal["Accounts Receivable"], 600); // 1000 - 400 = 600 still owed
    ASSERT_EQ(bal["Customer Deposits"], 0);     // no deposit liability
    return true;
}

bool void_invoice_posts_reversing_transaction() {
    auto db = std::make_shared<DBManager>();
    ASSERT_TRUE(db->open(":memory:"));
    int inv_id = finalize_parts_only(*db, 1.0, 1000, 600, 0);
    ASSERT_GE(inv_id, 1);

    auto bal_before = account_balances(*db);
    ASSERT_EQ(bal_before["Accounts Receivable"], 1000);

    ASSERT_TRUE(db->voidInvoice(inv_id));

    auto bal_after = account_balances(*db);
    // Every account nets to zero: the mirror splits cancel the originals.
    ASSERT_EQ(bal_after["Accounts Receivable"], 0);
    ASSERT_EQ(bal_after["Parts Income"], 0);
    ASSERT_EQ(bal_after["Cost of Goods Sold Expense"], 0);
    ASSERT_EQ(bal_after["Parts Inventory Asset"], 0);

    // The invoice is marked Voided and unposted.
    Invoice inv;
    ASSERT_TRUE(db->getInvoice(inv_id, inv));
    ASSERT_TRUE(inv.status == "Voided");
    ASSERT_EQ(inv.posted_tx_id, 0);

    // Ledger still balances (void posted balanced mirror splits).
    ASSERT_EQ(total_split_sum(*db), 0);
    return true;
}

bool void_then_refinalize_posts_again() {
    // After voiding, posted_tx_id is cleared, so the invoice can be re-finalized.
    auto db = std::make_shared<DBManager>();
    ASSERT_TRUE(db->open(":memory:"));
    int inv_id = finalize_parts_only(*db, 1.0, 1000, 600, 0);
    ASSERT_TRUE(db->voidInvoice(inv_id));

    // Re-finalize — should succeed and post a fresh transaction.
    ASSERT_TRUE(db->finalizeInvoice(inv_id, 600, 0, false));

    auto bal = account_balances(*db);
    // Original (+1000) + void (-1000) + re-finalize (+1000) = +1000 net A/R.
    ASSERT_EQ(bal["Accounts Receivable"], 1000);

    Invoice inv;
    ASSERT_TRUE(db->getInvoice(inv_id, inv));
    // posted_tx_id is non-zero after re-finalize (and points at the NEW tx).
    ASSERT_TRUE(inv.posted_tx_id != 0);
    return true;
}

bool classify_uses_item_type_not_specification() {
    // audit H3 fix: a line with item_type='Discount' but specification='Part'
    // must be classified as a Discount, not a Part. Discounts reduce the total.
    auto db = std::make_shared<DBManager>();
    ASSERT_TRUE(db->open(":memory:"));

    InvoiceItem p;
    p.part_number = "P1"; p.description = "Part";
    p.quantity = 1.0; p.unit_price = 1000;
    p.item_type = "Part"; p.specification = "Part"; p.taxable = true;

    InvoiceItem d;
    d.part_number = "DISC"; d.description = "Loyalty Discount";
    d.quantity = 1.0; d.unit_price = 100;   // $1.00 discount
    d.item_type = "Discount";
    d.specification = "Part";   // WRONG legacy field — must be ignored now
    d.taxable = true;

    seed_and_finalize(*db, {p, d}, 0, 0);

    auto bal = account_balances(*db);
    // invoice_total = parts(1000) - discount(100) = 900.
    // A/R debit = 900. Parts Income credit = -1000 (only the Part line).
    ASSERT_EQ(bal["Accounts Receivable"], 900);
    ASSERT_EQ(bal["Parts Income"], -1000);
    return true;
}

bool payments_balance_in_all_scenarios() {
    // The irreducible invariant: no matter what combination of finalize /
    // payment / void we throw at the ledger, debits must equal credits.
    auto db = std::make_shared<DBManager>();
    ASSERT_TRUE(db->open(":memory:"));

    int inv1 = finalize_parts_only(*db, 2.0, 1000, 700, 800);  // tax 8%
    ASSERT_TRUE(db->recordPayment(inv1, 500, "Cash", ""));

    int inv2 = seed_invoice(*db, {[]{
        InvoiceItem l; l.part_number="L"; l.description="Labor";
        l.quantity=1.0; l.unit_price=2500; l.item_type="Labor";
        l.specification="Labor"; l.taxable=false; return l;
    }()});
    ASSERT_TRUE(db->recordPayment(inv2, 1000, "Card", "tx-9"));  // pre-finalize deposit
    ASSERT_TRUE(db->finalizeInvoice(inv2, 0, 0, false));
    ASSERT_TRUE(db->voidInvoice(inv2));

    ASSERT_EQ(total_split_sum(*db), 0);
    return true;
}

// =============================================================================
// Test registry
// =============================================================================

RUN_TESTS {
    // Stable behavior:
    RUN_TEST(finalize_invoice_returns_true_on_success);
    RUN_TEST(finalize_invoice_posts_balanced_splits);
    RUN_TEST(finalize_invoice_parts_income_correct);
    RUN_TEST(finalize_invoice_cogs_and_inventory_balance);
    RUN_TEST(finalize_invoice_supplies_clamped);

    // Phase 2 fixes (former characterization tests, now flipped):
    RUN_TEST(finalize_debits_AR_not_checking);
    RUN_TEST(finalize_posts_prepayment_consumption);
    RUN_TEST(finalize_is_idempotent);
    RUN_TEST(finalize_rounds_tax_to_nearest_cent);

    // New Phase 2 coverage:
    RUN_TEST(record_payment_pre_finalize_posts_deposit);
    RUN_TEST(record_payment_post_finalize_posts_AR);
    RUN_TEST(void_invoice_posts_reversing_transaction);
    RUN_TEST(void_then_refinalize_posts_again);
    RUN_TEST(classify_uses_item_type_not_specification);
    RUN_TEST(payments_balance_in_all_scenarios);

    TEST_RETURN;
}
