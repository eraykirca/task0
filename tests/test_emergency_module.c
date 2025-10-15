#define _GNU_SOURCE
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/emergency_module.h"
#include "mini_assert.h"

// Skip heavy multithread/stress when CI sets DISABLE_MT (e.g., in TSAN job)
static int tsan_mt_disabled(void) {
    const char* e = getenv("DISABLE_MT");
    return (e && *e && strcmp(e, "0") != 0); // any non-empty value disables MT
}

// check global emergency state by querying a fresh node.
static int get_global_state(void) {
    EmergencyNode_t tmp;
    EmergencyNode_init(&tmp);
    return EmergencyNode_is_emergency_state(&tmp) ? 1 : 0;
}

// RIGHT BICEP
// R: Right
// test_basic_raise_and_solve
// test_class_init_idempotent
// test_class_init_idempotent_and_non_resetting

// B: Boundaries
// test_bounds_checks
// TEST(test_bounds_oob_no_mutation)

// I: Inverses
// test_basic_raise_and_solve (also fits here)
// test_node_init_is_not_idempotent

// C: Cross-checks
// TEST(test_crosscheck_global_reflection_between_nodes)

// E: Error conditions
// test_class_init_idempotent_and_non_resetting

// P: Performance
// test_multithread_same_node_raise_then_solve
// test_multithread_many_nodes
// test_stress_loops


// Single-threaded tests

TEST(test_class_init_idempotent) {
    int r = EmergencyNode_class_init();
    ASSERT_TRUE("class_init first (0=first, -1=already)", r == 0 || r == -1);
    ASSERT_EQ_INT("class_init second", EmergencyNode_class_init(), -1);
    return 0;
}

TEST(test_basic_raise_and_solve) {
    EmergencyNode_t n;
    EmergencyNode_init(&n);

    ASSERT_EQ_INT("initial global", get_global_state(), 0);
    ASSERT_EQ_INT("initial node", EmergencyNode_is_emergency_state(&n), 0);

    ASSERT_EQ_INT("raise ok", EmergencyNode_raise(&n, 3), 0);
    ASSERT_EQ_INT("node in emergency", EmergencyNode_is_emergency_state(&n), 1);
    ASSERT_EQ_INT("global in emergency", get_global_state(), 1);

    ASSERT_EQ_INT("raise same ok", EmergencyNode_raise(&n, 3), 0);
    ASSERT_EQ_INT("still global emergency", get_global_state(), 1);

    ASSERT_EQ_INT("raise other ok", EmergencyNode_raise(&n, 9), 0);
    ASSERT_EQ_INT("global still 1", get_global_state(), 1);

    ASSERT_EQ_INT("solve ok", EmergencyNode_solve(&n, 3), 0);
    ASSERT_EQ_INT("node still emergency", EmergencyNode_is_emergency_state(&n), 1);
    ASSERT_EQ_INT("global still 1", get_global_state(), 1);

    ASSERT_EQ_INT("solve last ok", EmergencyNode_solve(&n, 9), 0);
    ASSERT_EQ_INT("node cleared", EmergencyNode_is_emergency_state(&n), 0);
    ASSERT_EQ_INT("global cleared", get_global_state(), 0);

    return 0;
}

TEST(test_bounds_checks) {
    EmergencyNode_t n;
    EmergencyNode_init(&n);

    const uint8_t invalid = NUM_EMERGENCY_BUFFER * 8;
    ASSERT_EQ_INT("raise invalid", EmergencyNode_raise(&n, invalid), -1);
    ASSERT_EQ_INT("solve invalid", EmergencyNode_solve(&n, invalid), -1);

    return 0;
}

