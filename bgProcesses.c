#include <stdio.h>
#include <stdlib.h>

typedef struct node {
    pid_t val;
    struct node * next;
} node_t;


node_t *head = NULL;

void push_process( pid_t val) {
    node_t * new_node;
    new_node = malloc(sizeof(node_t));
    
    new_node->val = val;
    new_node->next = head;
    head = new_node;
}

pid_t pop_process(void) {
    pid_t retval = -1;
    node_t * next_node = NULL;
    
    if (head == NULL) {
        return -1;
    }
    
    next_node = (head)->next;
    retval = (head)->val;
    free(head);
    head = next_node;
    
    return retval;
}



pid_t last_process(void)
{
    return (head)->val;
}







