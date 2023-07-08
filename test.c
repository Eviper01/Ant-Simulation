#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

struct node {
uint64_t data;
struct node* next;

};


int main() {

//allocate three elements to the list
    struct node first_element;
    struct node second_element;
    struct node third_element;
    first_element.data = 0xdeadbeef; 
    second_element.data = 0xaeaeaeae;
    third_element.data = 0xbeefdead;

    first_element.next  = &second_element;
    second_element.next = &third_element;
    third_element.next  = NULL; 

    struct node current = first_element;
    while(current.next != NULL) {
        printf("%lx\n", current.data);
        current = *current.next;
    }
        
        printf("%lx\n", current.data);


    return 0;

} 
