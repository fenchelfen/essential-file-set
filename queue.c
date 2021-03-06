#include "queue.h"
#include <Library/UefiLib.h>

BOOLEAN list_empty(
	Queue *queue
)
{
	return NULL == queue->front;
}

VOID list_dump(
	Queue *queue
)
{
	Print(L"Queue dump: %x\r\n", queue);
	if (list_empty(queue))
		return;

	Node *current = (Node *)queue->front;

	while (NULL != current) {
		Print(L"Current node: %x\r\n", ((UINT64 *)(current->item))[0]);
		current = current->previous;
	}
}

VOID list_dump_broken(
	Queue *queue
)
{
	Print(L"Queue broken dump: %x\r\n", queue);

	UINT64 *current = list_dequeue(queue);

	while (NULL != current) {

		if (NULL != current) {
			Print(L"Current node: %x\r\n", current);
			for (int i = 0; i < 4; ++i) {
				Print(L"%x ", current[i]);
			}
			Print(L"\r\n");
		}
		// Print(L"Current node: %x\r\n", ((UINT64 *)(current->item))[0]);

		current = list_dequeue(queue);
	}
}

Queue *list_create()
{
	Queue *queue = AllocatePool(sizeof(Queue));
	queue->front = NULL;
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
	if (NULL == queue->front)
		return NULL;

	VOID *item = queue->front->item;
	Node *previous = queue->front->previous;
	FreePool(queue->front);

	queue->front = previous;
	// Print(L"...%x%x", item, previous);
	return item;
}
