#include "emufs-disk.h"
#include "emufs.h"

/* ------------------- In-Memory objects ------------------- */

int init=0; //if the file/directory handles arrays are initialized or not

struct file_t
{
	int offset;		                // offset of the file
	int inode_number;	            // inode number of the file in the disk
	int mount_point;    			// reference to mount point
                                    // -1: Free
                                    // >0: In Use
};

struct directory_t
{
    int inode_number;               // inode number of the directory in the disk
    int mount_point;    			// reference to mount point
                                    // -1: Free
                                    // >0: In Use
};


struct directory_t dir[MAX_DIR_HANDLES];    // array of directory handles
struct file_t files[MAX_FILE_HANDLES];      // array of file handles

int closedevice(int mount_point){
    /*
        * Close all the associated handles
        * Unmount the device
        
        * Return value: -1,     error
                         1,     success
    */

    for(int i=0; i<MAX_DIR_HANDLES; i++)
        dir[i].mount_point = (dir[i].mount_point==mount_point ? -1 : dir[i].mount_point);
    for(int i=0; i<MAX_FILE_HANDLES; i++)
        files[i].mount_point = (files[i].mount_point==mount_point ? -1 : files[i].mount_point);
    
    return closedevice_(mount_point);
}

int create_file_system(int mount_point, int fs_number){
    /*
	   	* Read the superblock.
        * Update the mount point with the file system number
	    * Set file system number on superblock
		* Clear the bitmaps.  values on the bitmap will be either '0', or '1'. 
        * Update the used inodes and blocks
		* Create Inode 0 (root) in metadata block in disk
		* Write superblock and metadata block back to disk.

		* Return value: -1,		error
						 1, 	success
	*/
    struct superblock_t superblock;
    read_superblock(mount_point, &superblock);

    update_mount(mount_point, fs_number);

    superblock.fs_number=fs_number;
    for(int i=3; i<MAX_BLOCKS; i++)
        superblock.block_bitmap[i]=0;
    for(int i=0; i<3; i++)
        superblock.block_bitmap[i]=1;
    for(int i=1; i<MAX_INODES; i++)
        superblock.inode_bitmap[i]=0;
    superblock.inode_bitmap[0]=1;
    superblock.used_blocks=3;
    superblock.used_inodes=1;
    write_superblock(mount_point, &superblock);

    struct inode_t inode;
    memset(&inode,0,sizeof(struct inode_t));
    inode.name[0]='/';
    inode.parent=255;
    inode.type=1;
    write_inode(mount_point, 0, &inode);
}

int alloc_dir_handle(){
    /*
        * Initialize the arrays if not already done
        * check and return if there is any free entry
        
		* Return value: -1,		error
						 1, 	success
    */
    if(init==0){
        for(int i=0; i<MAX_DIR_HANDLES; i++)
            dir[i].mount_point = -1;
        for(int i=0; i<MAX_FILE_HANDLES; i++)
            files[i].mount_point = -1;
        init=1;
    }
    for(int i=0; i<MAX_DIR_HANDLES; i++)
        if(dir[i].mount_point==-1)
            return i;
    return -1;
}

int alloc_file_handle(){
    for(int i=0; i<MAX_FILE_HANDLES; i++)
        if(files[i].mount_point==-1)
            return i;
    return -1;
}

int goto_parent(int dir_handle){
    /*
        * Update the dir_handle to point to the parent directory
        
		* Return value: -1,		error   (If the current directory is root)
						 1, 	success
    */

    struct inode_t inode;
    read_inode(dir[dir_handle].mount_point, dir[dir_handle].inode_number, &inode);
    if(inode.parent==255)
        return -1;
    dir[dir_handle].inode_number = inode.parent;
    return 1;
}

int open_root(int mount_point){
    /*
        * Open a directory handle pointing to the root directory of the mount
        
		* Return value: -1,		            error (no free handles)
						 directory handle, 	success
    */

    int dir_handle = alloc_dir_handle();
    if(dir_handle==-1)
        return -1;
    dir[dir_handle].mount_point = mount_point;
    dir[dir_handle].inode_number = 0;
    return dir_handle;
}

