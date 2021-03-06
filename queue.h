#include "Uefi.h"
#include "Library/MemoryAllocationLib.h"


typedef struct Node {
	VOID *item;
	struct Node *previous;
} Node;

typedef struct Queue {
	Node *front;
} Queue;

BOOLEAN list_empty(Queue * queue);

VOID list_dump(Queue * queue);

VOID list_dump_broken(Queue * queue);

Queue *list_create();

VOID list_enqueue(Queue * queue, VOID * item);

VOID *list_dequeue(Queue * queue);