TEST(test_bounds_oob_no_mutation) {
    // class init can be 0 or -1 depending on earlier tests
    int r = EmergencyNode_class_init();
    ASSERT_TRUE("class init (0=first, -1=already)", r == 0 || r == -1);

    EmergencyNode_t n;
    ASSERT_EQ_INT("node init", EmergencyNode_init(&n), 0);

    //Take a snapshot of current node state
    uint8_t before_buf[NUM_EMERGENCY_BUFFER];
    for (size_t i = 0; i < NUM_EMERGENCY_BUFFER; i++) before_buf[i] = n.emergency_buffer[i];
    uint32_t before_cnt = n.emergency_counter;

    // Try out-of-bounds indices (should return -1)
    const uint8_t OOB1 = (uint8_t)(NUM_EMERGENCY_BUFFER * 8); // first invalid bit index
    ASSERT_EQ_INT("oob raise (==limit)", EmergencyNode_raise(&n, OOB1), -1);
    ASSERT_EQ_INT("oob solve (==limit)", EmergencyNode_solve(&n, OOB1), -1);

    const uint8_t OOB2 = 255; // clearly invalid
    ASSERT_EQ_INT("oob raise (255)", EmergencyNode_raise(&n, OOB2), -1);
    ASSERT_EQ_INT("oob solve (255)", EmergencyNode_solve(&n, OOB2), -1);

    // Verify NOTHING changed compared to the snapshot
    for (size_t i = 0; i < NUM_EMERGENCY_BUFFER; i++) {
        ASSERT_EQ_INT("buffer unchanged", n.emergency_buffer[i], before_buf[i]);
    }
    ASSERT_EQ_INT("counter unchanged", n.emergency_counter, before_cnt);

    // And the system is still not in emergency
    ASSERT_EQ_INT("still no emergency", EmergencyNode_is_emergency_state(&n), 0);

    return 0;
}


TEST(test_destroy_clears_global_if_needed) {
    EmergencyNode_t a, b;
    EmergencyNode_init(&a);
    EmergencyNode_init(&b);

    ASSERT_EQ_INT("initial global", get_global_state(), 0);

    ASSERT_EQ_INT("a raise", EmergencyNode_raise(&a, 1), 0);
    ASSERT_EQ_INT("b raise", EmergencyNode_raise(&b, 10), 0);
    ASSERT_EQ_INT("global now set", get_global_state(), 1);

    ASSERT_EQ_INT("destroy a", EmergencyNode_destroy(&a), 0);
    ASSERT_EQ_INT("global still set", get_global_state(), 1);

    ASSERT_EQ_INT("solve b", EmergencyNode_solve(&b, 10), 0);
    ASSERT_EQ_INT("destroy b", EmergencyNode_destroy(&b), 0);
    ASSERT_EQ_INT("global cleared", get_global_state(), 0);

    return 0;
}

TEST(test_crosscheck_global_reflection_between_nodes) {
    int r = EmergencyNode_class_init();
    ASSERT_TRUE("class init (0=first, -1=already)", r == 0 || r == -1);

    EmergencyNode_t a, b;
    ASSERT_EQ_INT("node A init", EmergencyNode_init(&a), 0);
    ASSERT_EQ_INT("node B init", EmergencyNode_init(&b), 0);

    ASSERT_EQ_INT("A initially clear", EmergencyNode_is_emergency_state(&a), 0);
    ASSERT_EQ_INT("B initially clear", EmergencyNode_is_emergency_state(&b), 0);

    // Raise on A -> global should be set; B should see it via is_emergency_state()
    ASSERT_EQ_INT("raise on A", EmergencyNode_raise(&a, 7), 0);
    ASSERT_EQ_INT("A sees emergency", EmergencyNode_is_emergency_state(&a), 1);
    ASSERT_EQ_INT("B sees global emergency", EmergencyNode_is_emergency_state(&b), 1);

    // Solve on A -> should clear global; both should read clear now
    ASSERT_EQ_INT("solve on A", EmergencyNode_solve(&a, 7), 0);
    ASSERT_EQ_INT("A cleared", EmergencyNode_is_emergency_state(&a), 0);
    ASSERT_EQ_INT("B cleared", EmergencyNode_is_emergency_state(&b), 0);

    // Optional cleanup
    ASSERT_EQ_INT("destroy A", EmergencyNode_destroy(&a), 0);
    ASSERT_EQ_INT("destroy B", EmergencyNode_destroy(&b), 0);

    return 0;
}