int return_inode(int mount_point, int inodenum, char* path){
    /*
        * Parse the path 
        * Search the directory to find the matching entity
        
		* Return value: -1,		        error
						 inode number, 	success
    */

    // start from root directory
    if(path[0]=='/')
        inodenum=0;

    struct inode_t inode;
    read_inode(mount_point, inodenum, &inode);

    // the directory to start with is not a directory
    if(inode.type==0)
        return -1;

    int ptr1=0, ptr2=0;
    char buf[MAX_ENTITY_NAME];
    memset(buf,0,MAX_ENTITY_NAME);
    
    while(path[ptr1]){
        if(path[ptr1]=='/'){
            ptr1++;
            continue;
        }
        if(path[ptr1]=='.'){
            ptr1++;
            if(path[ptr1]=='/' || path[ptr1]==0)
                continue;
            if(path[ptr1]=='.'){
                ptr1++;
                if(path[ptr1]=='/' || path[ptr1]==0){
                    if(inodenum==0)
                        return -1;
                    inodenum = inode.parent;
                    read_inode(mount_point, inodenum, &inode);
                    continue;
                }
            }
            return -1;
        }
        while(1){
            int found=0;
            buf[ptr2++]=path[ptr1++];
            if(path[ptr1]==0 || path[ptr1]=='/'){
                for(int i=0; i<inode.size; i++){
                    struct inode_t entry;
                    read_inode(mount_point, inode.mappings[i], &entry);
                    if(memcmp(buf,entry.name,MAX_ENTITY_NAME)==0){
                        inodenum = inode.mappings[i];
                        inode = entry;
                        if(path[ptr1]=='/')
                            if(entry.type==0)
                                return -1;
                        ptr2=0;
                        memset(buf,0,MAX_ENTITY_NAME);
                        found=1;
                        break;
                    }
                }
                if(found)
                    break;
                return -1;
            }
            if(ptr2==MAX_ENTITY_NAME)
                return -1;
        }
    }
    return inodenum;
}

int change_dir(int dir_handle, char* path){
    /*
        * Update the handle to point to the directory denoted by path
        * You should use return_inode function to get the required inode

		* Return value: -1,		error
						 1, 	success
    */

    int mount_point = dir[dir_handle].mount_point;

    // Retrieve the inode number for the directory path
    int inode_number = return_inode(mount_point, dir[dir_handle].inode_number, path);
    if (inode_number == -1) {
        return -1;  // If the path is not found, return error
    }

    // Read the inode details of the found directory
    struct inode_t inode;
    read_inode(mount_point, inode_number, &inode);

    // If the inode represents a directory, update the dir_handle's inode number
    if (inode.type == 1) {  // Type 1 indicates it's a directory
    // printf("inside inode type, inode number: %d\n", inode_number);
        dir[dir_handle].inode_number = inode_number;
        return inode_number;  // Successfully changed the directory
    }

    // If it's not a directory, return an error
    return -1;
}

void emufs_close(int handle, int type){
    /*
        * type = 1 : Directory handle and 0 : File Handle
        * Close the file/directory handle
    */

    // Check if it is a directory handle
    if (type == 1) {
        // Reset the mount point for the directory handle to -1
        memset(&dir[handle], 0, sizeof(struct directory_t));
    } else {
        // Reset the mount point for the file handle to -1
        memset(&files[handle], 0, sizeof(struct file_t));
    }
}

int delete_entity(int mount_point, int inodenum){
    /*
        * Delete the entity denoted by inodenum (inode number)
        * Close all the handles associated
        * If its a file then free all the allocated blocks
        * If its a directory call delete_entity on all the entities present
        * Free the inode
        
        * Return value : inode number of the parent directory
    */

    struct inode_t inode;
    read_inode(mount_point, inodenum, &inode);
    if(inode.type==0){
        for(int i=0; i<MAX_FILE_HANDLES; i++)
            if(files[i].inode_number==inodenum)
                files[i].mount_point=-1;
        int num_blocks = inode.size/BLOCKSIZE;
        if(num_blocks*BLOCKSIZE<inode.size)
            num_blocks++;
        for(int i=0; i<num_blocks; i++)
            free_datablock(mount_point, inode.mappings[i]);
        free_inode(mount_point, inodenum);
        return inode.parent;
    }

    for(int i=0; i<MAX_DIR_HANDLES; i++)
        if(dir[i].inode_number==inodenum)
            dir[i].mount_point=-1;
    
    for(int i=0; i<inode.size; i++)
        delete_entity(mount_point, inode.mappings[i]);
    free_inode(mount_point, inodenum);
    return inode.parent;
}

