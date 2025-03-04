/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
#include "aesd_ioctl.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

#define MIN(a,b) ((a) < (b) ? (a) : (b))

 /** TODO: fill in your name **/
MODULE_AUTHOR("Fernando Guerra");
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
	struct aesd_dev* dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
	filp->private_data = dev;
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */
    struct aesd_dev *dev = filp->private_data;
    
    mutex_lock(&dev->circular_buffer_lock);
    PDEBUG("read: dev->circular_buffer.in_offs %d",dev->circular_buffer.in_offs);
    PDEBUG("read: dev->circular_buffer.out_offs %d",dev->circular_buffer.out_offs);

    size_t entry_offset_byte_rtn;
    struct aesd_buffer_entry*  entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->circular_buffer, *f_pos, &entry_offset_byte_rtn);
    if(entry)
    {
        PDEBUG("read: entry valid\n");
        size_t bytes_to_read = MIN(count, entry->size - entry_offset_byte_rtn);

        if (copy_to_user(buf, entry->buffptr + entry_offset_byte_rtn, bytes_to_read))
        {
            PDEBUG("read: copy_to_user failed\n");
            mutex_unlock(&dev->circular_buffer_lock);
            return -EFAULT;
        }
        else
        {
            PDEBUG("read: copy_to_user worked\n");
            retval = bytes_to_read;
            *f_pos += bytes_to_read;
        }
    }
    else
    {
        PDEBUG("read: entry not valid\n");
    }
    mutex_unlock(&dev->circular_buffer_lock);

    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle write
     */
    struct aesd_dev *dev = filp->private_data;
    mutex_lock(&dev->circular_buffer_lock);
    PDEBUG("write: dev->circular_buffer.in_offs %d",dev->circular_buffer.in_offs);
    PDEBUG("write: dev->circular_buffer.out_offs %d",dev->circular_buffer.out_offs);
    size_t offset = 0;
    size_t new_temp_size = count;
    if(dev->temp_circular_buffer_entry.size)
    {
        PDEBUG("writing into existing buffer\n");
        // data is in temp buffer reallocate
        new_temp_size = dev->temp_circular_buffer_entry.size + count;
        dev->temp_circular_buffer_entry.buffptr = krealloc(dev->temp_circular_buffer_entry.buffptr, new_temp_size, GFP_KERNEL);
        if(dev->temp_circular_buffer_entry.buffptr == NULL)
        {
            PDEBUG("bad realloc\n");
            dev->temp_circular_buffer_entry.size = 0;
            mutex_unlock(&dev->circular_buffer_lock);
            return -ENOMEM;
        }
        else
        {
            offset = dev->temp_circular_buffer_entry.size;
        }
        memset(&dev->temp_circular_buffer_entry.buffptr[offset], 0, count);
    }
    else
    {
        PDEBUG("new buffer\n");
        dev->temp_circular_buffer_entry.buffptr = kmalloc(count, GFP_KERNEL);
        if(dev->temp_circular_buffer_entry.buffptr == NULL)
        {
            mutex_unlock(&dev->circular_buffer_lock);
            return -ENOMEM;
        }
        memset(dev->temp_circular_buffer_entry.buffptr, 0, count);
    }

    if (copy_from_user(&dev->temp_circular_buffer_entry.buffptr[offset], buf, count))
    {
        PDEBUG("copy_from_user failed\n");
        kfree(dev->temp_circular_buffer_entry.buffptr);
        dev->temp_circular_buffer_entry.size = 0;
        mutex_unlock(&dev->circular_buffer_lock);
        return -EFAULT;
    }
    else
    {
        PDEBUG("copy_from_user worked\n");
        dev->temp_circular_buffer_entry.size = new_temp_size;
    }
    // update return values assuming operations below pass

    retval = count;
    // *f_pos += count;

    // user data is copied to temp entry at this point
    char* string = strchr(dev->temp_circular_buffer_entry.buffptr, '\n');
    size_t newline_offset = 0;
    bool newline_found = false;
    for(size_t i = 0; i < dev->temp_circular_buffer_entry.size; ++i)
    {
        if(dev->temp_circular_buffer_entry.buffptr[i] == '\n')
        {
            newline_offset = i;
            newline_found = true;
            break;
        }
    }
    if(newline_found)
    {
        PDEBUG("found new line offset %d\n", string - dev->temp_circular_buffer_entry.buffptr);
        PDEBUG("found new line size %d\n", dev->temp_circular_buffer_entry.size);
        char* old_entry_mem = aesd_circular_buffer_add_entry(&dev->circular_buffer, &dev->temp_circular_buffer_entry);
        if(old_entry_mem)
        {
            kfree(old_entry_mem);
        }
        dev->temp_circular_buffer_entry.buffptr = NULL;
        dev->temp_circular_buffer_entry.size = 0;
    }
    PDEBUG("write: dev->circular_buffer.in_offs %d",dev->circular_buffer.in_offs);
    PDEBUG("write: dev->circular_buffer.out_offs %d",dev->circular_buffer.out_offs);
    mutex_unlock(&dev->circular_buffer_lock);
    return retval;
}

