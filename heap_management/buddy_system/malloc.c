#include <unistd.h>
#include <stddef.h>
#include <math.h>
#include <string.h>

#include <stdio.h>
#include <signal.h>

// Compiler bug, add -fno-optimize-strlen to CFLAGS in gawk makefile

typedef struct node_t node_t;
typedef enum side
{
    LEFT,
    RIGHT
} side_t;

// Smaller BASE size causes some tests to fail. Possibly something to do wity byte alignment?
static size_t BASE = 128;   //Size of smallest memory block
static size_t N;            //Our heap size
static size_t MAX_ORDER;    //When we are on this order, we have one memory block which can be split into more blocks.
static node_t *head;

struct node_t
{
    char order; //current order
    char free;  //Allocated vs. not
    char side; // left or right side
    char data[]; //Shorthand to get to the memory after node_t (the user's data) 
};


void init()
{
    N = pow(2, 23); // 8192 KB, this is defualt heap size on Ubuntu. Granted, some will be used up by node_t overhead.
    MAX_ORDER = log2(N / BASE);
    head = sbrk(N);    //Moves program break forward by N bytes. 
    head->order = MAX_ORDER;
    head->side = LEFT;
    head->free = 1;
}


//Splits the memory block into two equally sized blocks, with a LEFT and RIGHT. 
static void split_node(node_t *node)
{
    node->order = node->order - 1;
    node_t *new_node = (node_t *)((char *)node + (size_t)(BASE * pow(2, node->order))); //set new_node to point to the middle of our old block
    new_node->order = node->order;
    new_node->side = RIGHT;
    node->side = LEFT;
    node->free = 1;
    new_node->free = 1;
}

//Coalesce two buddies into one block
static void coalesce(node_t *node)
{
    node->order++;
    // free is already set for this node
    //  Take node distance from head, divide by buddy size. If even, left, else, right.
    node->side = (side_t)(((char *)node - (char *)head) / (BASE * pow(2, node->order))) % 2;
}


//Finds a free block of appropriate size. 
node_t *block_finder(short order)
{
    while (order <= MAX_ORDER)
    {
        node_t *node = head;
        size_t step_size = BASE * pow(2, order); //order increases with each iteration, making step_size become each different size of blocks until a free block is found. 
        while ((char *)node < (char *)head + N)  //If we get to the program break, we don't have enough free memory
        {
            if (node->free && node->order == order) //if the node is free and of the order we want, we're done
            {
                return node;
            }
            else if (node->order > order) //If the order of the node is bigger, we can't use step_size
            {
                node = (node_t *)((char *)node + (size_t)(BASE * pow(2, node->order)));
            }
            else //Move forward to the next block using step_size
            {
                node = (node_t *)((char *)node + step_size);
            }
        }
        order++; //We found no block of the desired order, look for bigger ones
    }
    return NULL;
}

void free(void *ptr)
{
    if (ptr == NULL)
        return;

    node_t *node = ptr - sizeof(node_t);
    node->free = 1;

    while (node->free && node->order < MAX_ORDER) //Coalesce buddies if possible
    {
        if (node->side == RIGHT) //If node is right, we find the left and go into next iteration.
        {
            node = (node_t *)((char *)node - (size_t)(BASE * pow(2, node->order)));
        }
        else //We have the left node, and the left node is free
        {
            node_t *right_node = (node_t *)((char *)node + (size_t)(BASE * pow(2, node->order)));
            if (right_node->free && node->order == right_node->order) //if both left and right are free and of same order, coalesce
            {
                coalesce(node);
            }
            else //If not of same order, or if right node is not free, coalesce is not possible
            {
                break;
            }
        }
    }
}

void *malloc(size_t size)
{
    if (size == 0)
        return NULL;

    short order = size + sizeof(node_t) <= BASE ? 0 : ceil(log2(((double)(size + sizeof(node_t))) / BASE)); //Calculate which block order is needed

    if (head == NULL)
    {
        init();
    }

    node_t *node = block_finder(order); //Finds block of at least order

    if (node != NULL) //If we found a block:
    {
        while (node->order > order) //While the node order is larger than what we need, keep splitting the node
        {
            split_node(node);
        }
        node->free = 0;
        return node->data;
    }
    return NULL;
}

void *calloc(size_t nitems, size_t size) //Mallocs and sets data to 0's
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
    short order = size + sizeof(node_t) <= BASE ? 0 : ceil(log2(((double)(size + sizeof(node_t))) / BASE)); //The order neede for the new size the user wants
    if (order <= realloc_node->order) //If the new order needed is smaller than the old one, we can split our node.
    {
        while (order < realloc_node->order)
        {
            split_node(realloc_node);
        }
        return realloc_node->data;
    }

    //If the order needed is larger than the old one, we need to allocate new memory
    char *data = malloc(size);
    memcpy(data, ptr, BASE * pow(2, realloc_node->order) - sizeof(node_t)); //copies data to new memory location
    free(ptr);  //frees the previous memory location
    return data;
}