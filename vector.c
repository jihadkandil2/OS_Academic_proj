/*
 * vector.c
 *
 *  Created on: Dec 3, 2023
 *      Author: jihad
 */

#include <vector.h>

// Define a structure for the dynamic array
typedef struct {
    int *data;     // Pointer to the array
    uint32 size;    // Current number of elements in the array
    uint32 capacity; // Total capacity of the array
} Vector;

