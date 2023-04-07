#include <unistd.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef struct node_t node_t;
typedef struct list_t list_t;

static list_t list;
static node_t *free_head;

struct node_t
{
    size_t size;
    node_t *next;
    node_t *next_free;
    char data[];
};

struct list_t // The list :)
{
    node_t *head;
    node_t *tail;
};

static void *new_node(size_t size)
{
    node_t *node = (node_t *)sbrk(sizeof(node_t) + size);
    node->size = size;

    if (list.tail != NULL)
        list.tail->next = node;
    else
        list.head = node;

    list.tail = node;
    return node->data;
}

// Splits a node into two
static void split_node(node_t *node, size_t size)
{
    node_t *new_node = (node_t *)((char *)node + sizeof(node_t) + size);
    new_node->next = node->next;
    new_node->size = node->size - size - sizeof(node_t);

    node->next = new_node;
    new_node->next_free = node->next_free;
    node->next_free = new_node;
    if (list.tail == node)
        list.tail = new_node;

    node->size = size;
}

// Joins two neighbouring nodes together
static void coalesce(node_t *node) // Takes left node
{
    node_t *rm_node = node->next;
    node->size = node->size + sizeof(node_t) + rm_node->size;
    node->next_free = rm_node->next_free;
    node->next = rm_node->next;
    if (list.tail == rm_node)
        list.tail = node;
}

void free(void *ptr)
{

    if (ptr == NULL)
        return;

    node_t *free_node = ptr - sizeof(node_t);
    node_t *prev = NULL;
    node_t *node = free_head; // Set node to the first free node in our linked list of free node

    while (node != NULL && node < free_node) // Iterate until prev is before free_node in memory, and node is after in memory
    {
        prev = node;
        node = node->next_free;
    }

    free_node->next_free = node; // Node is first free node that is ahead of free_node
    if (prev != NULL)
    {
        prev->next_free = free_node; // prev is the closest node to free_node that is behind free_node
    }
    else // If prev is NULL, then free_node becomes the head of our free list
    {
        free_head = free_node;
    }

    if (prev != NULL && prev->next == free_node) //If prev and free_node are neighbours, we coalesce
    {
        coalesce(prev);
        free_node = prev;
    }

    if (node != NULL && free_node->next == node) //If free_node and node are neighbours, we also coalesce
        coalesce(free_node);
}

void *malloc(size_t size)
{
    if (size == 0)
        return NULL;

    node_t *prev = NULL;
    node_t *node = free_head;
    while (node != NULL) //Iterate through free list until we find a large enough node
    {
        if (size <= node->size)
        {
            if ((size + sizeof(node_t)) * 2 < node->size + sizeof(node_t)) // check if node->size is large enough to split
                split_node(node, size);

            if (prev != NULL)
            {
                prev->next_free = node->next_free;
            }
            else
            {
                free_head = node->next_free;
            }

            return node->data;
        }
        prev = node;
        node = node->next_free;
    }
    return new_node(size); //If there are no large enough free nodes, allocate a new node.
}

void *calloc(size_t nitems, size_t size)
{
    void *data = malloc(nitems * size);
    if (data != NULL)
        memset(data, 0, nitems * size);
    return data;
}

void *realloc(void *ptr, size_t size)
{
    if (ptr == NULL)
        return malloc(size);

    if (size == 0)
    {
        free(ptr);
        return NULL;
    }

    node_t *realloc_node = ptr - sizeof(node_t);

    if (size <= realloc_node->size)
    {
        if ((size + sizeof(node_t)) * 2 < realloc_node->size + sizeof(node_t)) // check if node->size is large enough to split
            split_node(realloc_node, size);

        return realloc_node->data;
    }

    char *data = malloc(size);
    memcpy(data, ptr, realloc_node->size);
    free(ptr);
    return data;
}