loff_t aesd_llseek(struct file *filp, loff_t off, int whence) 
{
    PDEBUG("llseek filp->f_pos: %d off: %d whence:%d\n", filp->f_pos, off, whence);
    loff_t new_pos;

    struct aesd_dev *dev = filp->private_data;
    mutex_lock(&dev->circular_buffer_lock);

    // get size of data currently stored.
    size_t size = 0;
    uint8_t index;
    struct aesd_buffer_entry *entry = NULL;
    AESD_CIRCULAR_BUFFER_FOREACH(entry,&aesd_device.circular_buffer,index) 
    {
        if(entry)
        {
            size += entry->size;
        }
    }

    switch (whence) {
        case SEEK_SET:
            new_pos = off;
            break;
        case SEEK_CUR:
            new_pos = filp->f_pos + off;
            break;
        case SEEK_END:
            new_pos = size + off;
            break;
        default:
            mutex_unlock(&dev->circular_buffer_lock);
            return -EINVAL;
    }

    if(new_pos > size || new_pos < 0 )
    {
        mutex_unlock(&dev->circular_buffer_lock);
        return -EINVAL;
    }

    filp->f_pos = new_pos;
    mutex_unlock(&dev->circular_buffer_lock);
    return filp->f_pos;
}

long int aesd_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long passed_seek)
{
    PDEBUG("aesd_unlocked_ioctl filp->f_pos: %d cmd: %d passed_seek:%d\n", filp->f_pos, cmd, passed_seek);
    struct aesd_dev *dev = filp->private_data;

    struct aesd_seekto seekto;
    int retval = 0;

    switch (cmd)
    {
        case AESDCHAR_IOCSEEKTO:
            if (copy_from_user(&seekto, (struct aesd_seekto *)passed_seek, sizeof(seekto)))
            {
                PDEBUG("aesd_unlocked_ioctl copy_from_user failed\n");
                retval = -EFAULT;
            }
            else
            {
                PDEBUG("aesd_unlocked_ioctl copy_from_user worked\n");

                if (mutex_lock_interruptible(&dev->circular_buffer_lock)) {
                    return -ERESTARTSYS;
                }

                size_t index_into_buffer = (dev->circular_buffer.out_offs + seekto.write_cmd) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
                PDEBUG("aesd_unlocked_ioctl index_into_buffer %d\n", index_into_buffer);

                //validate parms into buffer
                if((seekto.write_cmd >= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED) || (seekto.write_cmd_offset >= dev->circular_buffer.entry[index_into_buffer].size))
                {
                    PDEBUG("aesd_unlocked_ioctl invalid parms into buffer\n");
                    mutex_unlock(&dev->circular_buffer_lock);
                    return -EINVAL;
                }

                size_t new_pos = 0;
                size_t index = 0;
                for (int i = 0; i < seekto.write_cmd; i++) {
                    index = (dev->circular_buffer.out_offs + i) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
                    new_pos += dev->circular_buffer.entry[index].size;
                }
                new_pos += seekto.write_cmd_offset;
                filp->f_pos = new_pos;
                retval = filp->f_pos;
                mutex_unlock(&dev->circular_buffer_lock);
            }
            break;
        default:
            retval = -EFAULT;
            break;
    }

    return retval;
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
    .llseek         =    aesd_llseek,
    .unlocked_ioctl =    aesd_unlocked_ioctl,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));
    /**
     * TODO: initialize the AESD specific portion of the device
     */

    mutex_init(&aesd_device.circular_buffer_lock);
    // init circular buffer
    aesd_circular_buffer_init(&aesd_device.circular_buffer);
    // init temp entry for partial writes
    aesd_device.temp_circular_buffer_entry.buffptr= NULL;
    aesd_device.temp_circular_buffer_entry.size= 0;

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */

    if(aesd_device.temp_circular_buffer_entry.buffptr)
    {
        #ifdef __KERNEL__
            kfree((void*)aesd_device.temp_circular_buffer_entry.buffptr);
        #else
            free((void*)aesd_device.temp_circular_buffer_entry.buffptr);
        #endif 
    }

    uint8_t index;
    struct aesd_buffer_entry *entry = NULL;

    AESD_CIRCULAR_BUFFER_FOREACH(entry,&aesd_device.circular_buffer,index) 
    {
        if(entry)
        {
            #ifdef __KERNEL__
                kfree((void*)entry->buffptr);
            #else
                free((void*)entry->buffptr);
            #endif 
        }

    }
    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