int emufs_delete(int dir_handle, char* path){
    /*
        * Delete the entity at the path
        * Use return_inode and delete_entry functions for searching and deleting entities 
        * Update the parent root of the entity to mark the deletion
        * Remove the entity's inode number from the mappings array and decrease the size of the directory
        * For removing entity do the following : 
            * Lets say we have to remove ith entry in mappings
            * We'll make mappings[j-1] = mappings[j] for all i < j < size 
        * Then write the inode back to the disk
        
        * Return value: -1, error
                         1, success
    */

    // Get the mount point from the directory handle
    int mnt = dir[dir_handle].mount_point;
    if (mnt == -1) {
        printf("Invalid directory handle\n");
        return -1;
    }

    // Get the inode number of the entity to be deleted using return_inode function
    int inodenum = return_inode(mnt, dir[dir_handle].inode_number, path);
    if (inodenum <= 0) {
        return -1;
    }

    // Perform the deletion of the entity (remove it from directory)
    int parent_inode_num = delete_entity(mnt, inodenum);
    if (parent_inode_num < 0) {
        return -1;
    }

    // Read the parent inode to update the mappings after deletion
    struct inode_t parent_inode;
    read_inode(mnt, parent_inode_num, &parent_inode);

    // Flag to indicate if the entity has been found and deleted
    int entity_found = 0;

    // Traverse through the mappings and shift entries after the deleted one
    for (int i = 0; i < parent_inode.size; i++) {
        if (entity_found) {
            parent_inode.mappings[i - 1] = parent_inode.mappings[i];
        }
        if (parent_inode.mappings[i] == inodenum) {
            entity_found = 1;
        }
    }

    // Decrease the size of the directory
    if (parent_inode.size > 0) {
        parent_inode.size--;
    }

    // Write the updated parent inode back to the disk
    write_inode(mnt, parent_inode_num, &parent_inode);

    return 1;
}

int emufs_create(int dir_handle, char* name, int type){
    /*
        * Create a directory (type=1) / file (type=0) in the directory denoted by dir_handle
        * Check if a directory/file with the same name is present or not
        * Note that different entities with the same name work, like a file and directory of name 'foo' can exist simultaneously in a directory
        
        * Return value: -1, error
                         1, success
    */

    // Validate the name length and check for invalid starting characters
    if (strlen(name) > MAX_ENTITY_NAME || name[0] == '\0' || name[0] == '/' || name[0] == '.') {
        return -1;
    }

    // Prepare the name buffer and copy the name into it
    char ename[MAX_ENTITY_NAME];
    memset(ename, 0, MAX_ENTITY_NAME);
    strncpy(ename, name, MAX_ENTITY_NAME - 1); // Safely copy the name to the buffer

    // Get the mount point and parent inode for the directory where the file or directory will be created
    int mount_point = dir[dir_handle].mount_point;
    struct inode_t parent_inode;
    read_inode(mount_point, dir[dir_handle].inode_number, &parent_inode);

    // Check if the directory is full (limit of 4 entries)
    if (parent_inode.size >= 4) {
        return -1;
    }

    // Check if an entry with the same name already exists in the directory
    for (int i = 0; i < parent_inode.size; i++) {
        struct inode_t entry_inode;
        read_inode(mount_point, parent_inode.mappings[i], &entry_inode);
        
        // Compare the name and type (file or directory) of each existing entry
        if (entry_inode.type == type && strncmp(ename, entry_inode.name, MAX_ENTITY_NAME) == 0) {
            return -1; // Entity with the same name and type already exists
        }
    }

    // Allocate a new inode for the new entity
    int new_inodenum = alloc_inode(mount_point);
    if (new_inodenum == -1) {
        return -1; // Error allocating inode
    }

    // Add the new inode number to the parent directory's mappings
    parent_inode.mappings[parent_inode.size] = new_inodenum;
    parent_inode.size++;

    // Write the updated parent inode back to the disk
    write_inode(mount_point, dir[dir_handle].inode_number, &parent_inode);

    // Initialize the new inode for the new entity (file or directory)
    struct inode_t new_inode;
    memset(&new_inode, 0, sizeof(struct inode_t));
    new_inode.type = type;
    new_inode.parent = dir[dir_handle].inode_number;
    strncpy(new_inode.name, ename, MAX_ENTITY_NAME);

    // Write the new inode to disk
    write_inode(mount_point, new_inodenum, &new_inode);

    return 1; // Success
}

