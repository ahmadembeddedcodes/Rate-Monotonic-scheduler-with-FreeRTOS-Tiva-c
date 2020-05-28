// C program for array implementation of queue 
#include <stdio.h> 
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h> 
  
// A structure to represent a queue 
struct My_queue 
{ 
    int front, rear, size;
    unsigned capacity; 
    void** array; 
}; 
  
// function to create a queue of given capacity.  
// It initializes size of queue as 0 
struct My_queue* createQueue(unsigned capacity);
  
// Queue is full when size becomes equal to the capacity  
int isFull(struct My_queue* queue);
  
// Queue is empty when size is 0 
int isEmpty(struct My_queue* queue);
  
// Function to add an item to the queue.   
// It changes rear and size 
void enqueue(struct My_queue* queue, void* item) ;
  
// Function to remove an item from queue.  
// It changes front and size 
void* dequeue(struct My_queue* queue) ;
  
// Function to get front of queue 
void* front(struct My_queue* queue) ;
  
// Function to get rear of queue 
void* rear(struct My_queue* queue);
  
// Driver program to test above functions./ 
//int main() 
//{ 
//    struct Queue* queue = createQueue(1000); 
//  
//    enqueue(queue, 10); 
//    enqueue(queue, 20); 
//    enqueue(queue, 30); 
//    enqueue(queue, 40); 
//  
//    printf("%d dequeued from queue\n\n", dequeue(queue)); 
//  
//    printf("Front item is %d\n", front(queue)); 
//    printf("Rear item is %d\n", rear(queue)); 
//  
//    return 0; 
//}