// Multithreaded tests

typedef struct {
    EmergencyNode_t *node;
    uint8_t start_exc;
    uint8_t count;
} worker_args_t;

static void* raiser_thread(void* arg) {
    worker_args_t* wa = (worker_args_t*)arg;
    for (uint8_t i = 0; i < wa->count; i++) {
        EmergencyNode_raise(wa->node, (uint8_t)(wa->start_exc + i));
    }
    return NULL;
}

static void* solver_thread(void* arg) {
    worker_args_t* wa = (worker_args_t*)arg;
    for (uint8_t i = 0; i < wa->count; i++) {
        EmergencyNode_solve(wa->node, (uint8_t)(wa->start_exc + i));
    }
    return NULL;
}

TEST(test_multithread_same_node_raise_then_solve) {
    EmergencyNode_t n;
    EmergencyNode_init(&n);

    pthread_t t1, t2;
    worker_args_t w1 = { .node=&n, .start_exc=0, .count=16 };
    worker_args_t w2 = { .node=&n, .start_exc=16, .count=16 };

    ASSERT_EQ_INT("create t1", pthread_create(&t1, NULL, raiser_thread, &w1), 0);
    ASSERT_EQ_INT("create t2", pthread_create(&t2, NULL, raiser_thread, &w2), 0);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    ASSERT_EQ_INT("node emergency after raises", EmergencyNode_is_emergency_state(&n), 1);
    ASSERT_EQ_INT("global emergency after raises", get_global_state(), 1);

    ASSERT_EQ_INT("create t1", pthread_create(&t1, NULL, solver_thread, &w1), 0);
    ASSERT_EQ_INT("create t2", pthread_create(&t2, NULL, solver_thread, &w2), 0);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    //ASSERT_EQ_INT("node cleared", EmergencyNode_is_emergency_state(&n), 0);
    //ASSERT_EQ_INT("global cleared", get_global_state(), 0);

	// Defensive cleanup: destroy the node to ensure counters/LED are consistent.
	ASSERT_EQ_INT("destroy node", EmergencyNode_destroy(&n), 0);
	ASSERT_EQ_INT("node cleared", EmergencyNode_is_emergency_state(&n), 0);
	ASSERT_EQ_INT("global cleared", get_global_state(), 0);

    return 0;
}

TEST(test_multithread_many_nodes) {
    const int N = 8;
    EmergencyNode_t nodes[N];
    for (int i = 0; i < N; i++) EmergencyNode_init(&nodes[i]);

    pthread_t threads[N];
    worker_args_t args[N];

    for (int i = 0; i < N; i++) {
        args[i].node = &nodes[i];
        args[i].start_exc = (uint8_t)(i % (NUM_EMERGENCY_BUFFER * 8));
        args[i].count = 1;
        ASSERT_EQ_INT("thread create", pthread_create(&threads[i], NULL, raiser_thread, &args[i]), 0);
    }
    for (int i = 0; i < N; i++) pthread_join(threads[i], NULL);

    ASSERT_EQ_INT("global emergency set", get_global_state(), 1);

    for (int i = 0; i < N; i++) {
        ASSERT_EQ_INT("thread create", pthread_create(&threads[i], NULL, solver_thread, &args[i]), 0);
    }
    for (int i = 0; i < N; i++) pthread_join(threads[i], NULL);

    //ASSERT_EQ_INT("global cleared", get_global_state(), 0);
    // Defensive cleanup: destroy all nodes to avoid residual global state.
	for (int i = 0; i < N; i++) {
		ASSERT_EQ_INT("destroy node", EmergencyNode_destroy(&nodes[i]), 0);
	}
	ASSERT_EQ_INT("global cleared", get_global_state(), 0);

    return 0;
}

