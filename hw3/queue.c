#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "queue.h"

static void copy_to_node(Item item, Node *pn);
static void copy_to_item(Node *pn, Item *pi);

void initialize_queue(Queue *pq)
{
	pq->front = NULL;
    pq->rear = NULL;
	pq->items = 0;
}

int queue_is_full(const Queue *pq)
{
	return pq->items == MAXQUEUE;
}

int queue_is_empty(const Queue *pq)
{
	return pq->items == 0;
}

int queue_item_count(const Queue *pq)
{
	return pq->items;
}

int enqueue(Item item, Queue *pq)
{
	// add missing stuff
	if(queue_is_full(pq))
		return -1;
	Node* n = (Node*) malloc(sizeof(Node));
	if(!n)
		return -1;
	copy_to_node(item, n);
	n->next = NULL;
	if(queue_is_empty(pq))
	{
		pq->front = n;
	}else
	{
		pq->rear->next = n;
	}
	pq->rear = n;
	pq->items++;
    return 0;
}

int dequeue(Item *pitem, Queue *pq)
{
	// add missing stuff
	Node *elem=pq->front;
	if(queue_is_empty(pq))
		return -1;
	copy_to_item(pq->front, pitem);
	//if it only consists of one Node
	pq->front = pq->front->next;
	free(elem);
	if(pq->items == 1)
	{
		pq->rear = NULL;
	}

	//if last element is to be removed		
	pq->items--;

	return 0;
}

int dequeue_back(Item *pitem, Queue *pq)
{
    if(queue_is_empty(pq))
		return -1;
    //if only one item
    Node *cursor = pq->front;
    if(pq->items == 1)
    {
        copy_to_item(pq->front, pitem);
        pq->rear = NULL;
        pq->front = NULL;
        free(cursor);
        pq->items--;
        return 0;
    }
    while(cursor->next->next != NULL)
    {
        cursor = cursor->next;
    }
    copy_to_item(cursor->next, pitem);
    cursor->next = NULL;
    pq->rear = cursor;
    pq->items--;
    return 0;
}

void printq(Queue *pq)
{
	Node *cursor = pq->front;
	printf("content of the queue: ");
	while(cursor!= NULL)
	{
		printf("%s ", cursor->item);
		cursor = cursor->next;
	}
	printf("\n");
}

void empty_queue(Queue *pq)
{
	Item dummy;
	while (!queue_is_empty(pq)) {
		dequeue(&dummy, pq);
	}
}

static void copy_to_node(Item item, Node *pn)
{
	strcpy(pn->item, item);
}

static void copy_to_item(Node *pn, Item *pi)
{
	strcpy(*pi, pn->item);
}
