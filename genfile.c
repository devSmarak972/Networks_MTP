#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Define the size of each block (adjust as needed)
#define BLOCK_SIZE 1024 // 1 KB

// Define the total size of the file in bytes (adjust as needed)
#define TOTAL_SIZE 120 * 1024 // 100 KB

// Function to generate a random character
// char random_char() {
//     return (char)('A' + rand() % 26); // Generate a random character between 'A' and 'Z'
// }
char random_char() {
    const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    const size_t charset_size = sizeof(charset) - 1; // Exclude '\0'
    return charset[rand() % charset_size];
}
int main() {
    FILE *file;

    // Seed the random number generator
    srand(time(NULL));

    // Open file for writing in binary mode
    file = fopen("large_file.txt", "wb");
    if (file == NULL) {
        perror("Error opening file");
        return 1;
    }

    // Write blocks of random data to the file until it reaches the desired size
    size_t bytes_written = 0;
    while (bytes_written < TOTAL_SIZE) {
        // Calculate the number of bytes to write in this iteration
        size_t bytes_to_write = (TOTAL_SIZE - bytes_written) < BLOCK_SIZE ? (TOTAL_SIZE - bytes_written) : BLOCK_SIZE;

        // Generate random data
        char buffer[BLOCK_SIZE];
        for (size_t i = 0; i < bytes_to_write; i++) {
            buffer[i] = random_char();
        }

        // Write the block of data to the file
        size_t result = fwrite(buffer, 1, bytes_to_write, file);
        if (result != bytes_to_write) {
            perror("Error writing to file");
            fclose(file);
            return 1;
        }

        bytes_written += bytes_to_write;
    }
    fputs("\r\n", file);

    // Close the file
    fclose(file);

    printf("Big file of over 100 KB filled with random strings generated successfully.\n");

    return 0;
}
