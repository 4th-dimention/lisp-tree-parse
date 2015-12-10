/*
 * Simple Tree Parser
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

struct Data{
    char *data;
    int size;
};

Data
open_file(const char *filename){
    Data result;
    FILE *file;

    result = {};
    file = fopen(filename, "rb");
    if (file){
        fseek(file, 0, SEEK_END);
        result.size = ftell(file);
        fseek(file, 0, SEEK_SET);

        if (result.size > 0){
            result.data = (char*)malloc(result.size);
            fread(result.data, result.size, 1, file);
        }

        fclose(file);
    }

    return(result);
}

enum Node_Type{
    NT_Root,
    NT_Group,
    NT_Word,
    // never below this
    NT_Count
};

struct Node{
    int type;
    int parent;
    int first_child;
    int next_sibling;
    int word_start, word_end;
};

struct Tree{
    Node *nodes;
    int count, max;
};

struct Stack{
    int *stack;
    int top, max;
};

struct Parse_State{
    int pos;
    
    int has_word_start;
    int word_start;

    Stack parent;
    Stack write_head;
};

struct Parse_Step{
    int finished;
    int memory;
};

void*
parse_provide_memory(Tree *tree, void *mem, int size){
    void *result;
    result = tree->nodes;
    assert(size > tree->max*sizeof(Node));
    memcpy(mem, result, tree->count*sizeof(Node));
    tree->nodes = (Node*)mem;
    tree->max = size / sizeof(Node);
    return(result);
}

int
is_whitespace(char c){
    int result;
    result = (c == ' ' || c == '\n' || c == '\t' ||
              c == '\r' || c == '\f' || c == '\v');
    return(result);
}

int
push_node(Parse_State *state, Tree *tree, Node node, int parent){
    int result;
    Node *prev;
    prev = tree->nodes + state->write_head.stack[state->write_head.top-1];

    if (tree->count - 1 == parent){
        assert(prev->first_child == 0);
        prev->first_child = tree->count;
    }
    else{
        assert(prev->next_sibling == 0);
        prev->next_sibling = tree->count;
    }

    result = (tree->count++);
    tree->nodes[result] = node;
    state->write_head.stack[state->write_head.top-1] = result;
    return(result);
}

void
push_word(Parse_State *state, Tree *tree, int parent, int start, int end){
    Node node;
    
    assert(tree->count < tree->max);

    node.type = NT_Word;
    node.parent = parent;
    node.first_child = 0;
    node.next_sibling = 0;
    node.word_start = start;
    node.word_end = end;

    push_node(state, tree, node, parent);
}

void
stack_push(Stack *stack, int x){
    assert(stack->top < stack->max);
    stack->stack[stack->top++] = x;
}

void
push_group(Parse_State *state, Tree *tree, int parent, int start){
    Node node;
    int new_parent;
    
    assert(tree->count < tree->max);

    node.type = NT_Group;
    node.parent = parent;
    node.first_child = 0;
    node.next_sibling = 0;
    node.word_start = start;

    new_parent = push_node(state, tree, node, parent);

    stack_push(&state->parent, new_parent);
    stack_push(&state->write_head, new_parent);
}

void
pop_group(Parse_State *state, Tree *tree, int parent, int end){
    assert(state->parent.top > 1);
    assert(state->parent.top == state->write_head.top);
    --state->parent.top;
    --state->write_head.top;
    assert(state->parent.stack[state->parent.top] == parent);
    tree->nodes[parent].word_end = end;
}


Parse_Step
parse_step(Parse_State *state, Tree *tree, Data data){
    Parse_Step result;
    int pos;
    int end_word, start_word;
    int end_group, start_group;
    int whitespace_zoom;
    int parent;
    char c;
    
    result = {};
    
    if (tree->count + 2 >= tree->max){
        result.memory = (tree->max+1)*2*sizeof(Node);
    }
    else{
        pos = state->pos;
        parent = state->parent.stack[state->parent.top - 1];

        end_word = 0;
        start_word = 0;
        end_group = 0;
        start_group = 0;
        whitespace_zoom = 0;

        if (pos < data.size){
            c = data.data[pos];
            if (is_whitespace(c)){
                end_word = 1;
                whitespace_zoom = 1;
            }
            else if (c == '('){
                end_word = 1;
                start_group = 1;
                ++pos;
            }
            else if (c == ')'){
                end_word = 1;
                end_group = 1;
                ++pos;
            }
            else{
                start_word = 1;
            }
        }
        else{
            end_word = 1;
            result.finished = 1;
        }
        
        if (end_word){
            if (state->has_word_start){
                push_word(state, tree, parent, state->word_start, pos);
                state->has_word_start = 0;
            }
        }
        
        if (start_word){
            state->has_word_start = 1;
            state->word_start = pos;
            for (++pos; pos < data.size; ++pos){
                c = data.data[pos];
                if (is_whitespace(c) || c == '(' || c == ')'){
                    break;
                }
            }
        }
        
        if (end_group){
            pop_group(state, tree, parent, pos-1);
        }

        if (start_group){
            push_group(state, tree, parent, pos-1);
        }
        
        if (whitespace_zoom){
            for (;pos < data.size && is_whitespace(data.data[pos]); ++pos);
        }
        
        state->pos = pos;
    }
    
    return(result);
}

int main(){
    Parse_State parse;
    Parse_Step step;
    Tree tree;
    Data file;
    char *filename;
    void *mem;

    filename = "test.txt";
    file = open_file(filename);
    if (file.data == 0){
        printf("could not open %s\n", filename);
        return(1);
    }
    
    tree = {};
    tree.max = 1024;
    tree.nodes = (Node*)malloc(tree.max*sizeof(Node));
    memset(tree.nodes, 0, tree.max*sizeof(Node));
    tree.nodes[0].type = NT_Root;
    tree.count = 1;
    
    parse = {};
    
    parse.parent.max = 64;
    parse.parent.stack = (int*)malloc(parse.parent.max*sizeof(int));
    memset(parse.parent.stack, 0, parse.parent.max*sizeof(int));
        
    parse.write_head.max = 64;
    parse.write_head.stack = (int*)malloc(parse.write_head.max*sizeof(int));
    memset(parse.write_head.stack, 0, parse.write_head.max*sizeof(int));
    
    
    stack_push(&parse.parent, 0);
    stack_push(&parse.write_head, 0);
    
    for (step = {};
         step.finished == 0;){
        step = parse_step(&parse, &tree, file);
        
        if (step.memory){
            mem = malloc(step.memory);
            mem = parse_provide_memory(&tree, mem, step.memory);
            free(mem);
        }
    }
    
    
    return(0);
}



