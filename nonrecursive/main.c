#include "common/io.h"
#include "common/sumset.h"

#define STACK_SIZE 1024

typedef struct 
{
    Sumset A;
    Sumset* B;
    bool visited;
} Frame;

typedef struct 
{
    Frame stack[STACK_SIZE];
    int top;
} Stack;

static Stack stack;

static inline void stack_init(){
    stack.top = -1;
}

static inline bool is_stack_empty(){
    return stack.top < 0;
}


typedef struct
{
    Sumset* A;
    Sumset* B;
    bool visited;
} data;

static inline void push(Frame* f){
    stack.stack[++stack.top] = *f;
}

static inline void inc_and_set_visited(){
    stack.stack[++stack.top].visited = 1;
}

static inline Sumset* get_a(){
    return &stack.stack[stack.top].A;
}

static inline Sumset* get_b(){
    return stack.stack[stack.top].B;
}

static inline void sumset_copy(Sumset* dest, const Sumset* src) {
    *dest = *src;
}

static inline void sumset_swap(Sumset** a, Sumset** b) {
    Sumset* tmp = *a;
    *a = *b;
    *b = tmp;
}

static data d;

static inline void get_data(){
    d.A = get_a();
    d.B = get_b();
    d.visited = stack.stack[stack.top].visited;
}



int main()
{
    InputData input_data;
    Solution best_solution;
    
    input_data_read(&input_data);
    
    solution_init(&best_solution);

    stack_init();
    
    Sumset A0, B0;
    sumset_copy(&A0, &input_data.a_start);
    sumset_copy(&B0, &input_data.b_start);
    
    Frame first = {A0, &B0, 0};
    push(&first);
    
    while(!is_stack_empty()){

        if(stack.stack[stack.top].visited){
            stack.top--;
        }else{
        
        get_data();
        stack.top--;

        if(!d.visited){
            if(d.A->sum > d.B->sum){
                sumset_swap(&d.A,&d.B);
            }

            if(is_sumset_intersection_trivial(d.A,d.B)){
                for(size_t i = d.A->last; i <= input_data.d; i++){
                        if(!does_sumset_contain(d.B,i)){
                            if(!d.visited){
                                inc_and_set_visited();
                            }
                            sumset_add(&stack.stack[++stack.top].A, d.A, i);
                            stack.stack[stack.top].B = d.B;
                            stack.stack[stack.top].visited = 0;
                        }
                    }
            }else if((d.A->sum == d.B->sum) && get_sumset_intersection_size(d.A, d.B) == 2){
                if(d.B->sum > best_solution.sum)
                    solution_build(&best_solution, &input_data, d.A, d.B);
            }
        }
        }
    }
    solution_print(&best_solution);
    return 0;
}