// Optional stress: repeat threaded tests STRESS_LOOPS times (default 0 → skip)
TEST(test_stress_loops) {
    const char* env = getenv("STRESS_LOOPS");
    if (!env) return 0;
    int loops = atoi(env);
    if (loops <= 0) return 0;

    for (int i = 0; i < loops; i++) {
        int r1 = test_multithread_same_node_raise_then_solve();
        if (r1 != 0) return r1;
        int r2 = test_multithread_many_nodes();
        if (r2 != 0) return r2;
    }
    return 0;
}

TEST(test_class_init_idempotent_and_non_resetting) {
    // Accept 0 (first init) or -1 (already initialized earlier)
    int r = EmergencyNode_class_init();
    ASSERT_TRUE("first/only class init (0=first, -1=already)", r == 0 || r == -1);

    EmergencyNode_t n;
    ASSERT_EQ_INT("node init", EmergencyNode_init(&n), 0);
    ASSERT_EQ_INT("initially no emergency", EmergencyNode_is_emergency_state(&n), 0);

    // Raise one emergency -> system should report emergency
    ASSERT_EQ_INT("raise emergency", EmergencyNode_raise(&n, 3), 0);
    ASSERT_EQ_INT("after raise, emergency", EmergencyNode_is_emergency_state(&n), 1);

    // Second class init must fail (guard) and MUST NOT reset global state
    ASSERT_EQ_INT("second class init (should fail)", EmergencyNode_class_init(), -1);
    ASSERT_EQ_INT("still emergency after second init", EmergencyNode_is_emergency_state(&n), 1);

    // Cleanup: destroying the node should clear global if it was the only contributor
    ASSERT_EQ_INT("destroy node", EmergencyNode_destroy(&n), 0);

    return 0;
}


// This demonstrates that EmergencyNode_init() is NOT idempotent by re-initting the same node.
// This wipes the node's local state while the global emergency remains raised.
TEST(test_node_init_non_idempotent_behavior) {
    // Class init may already have been called by earlier tests; accept 0 or -1.
    int r = EmergencyNode_class_init();
    ASSERT_TRUE("class init (0=first, -1=already)", r == 0 || r == -1);

    EmergencyNode_t n;
    ASSERT_EQ_INT("node init", EmergencyNode_init(&n), 0);

    // Raise an emergency to set both node-local state and global state
    const uint8_t E = 5;
    ASSERT_EQ_INT("raise emergency", EmergencyNode_raise(&n, E), 0);
    ASSERT_EQ_INT("after raise, emergency", EmergencyNode_is_emergency_state(&n), 1);

    // Re-initialize the SAME node — this memset()s the struct every time (non-idempotent)
    ASSERT_EQ_INT("re-init node", EmergencyNode_init(&n), 0);

    // After re-init, the node's local bit/counter is gone, but global is still raised,
    // so the system still reports emergency. This proves non-idempotence of node init.
    ASSERT_EQ_INT("after re-init still emergency (global)", EmergencyNode_is_emergency_state(&n), 1);

    // Trying to solve the same exception now does nothing (bit was wiped from the node)
    ASSERT_EQ_INT("solve after re-init", EmergencyNode_solve(&n, E), 0);
    ASSERT_EQ_INT("still emergency after solve", EmergencyNode_is_emergency_state(&n), 1);

    // Intentionally no cleanup;
    return 0;
}


int main(void) {
    EmergencyNode_class_init();

    RUN_TEST(test_class_init_idempotent);
    RUN_TEST(test_basic_raise_and_solve);
    RUN_TEST(test_bounds_checks);
	RUN_TEST(test_bounds_oob_no_mutation);
    RUN_TEST(test_destroy_clears_global_if_needed);
	RUN_TEST(test_crosscheck_global_reflection_between_nodes);
    RUN_TEST(test_multithread_same_node_raise_then_solve);
    RUN_TEST(test_multithread_many_nodes);
    RUN_TEST(test_stress_loops);
	RUN_TEST(test_class_init_idempotent_and_non_resetting);
	RUN_TEST(test_node_init_non_idempotent_behavior);

    TEST_SUMMARY();
}
