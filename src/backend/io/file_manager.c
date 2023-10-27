#include "file_manager.h"
#include "../../utils/logger.h"
#include <fcntl.h>
#include <inttypes.h>
#include <stdlib.h>
#include <sys/mman.h>


char* filename = NULL;
int fd;
uint64_t file_size;
void* mmaped_data;
off_t page_offset = 0;

void* get_mmaped_data(){
    return mmaped_data;
}

enum FileStatus {FILE_FAIL=-1, FILE_SUCCESS=0};


/**
 * File initialization
 * @param filename
 * @return FILE_SUCCESS or FILE_FAIL
 */

int init_file(const char* file_name){
    filename = (char*)malloc(strlen(file_name)+1);
    strcpy(filename, file_name);
    logger(LL_INFO, __func__ ,"Opening file %s.", filename);
    fd = open(filename,
                           O_RDWR | // Read/Write mode
                           O_CREAT, // Create file if it does not exist
                           S_IWUSR | // User write permission
                           S_IRUSR // User read permission
    );
    if (fd == -1){
        logger(LL_ERROR, __func__ ,"Unable to open file.");
        return FILE_FAIL;
    }
    file_size = (uint64_t) lseek(fd, 0, SEEK_END);
    if(file_size != 0){
        if(mmap_page(page_offset) == FILE_FAIL){
            logger(LL_ERROR, __func__, "Unable map file");
            return FILE_FAIL;
        }
    }
    return FILE_SUCCESS;
}


/**
 * Map File
 * @param offset
 * @return FILE_SUCCESS or FILE_FAIL
 */

int mmap_page(off_t offset){
    logger(LL_INFO, __func__,
           "Mapping page from file with descriptor %d and file size %" PRIu64, fd, file_size);
    if(file_size == 0){
        return FILE_FAIL;
    }

    if((mmaped_data = mmap(NULL, PAGE_SIZE,
                                            PROT_WRITE |
                                            PROT_READ,
                           MAP_SHARED, fd, offset)) == MAP_FAILED){
        logger(LL_ERROR, __func__ , "Unable to map file: %s %d.", strerror(errno), errno);
        return FILE_FAIL;
    };
    page_offset = offset;
    return FILE_SUCCESS;
}

/**
 * Synchronize a mapped region
 * @return FILE_SUCCESS or FILE_FAIL
 */

int sync_page(){
    logger(LL_INFO, __func__, "Syncing page from file with fd %d.", fd);
    if(msync(mmaped_data, PAGE_SIZE, MS_ASYNC) == -1){
        logger(LL_ERROR, __func__ , "Unable sync page: %s %d.", strerror(errno), errno);
        return FILE_FAIL;
    }
    return FILE_SUCCESS;
}

/**
 * Unmap page
 * @return FILE_SUCCESS or FILE_FAIL
 */

int unmap_page(){
    if(file_size == 0){
        return FILE_SUCCESS;
    }

    logger(LL_INFO, __func__,
           "Unmapping page from file with pointer %p and file size %" PRIu64,
           mmaped_data, file_size);
    if(sync_page() == FILE_FAIL){
        return FILE_FAIL;
    }
    if(munmap(mmaped_data, PAGE_SIZE) == -1){
        logger(LL_ERROR, __func__, "Unable unmap page: %s %d.", strerror(errno), errno);
        return FILE_FAIL;
    };
    mmaped_data = NULL;
    return FILE_SUCCESS;
}

/**
 * Close file
 * @return FILE_SUCCESS or FILE_FAIL
 */
int close_file(){
    if(unmap_page() == FILE_FAIL){
        logger(LL_ERROR, __func__, "Unable unmap file.");
        return FILE_FAIL;
    }
    close(fd);
    fd = -1;
    file_size = -1;
    free(filename);
    return FILE_SUCCESS;
}

/**
 * Delete file
 * @return FILE_SUCCESS or FILE_FAIL
 */
int delete_file(){
    logger(LL_INFO, __func__, "Deleting file with name %s.", filename);
    if(unlink(filename) == -1){
        logger(LL_ERROR, __func__, "Unable delete file with name %s.", filename);
        return FILE_FAIL;
    }
    if(close_file() == FILE_FAIL){
        logger(LL_ERROR, __func__, "Unable close file.");
        return FILE_FAIL;
    }
    return FILE_SUCCESS;
}

/**
 * Truncate or extend a file to a page size
 * @param new_size
 * @return FILE_SUCCESS or FILE_FAIL
 */

int init_page(){
    logger(LL_INFO, __func__ , "Init new page");
    if(mmaped_data && unmap_page() == FILE_FAIL){
        logger(LL_ERROR, __func__, "Unable unmap file.");
        return FILE_FAIL;
    }
    if(ftruncate(fd,  (off_t) (file_size + PAGE_SIZE)) == -1){
        logger(LL_ERROR, __func__, "Unable change file size: %s %d", strerror(errno), errno);
        return FILE_FAIL;
    }
    page_offset = (off_t)file_size;
    file_size += PAGE_SIZE;
    if(mmap_page(page_offset) == FILE_FAIL){
        logger(LL_ERROR, __func__, "Unable to mmap file.");
    }
    return FILE_SUCCESS;
}

/**
 * Copies size bytes from memory area src to mapped_file_page and make synchronization with
 * original file.
 * @param src
 * @param size
 * @param offset - page offset
 * @return FILE_SUCCESS or FILE_FAIL
 */

int write_page(void* src, uint64_t size, off_t offset){
    logger(LL_INFO, __func__ , "Writing to fd %d with size %"PRIu64 "%"PRIu64 " bytes.", fd, file_size, size);
    if(mmaped_data == NULL){
        logger(LL_ERROR, __func__, "Unable write, mapped file is NULL.");
        return FILE_FAIL;
    }
    memcpy(mmaped_data + offset, src, size);
    sync_page();
    return FILE_SUCCESS;
}

/**
 * Copies size bytes to memory area dest from mapped_file_page.
 * @param dest
 * @param size
 * @param offset
 * @return FILE_SUCCESS or FILE_FAIL
 */

int read_page(void* dest, uint64_t size, off_t offset){
    logger(LL_INFO, __func__ , "Reading from fd %d with size %"PRIu64 "%"PRIu64 " bytes.", fd, file_size, size);
    if(mmaped_data == NULL){
        logger(LL_ERROR, __func__, "Unable write, mapped file is NULL.");
        return FILE_FAIL;
    }
    memcpy(dest, mmaped_data + offset, size);
    return FILE_SUCCESS;
}