// C program for array implementation of queue 
#include <stdio.h> 
#include <stdlib.h> 
#include <limits.h>
#include <My_queue.h>
  
// function to create a queue of given capacity.  
// It initializes size of queue as 0 
struct My_queue* createQueue(unsigned capacity) 
{ 
    struct My_queue* queue = (struct My_queue*) pvPortMalloc(sizeof(struct My_queue)); 
    queue->capacity = capacity; 
    queue->front = queue->size = 0;  
    queue->rear = capacity - 1;  // This is important, see the enqueue 
    queue->array = (void**)pvPortMalloc(queue->capacity * sizeof(void*)); 
    return queue; 
} 
  
// Queue is full when size becomes equal to the capacity  
int isFull(struct My_queue* queue) 
{  return (queue->size == queue->capacity);  } 
  
// Queue is empty when size is 0 
int isEmpty(struct My_queue* queue) 
{  return (queue->size == 0); } 
  
// Function to add an item to the queue.   
// It changes rear and size 
void enqueue(struct My_queue* queue, void* item) 
{ 
    if (isFull(queue)) 
        return; 
    queue->rear = (queue->rear + 1)%queue->capacity; 
    queue->array[queue->rear] = item; 
    queue->size = queue->size + 1; 
    //printf("%d enqueued to queue\n", item); 
} 
  
// Function to remove an item from queue.  
// It changes front and size 
void* dequeue(struct My_queue* queue) 
{ 
    if (isEmpty(queue)) 
        return NULL; 
    void* item = queue->array[queue->front]; 
    queue->front = (queue->front + 1)%queue->capacity; 
    queue->size = queue->size - 1; 
    return item; 
} 
  
// Function to get front of queue 
void* front(struct My_queue* queue) 
{ 
    if (isEmpty(queue)) 
        return NULL; 
    return queue->array[queue->front]; 
} 
  
// Function to get rear of queue 
void* rear(struct My_queue* queue) 
{ 
    if (isEmpty(queue)) 
        return NULL; 
    return queue->array[queue->rear]; 
} 
  
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
