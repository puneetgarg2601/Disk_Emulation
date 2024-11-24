#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include "emufs.h"

void print_menu() {
    printf("\nSelect an action:\n");
    printf("1. Create new Device\n");
    printf("2. Mount device\n");
    printf("3. Unmount device\n");
    printf("4. View file system metadata\n");
    printf("5. Change directory\n");
    printf("6. Create directory\n");
    printf("7. Create file\n");
    printf("8. Delete\n");
    printf("9. Read file\n");
    printf("10. Write file\n");
    printf("11. Exit\n");
}

int main() {
    int mount_point = -1;
    char device_name[32];
    char dir_name[32];
    char file_name[32];
    char buf[256*2];
    int choice;
    int dir_handle = -1, dir_handle1=-1;

    while (1) {
        dir_handle = dir_handle1;
        // printf("before menu Directory handle: %d\n", dir_handle);
        print_menu();
        printf("Enter your choice: ");
        scanf("%d", &choice);

        switch (choice) {
            case 1:  // Create new Device
                printf("Enter device name: ");
                scanf("%s", device_name);

                // Create new device
                mount_point = opendevice(device_name, 60);
                if (mount_point == -1) {
                    printf("Error creating device.\n");
                } else {
                    printf("Device created successfully.\n");

                    // Create filesystem after device creation
                    int fs_number = 0;  // Assuming '1' is the filesystem number for the device
                    int result = create_file_system(mount_point, fs_number);
                    if (result == 0) {
                        printf("File system created successfully.\n");

                        // Change to the root directory after filesystem creation
                        dir_handle = open_root(mount_point);
                        dir_handle1 = dir_handle;
                        if (dir_handle == -1) {
                            printf("Error changing to root directory.\n");
                        } else {
                            printf("Changed to root directory successfully.\n");
                        }
                        // printf("Directory handle: %d\n", dir_handle);
                    } else {
                        printf("Error initializing file system.\n");
                    }
                }
                break;

            case 2:  // Mount device
                printf("Enter device name: ");
                scanf("%s", device_name);
                mount_point = opendevice(device_name, 60);
                if (mount_point == -1) {
                    printf("Error mounting device.\n");
                } else {

                    printf("Device mounted successfully.\n");

                    // Change to the root directory after mounting the device
                    int dir_handle = open_root(mount_point);
                    dir_handle1 = dir_handle;
                    if (dir_handle == -1) {
                        printf("Error changing to root directory.\n");
                    } else {
                        // printf("Directory handle: %d\n", dir_handle);
                        printf("Changed to root directory successfully.\n");
                    }
                }
                break;

            case 3:  // Unmount device
                if (mount_point != -1) {
                    closedevice(mount_point);
                    mount_point = -1;
                    printf("Device unmounted successfully.\n");
                } else {
                    printf("No device is mounted.\n");
                }
                break;

            case 4:  // View file system metadata
                if (mount_point != -1) {
                    // printf("Directory handle: %d\n", dir_handle);
                    fsdump(mount_point);
                    // printf("Directory handle: %d\n", dir_handle);

                } else {
                    printf("No device is mounted.\n");
                }
                break;

            case 5:  // Change directory
                if (mount_point != -1) {
                    printf("Enter directory path: ");
                    scanf("%s", dir_name);
                    dir_name[strlen(dir_name)] = 0;
                    // printf("Directory handle: %d\n", dir_handle);
                    if (dir_handle1 = change_dir(dir_handle, dir_name) == -1) {
                        printf("Error changing directory.\n");
                    } else {
                        
                        printf("Changed directory successfully.\n");
                    }
                    // printf("Directory Handle: %d and Mount Point: %d\n", dir_handle, mount_point);
                } else {
                    printf("No device is mounted.\n");
                }
                break;

            case 6:  // Create directory
                if (mount_point != -1) {
                    printf("Enter directory name: ");
                    scanf("%s", dir_name);
                    dir_name[strlen(dir_name)] = 0;
                    if (emufs_create(dir_handle, dir_name, 1) == -1) {
                        printf("Error creating directory.\n");
                    } else {
                        printf("Directory created successfully.\n");
                    }
                } else {
                    printf("No device is mounted.\n");
                }
                break;

            case 7:  // Create file
                if (mount_point != -1 && dir_handle != -1) {
                    printf("Enter file name: ");
                    scanf("%s", file_name);
                    printf("File name: %s and length is %ld\n", file_name, strlen(file_name));
                    file_name[strlen(file_name)] = 0;
                    if (emufs_create(dir_handle, file_name, 0) == -1) {
                        printf("Error creating file.\n");
                    } else {
                        printf("File created successfully.\n");
                    }
                } else {
                    printf("No device is mounted or root directory not initialised\n");
                }
                break;

            case 8:  // Delete
                if (mount_point != -1) {
                    printf("Enter file or directory name to delete: ");
                    scanf("%s", file_name);
                    int result = emufs_delete(mount_point, file_name);
                    if (result == -1) {
                        printf("Error deleting file or directory.\n");
                    } else {
                        printf("Deleted successfully.\n");
                    }
                } else {
                    printf("No device is mounted.\n");
                }
                break;

            case 9:  // Read file
                if (mount_point != -1) {
                    printf("Enter file name to read: ");
                    scanf("%s", file_name);
                    file_name[strlen(file_name)] = 0;
                    int file_handle = open_file(dir_handle, file_name);
                    if (file_handle == -1) {
                        printf("Error opening file.\n");
                    } else {
                        printf("Enter the number of bytes to read: ");
                        int byte_read;
                        scanf("%d", &byte_read);
                        printf("Reading file %s...\n", file_name);
                        int bytes_read = emufs_read(file_handle, buf, byte_read);
                        if (bytes_read == -1) {
                            printf("Error reading file.\n");
                        } else {
                            buf[bytes_read] = '\0';
                            // buf[bytes_read] = '\0';
                            printf("Data read: %s\n", buf);
                        }
                        emufs_close(file_handle, 0);
                    }
                } else {
                    printf("No device is mounted.\n");
                }
                break;

            case 10:  // Write file
                if (mount_point != -1) {
                    printf("Enter file name to write to: ");
                    scanf("%s", file_name);
                    file_name[strlen(file_name)] = 0;
                    int file_handle = open_file(dir_handle, file_name);
                    if (file_handle == -1) {
                        printf("Error opening file.\n");
                    } else {
                        printf("Enter data to write to the file: ");
                        int c;
                        while ((c = getchar()) != '\n' && c != EOF) {
                            // Discard characters until a newline or EOF is found
                        }
                        if (fgets(buf, sizeof(buf), stdin)) {
                            // Remove the trailing newline character added by fgets
                            buf[strcspn(buf, "\n")] = '\0'; 
                        }
                        // printf("Input: %s\n", buf);
                        int len = strlen(buf);
                        buf[strlen(buf)] = 0;
                        if(len > 256*4){
                            printf("Allowed write limit in a file is %d Bytes\n", 256*4);
                        }
                        else{
                            
                            int bytes_written = 0;
                            char temp_buf[256];  // Buffer for 256-byte chunks
                            while (bytes_written < len) {
                                int chunk_size = (len - bytes_written > 255) ? 255 : (len - bytes_written);
                                strncpy(temp_buf, buf + bytes_written, chunk_size);
                                temp_buf[chunk_size] = 0;  // Null-terminate the chunk
                                int result = emufs_write(file_handle, temp_buf, chunk_size);
                                if (result == -1) {
                                    puts("Error writing to file.\n");
                                    break;
                                }

                                bytes_written += chunk_size;
                            }

                            // int result = emufs_write(file_handle, buf, len);
                            // if (result == -1) {
                            //     puts("Error writing to file.\n");
                            //     break;
                            // }
                            fflush(stdout); // Flush the standard output stream
                            
                            while ((c = getchar()) != '\n' && c != EOF);
                            emufs_close(file_handle, 0);
                        }
                        
                        // if(total_bytes > 256*4){
                        //     printf("Allowed write limit in a file is %d Bytes\n", 256*4);
                        // }
                        // else{
                        //     int bytes_written = 0;
                        //     char temp_buf[256];  // Buffer for 256-byte chunks
                        //     while (bytes_written < total_bytes) {
                        //         int chunk_size = (total_bytes - bytes_written > 255) ? 255 : (total_bytes - bytes_written);
                        //         strncpy(temp_buf, buf + bytes_written, chunk_size);
                        //         temp_buf[chunk_size] = 0;  // Null-terminate the chunk
                        //         int result = emufs_write(file_handle, temp_buf, chunk_size);
                        //         if (result == -1) {
                        //             puts("Error writing to file.\n");
                        //             break;
                        //         }

                        //         bytes_written += chunk_size;
                        //     }
                        //     if (bytes_written == total_bytes) {
                        //         puts("Data written successfully.");
                        //     }
                        // }
                        
                    }
                } else {
                    puts("No device is mounted.\n");
                }
                break;

            case 11:  // Exit
                if (mount_point != -1) {
                    closedevice(mount_point);
                }
                printf("Exiting program.\n");
                return 0;

            default:
                printf("Invalid choice! Please select a valid action.\n");
        }
    }

    return 0;
}
