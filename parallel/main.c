#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>
#include "common/io.h"
#include "common/sumset.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

typedef struct SumsetWrapper {
    Sumset sumset;
    atomic_int f_count;
    struct SumsetWrapper* parent;
} SumsetWrapper;

typedef struct Node {
    SumsetWrapper* A;
    SumsetWrapper* B;
    struct Node* prev;
} Node;

typedef struct {
    Node* head;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    atomic_int waiting_threads;
} Stack;

atomic_bool done = false;

static inline void sumset_copy(Sumset* dest, const Sumset* src) {
    *dest = *src;
}

static void push(Stack* stack, Node* new_node) {
    pthread_mutex_lock(&stack->mutex);
    new_node->prev = stack->head;
    stack->head = new_node;
    pthread_cond_signal(&stack->cond);
    pthread_mutex_unlock(&stack->mutex);
}

Node* pop(Stack* stack) {
    pthread_mutex_lock(&stack->mutex);
    Node* old_head = stack->head;
    if (old_head) stack->head = old_head->prev;
    pthread_mutex_unlock(&stack->mutex);
    return old_head;
}

static void sumset_wrapper_free(SumsetWrapper* wrapper) {
    if (!wrapper) return;

    if (atomic_fetch_sub(&wrapper->f_count, 1) <= 1) {
        sumset_wrapper_free(wrapper->parent);
        free(wrapper);
    }
}

static pthread_mutex_t solution_mutex;
static pthread_cond_t main_cond;
static Solution* best_solution;
static InputData input_data;
static Stack stack;

static void solve(SumsetWrapper* a, SumsetWrapper* b, Solution* solution) {
    if (a->sumset.sum > b->sumset.sum) {
        SumsetWrapper* temp = a;
        a = b;
        b = temp;
    }

    if (is_sumset_intersection_trivial(&a->sumset, &b->sumset)) {
        int counter = 0;
        for (size_t i = a->sumset.last; i <= input_data.d; ++i) {
            if (!does_sumset_contain(&b->sumset, i)) counter++;
        }

        counter = counter - 2;

        for (size_t i = a->sumset.last; i <= input_data.d; ++i) {
            if (!does_sumset_contain(&b->sumset, i)) {
                Node* new_node = malloc(sizeof(Node));
                SumsetWrapper* wrapper = malloc(sizeof(SumsetWrapper));
                
                new_node->A = wrapper;
                wrapper->parent = a;
                atomic_store(&wrapper->f_count, 1);
                sumset_add(&wrapper->sumset, &a->sumset, i);

                atomic_fetch_add(&a->f_count, 1);
                atomic_fetch_add(&b->f_count, 1);

                new_node->B = b;
                if (counter-- > 0 && atomic_load(&stack.waiting_threads) > 0) {
                    push(&stack, new_node);
                } else {
                    solve(new_node->A, b, solution);
                    sumset_wrapper_free(new_node->A);
                    sumset_wrapper_free(new_node->B);
                    free(new_node);
                }
            }
        }
    } else if (a->sumset.sum == b->sumset.sum && get_sumset_intersection_size(&a->sumset, &b->sumset) == 2) {
        if (b->sumset.sum > solution->sum) {
            solution_build(solution, &input_data, &a->sumset, &b->sumset);
        }
    }
}

void* worker(void* arg) {
    Stack* stack = (Stack*)arg;
    Solution solution;
    solution_init(&solution);
    while (!atomic_load(&done)) {
        Node* node = pop(stack);
        if (node) {
            solve(node->A, node->B, &solution);
            sumset_wrapper_free(node->A);
            sumset_wrapper_free(node->B);
            free(node);
        } else {
            atomic_fetch_add(&stack->waiting_threads, 1);
            if (stack->waiting_threads == input_data.t) {
                atomic_store(&done, true);
                pthread_cond_broadcast(&stack->cond);
                pthread_cond_signal(&main_cond);
            } else {
                pthread_mutex_lock(&stack->mutex);
                pthread_cond_wait(&stack->cond, &stack->mutex);
                pthread_mutex_unlock(&stack->mutex);
                atomic_fetch_sub(&stack->waiting_threads, 1);
            }
        }
    }

    pthread_mutex_lock(&solution_mutex);
    if(solution.sum > best_solution->sum){
        *best_solution = solution;
    }
    pthread_mutex_unlock(&solution_mutex);

    return NULL;
}

int main() {
    input_data_read(&input_data);
    Solution sol;
    best_solution = &sol;

    solution_init(best_solution);
    pthread_mutex_init(&solution_mutex, NULL);
    pthread_cond_init(&main_cond, NULL);
    pthread_mutex_init(&stack.mutex, NULL);
    pthread_cond_init(&stack.cond, NULL);

    stack.head = NULL;
    atomic_store(&stack.waiting_threads, 0);

    Node* initial_node = malloc(sizeof(Node));
    initial_node->A = malloc(sizeof(SumsetWrapper));
    initial_node->B = malloc(sizeof(SumsetWrapper));
    initial_node->prev = NULL;

    sumset_copy(&initial_node->A->sumset, &input_data.a_start);
    initial_node->A->parent = NULL;
    atomic_store(&initial_node->A->f_count, 1);

    sumset_copy(&initial_node->B->sumset, &input_data.b_start);
    initial_node->B->parent = NULL;
    atomic_store(&initial_node->B->f_count, 1);

    push(&stack, initial_node);

    pthread_t* threads = malloc(sizeof(pthread_t) * input_data.t);
    for (int i = 0; i < input_data.t; ++i) {
        pthread_create(&threads[i], NULL, worker, &stack);
    }

    pthread_mutex_lock(&stack.mutex);
    while (!done) pthread_cond_wait(&main_cond, &stack.mutex);
    pthread_mutex_unlock(&stack.mutex);

    for (int i = 0; i < input_data.t; ++i) pthread_join(threads[i], NULL);

    solution_print(best_solution);
    pthread_mutex_destroy(&solution_mutex);
    pthread_mutex_destroy(&stack.mutex);
    pthread_cond_destroy(&stack.cond);
    pthread_cond_destroy(&main_cond);

    free(threads);
    return 0;
}