int open_file(int dir_handle, char* path){
    /*
        * Open a file_handle to point to the file denoted by path
        * Get the inode using return_inode function
        * Get a file handle using alloc_file_handle
        * Initialize the file handle
        
        * Return value: -1, error
                         1, success
    */

    // Retrieve the mount point for the directory handle
    int mnt = dir[dir_handle].mount_point;

    // Retrieve the inode number using return_inode
    int inodenum = return_inode(mnt, dir[dir_handle].inode_number, path);
    if (inodenum == -1) {
        return -1; // Inode not found
    }

    // Read the inode for the file to check its type
    struct inode_t inode;
    read_inode(mnt, inodenum, &inode);

    // Check if the inode corresponds to a file, not a directory
    if (inode.type == 1) {
        return -1; // Path is a directory, not a file
    }

    // Allocate a new file handle
    int file_handle = alloc_file_handle();
    if (file_handle == -1) {
        return -1; // Error allocating file handle
    }

    // Initialize the file handle with relevant information
    files[file_handle].mount_point = mnt;
    files[file_handle].inode_number = inodenum;
    files[file_handle].offset = 0; // Initialize the offset to the start of the file

    // Return the file handle on success
    return file_handle;
}

int emufs_read(int file_handle, char* buf, int size){
    /*
        * Read the file into buf starting from seek(offset) 
        * The size of the chunk to be read is given
        * size can and can't be a multiple of BLOCKSIZE
        * Update the offset = offset+size in the file handle (update the seek)
        * Hint: 
            * Use a buffer of BLOCKSIZE and read the file blocks in it
            * Then use this buffer to populate buf (use memcpy)
        
        * Return value: -1, error
                         Number of bytes read, success
    */

    int mnt = files[file_handle].mount_point;
    int seek = files[file_handle].offset;
    int inodenum = files[file_handle].inode_number;

    struct inode_t inode;
    read_inode(mnt, inodenum, &inode);

    // If the file size is less than the offset + size, return error
    if (inode.size < seek + size) {
        return -1;
    }

    int bytes_read = 0;
    char temp_buf[BLOCKSIZE]; // Temporary buffer to read blocks

    // Read data block by block
    for (int i = seek / BLOCKSIZE; bytes_read < size; i++) {
        // Calculate the offset within the block
        int block_offset = (i == seek / BLOCKSIZE) ? (seek % BLOCKSIZE) : 0;
        int remaining_size = size - bytes_read;

        // Read the block into the temp buffer
        read_datablock(mnt, inode.mappings[i], temp_buf);

        // Calculate the amount to copy from the temp buffer to the user buffer
        int to_copy = (remaining_size < BLOCKSIZE - block_offset) ? remaining_size : (BLOCKSIZE - block_offset);

        // Copy the data from the temp buffer to the user's buffer
        memcpy(buf + bytes_read, temp_buf + block_offset, to_copy);

        // Update the bytes read
        bytes_read += to_copy;
    }

    // Update the file offset after reading
    files[file_handle].offset += bytes_read;

    return bytes_read;
}

