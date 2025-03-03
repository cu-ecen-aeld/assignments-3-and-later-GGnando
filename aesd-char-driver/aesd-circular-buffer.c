/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#include "aesd-circular-buffer.h"

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
            size_t char_offset, size_t *entry_offset_byte_rtn )
{
    // size_t current_offset = 0;
    // uint32_t index;
    // uint32_t total_entries;
    // if(buffer->full)
    // {
    //     total_entries = AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    // }
    // else if(buffer->in_offs > buffer->out_offs)
    // {
    //     total_entries = buffer->in_offs - buffer->out_offs;
    // }
    // else
    // {
    //     total_entries = AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED - buffer->out_offs + buffer->in_offs;
    // } 
    
    // index = buffer->out_offs;

    // for (uint32_t i = 0; i < total_entries; ++i) {
    //     struct aesd_buffer_entry *entry = &buffer->entry[index];
        
    //     if (char_offset < (current_offset + entry->size)) {
    //         *entry_offset_byte_rtn = char_offset - current_offset;
    //         return entry;
    //     }
    //     current_offset += entry->size;
    //     index = (index + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    // }

    for (int i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++)
    {
        int entry_i = (buffer->out_offs + i) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

        if (buffer->entry[entry_i].buffptr == NULL)
        {
            return NULL;
        }

        if (buffer->entry[entry_i].size > char_offset)
        {
            *entry_offset_byte_rtn = char_offset;
            return &buffer->entry[entry_i];
        }
        else
        {
            char_offset -= buffer->entry[entry_i].size;
        }
    }
    return NULL;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
char* aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    char* memory_return_ptr = buffer->entry[buffer->in_offs].buffptr;
    if (buffer->full) {
        buffer->out_offs = (buffer->out_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }
    
    buffer->entry[buffer->in_offs] = *add_entry;
    buffer->in_offs = (buffer->in_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    
    if (buffer->in_offs == buffer->out_offs) {
        buffer->full = true;
    }
    else
    {
        buffer->full = false;
    }
    return memory_return_ptr;
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}
