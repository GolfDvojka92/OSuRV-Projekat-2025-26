#include <netinet/in.h>
#include <stdint.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>      // printf, perror
#include <stdlib.h>     // EXIT_FAILURE, EXIT_SUCCESS
#include <string.h>     // memset
#include <sys/socket.h> // socket, bind, recvfrom, sendto
#include <arpa/inet.h>  // struct sockaddr_in
#include <unistd.h>     // close, usleep

#define DEV_FN "/dev/gpio_stream"

#define IP_ADDRESS "127.0.0.1"
#define SERVER_PORT 32501
#define DEFAULT_BUFLEN 1024

#define ERROR_EXIT(...) fprintf(stderr, __VA_ARGS__); exit(EXIT_FAILURE)

#define NUM_OF_ANGLES 100

int gpio_write(int fd, uint8_t pin, uint8_t value) {
    uint8_t pkg[3];
    pkg[0] = 'w';
    pkg[1] = pin;
    pkg[2] = value;
    printf("[DEBUG] GPIO write: pin=%d value=%d\n", pin, value);
    if(write(fd, &pkg, 3) != 3){
        perror("Failed to write to GPIO");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

// Half-step sequence for ULN2003 + 28BYJ-48
const uint8_t step_seq[8][4] = {
    {1,0,0,0},
    {1,1,0,0},
    {0,1,0,0},
    {0,1,1,0},
    {0,0,1,0},
    {0,0,1,1},
    {0,0,0,1},
    {1,0,0,1}
};

int main()
{
    int client_socket_fd;
    struct sockaddr_in servaddr;

    // Creating socket file descriptor
    if ((client_socket_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        ERROR_EXIT("Socket creation failed\n");
    }
    printf("Socket created successfully, socket_fd = %d\n", client_socket_fd);

    memset(&servaddr, 0, sizeof(servaddr));

    // Filling server information
    servaddr.sin_family = AF_INET; // IPv4
    servaddr.sin_addr.s_addr = inet_addr(IP_ADDRESS);
    servaddr.sin_port = htons(SERVER_PORT);
    
    // Connect the socket to the server
    if (connect(client_socket_fd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        close(client_socket_fd);
        ERROR_EXIT("Connecting failed\n");
    }
    printf("Socket connected successfully, IP: %s, port: %hu\n", inet_ntoa(servaddr.sin_addr), ntohs(servaddr.sin_port));

    // Opening GPIO driver
    int gpio_fd = open(DEV_FN, O_RDWR);
    if(gpio_fd < 0){
        close(client_socket_fd);
        ERROR_EXIT("Failed to open GPIO driver\n");
    }
    printf("GPIO driver opened successfully\n");

    uint32_t distance;
    char message[] = "REQ";
    uint16_t angle = 0;
    uint32_t measurements[NUM_OF_ANGLES];

    while (1) {
        // Send REQ to sensor
        if (sendto(client_socket_fd, message, strlen(message), 0, NULL, sizeof(servaddr)) != strlen(message)) {
            close(client_socket_fd);
            close(gpio_fd);
            ERROR_EXIT("Sending failed\n");
        }

        // Receive distance
        if (recvfrom(client_socket_fd, &distance, sizeof(distance), 0, NULL, NULL) < 0) {
            close(client_socket_fd);
            close(gpio_fd);
            ERROR_EXIT("Receiving failed\n");
        }
        printf("Received distance: %u mm\n", distance);

        // Save measurement
        measurements[angle] = distance;

        // Rotate stepper 20 steps using half-step sequence
        for(int i=0; i<20; i++){
            for(int s=0; s<8; s++){
                gpio_write(gpio_fd, 17, step_seq[s][0]);
                gpio_write(gpio_fd, 18, step_seq[s][1]);
                gpio_write(gpio_fd, 22, step_seq[s][2]);
                gpio_write(gpio_fd, 23, step_seq[s][3]);
                usleep(70000); // 2 ms delay, možeš povećati ako motor šteka
            }
        }

        // Increment angle index
        angle++;
        if (angle >= NUM_OF_ANGLES) angle = 0;
    }

    close(client_socket_fd);
    close(gpio_fd);
    return EXIT_SUCCESS;
}


