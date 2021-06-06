#include "queue.h"

BOOLEAN list_empty(
	Queue *queue
)
{
	return NULL == queue->front;
}

Queue *list_create()
{
	Queue *queue = AllocatePool(sizeof(Queue));
	return queue;
}

VOID list_enqueue(
	Queue *queue,
	VOID *item
)
/*
	Put at the back of the queue
*/
{
	Node *node = AllocatePool(sizeof(Node));
	node->item = item;
	node->previous = queue->front;
	queue->front = node;
}

VOID *list_dequeue(
	Queue *queue
)
/*
	Pop an element at the front of the queue
*/
{
	VOID *item = queue->front->item;
	Node *previous = queue->front->previous;
	FreePool(queue->front);

	queue->front = previous;
	return item;
}