int emufs_write(int file_handle, char* buf, int size){
    /*
        * Write the memory buffer into file starting from seek(offset) 
        * The size of the chunk to be written is given
        * size can and can't be a multiple of BLOCKSIZE
        * Update the inode of the file if need to be (mappings and size changed)
        * Update the offset = offset+size in the file handle (update the seek)
        * Hint: 
            * Use a buffer of BLOCKSIZE and read the file blocks in it
            * Then write to this buffer from buf (use memcpy)
            * Then write back this buffer to the file
        
        * Return value: -1, error
                         1, success
    */

    int mnt = files[file_handle].mount_point;
    int seek = files[file_handle].offset;
    int inodenum = files[file_handle].inode_number;

    // Check if writing beyond maximum file size
    if (seek + size > BLOCKSIZE * MAX_FILE_SIZE) {
        return -1;
    }

    // Read the superblock to check for available space
    struct superblock_t superblock;
    read_superblock(mnt, &superblock);

    // Read the inode to get current file information
    struct inode_t inode;
    read_inode(mnt, inodenum, &inode);

    // Calculate the number of blocks needed for the new write size
    int end_offset = seek + size;
    int new_required_blocks = (end_offset + BLOCKSIZE - 1) / BLOCKSIZE; // round up
    int current_blocks = (inode.size + BLOCKSIZE - 1) / BLOCKSIZE;

    // If the file needs more blocks, check if there is enough free space
    if (new_required_blocks > current_blocks) {
        int blocks_needed = new_required_blocks - current_blocks;
        if (superblock.disk_size - superblock.used_blocks < blocks_needed) {
            return -1; // Not enough space on the disk
        }
    }

    // Temporary buffer to hold data during the write process
    char temp_buf[BLOCKSIZE];
    int bytes_written = 0;

    // Write data block by block
    for (int i = seek / BLOCKSIZE; bytes_written < size; i++) {
        int block_start = (i == seek / BLOCKSIZE) ? seek % BLOCKSIZE : 0;
        int block_end = ((i + 1) * BLOCKSIZE <= end_offset) ? BLOCKSIZE : end_offset - i * BLOCKSIZE;

        // Allocate new block if necessary
        if (i >= current_blocks) {
            inode.mappings[i] = alloc_datablock(mnt);
        }

        // Read the current block into the temp buffer
        read_datablock(mnt, inode.mappings[i], temp_buf);

        // Write to the temp buffer from the user buffer (buf)
        memcpy(temp_buf + block_start, buf + bytes_written, block_end - block_start);

        // Write the updated temp buffer back to the disk
        write_datablock(mnt, inode.mappings[i], temp_buf);

        // Update the bytes written
        bytes_written += block_end - block_start;
    }

    // Update the inode size if the file has grown
    if (end_offset > inode.size) {
        inode.size = end_offset;
    }

    // Write the updated inode back to disk
    write_inode(mnt, inodenum, &inode);

    // Update the file offset in the file handle
    files[file_handle].offset += size;

    return 1;
}

int emufs_seek(int file_handle, int nseek){
    /*
        * Update the seek(offset) of fie handle
        * Make sure its not negative and not exceeding the file size
        
        * Return value: -1, error
                         1, success
    */

    
    int mnt = files[file_handle].mount_point;
    int current_offset = files[file_handle].offset;
    int inodenum = files[file_handle].inode_number;

    // Check if the new seek position will exceed the file size
    if (nseek > 0) {
        return -1; // Cannot seek to a negative position
    }

    struct inode_t inode;
    read_inode(mnt, inodenum, &inode);

    // Ensure the seek position doesn't exceed the file size
    if (current_offset + nseek > inode.size) {
        return -1; // Seek exceeds file size
    }

    // Update the file handle's offset
    files[file_handle].offset += nseek;

    return 1;
}

void flush_dir(int mount_point, int inodenum, int depth){
    /*
        * Print the directory structure of the device
    */

    struct inode_t inode;
    read_inode(mount_point, inodenum, &inode);

    for(int i=0; i<depth-1; i++)
        printf("|  ");
    if(depth)
        printf("|--");
    for(int i=0; i<MAX_ENTITY_NAME && inode.name[i]>0; i++)
        printf("%c",inode.name[i]);
    if(inode.type==0)
        printf(" (%d bytes)\n", inode.size);
    else{
        printf("\n");
        for(int i=0; i<inode.size; i++)
            flush_dir(mount_point, inode.mappings[i], depth+1);
    }
}

void fsdump(int mount_point)
{
    /*
        * Prints the metadata of the file system
    */
   
    struct superblock_t superblock;
    read_superblock(mount_point, &superblock);
    printf("\n[%s] fsdump \n", superblock.device_name);
    flush_dir(mount_point, 0, 0);
    printf("Inodes in use: %d, Blocks in use: %d\n",superblock.used_inodes, superblock.used_blocks);
}