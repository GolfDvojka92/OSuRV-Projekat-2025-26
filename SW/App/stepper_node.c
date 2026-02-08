#include <netinet/in.h>
#include <stdint.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>      //printf, perror
#include <stdlib.h>     //EXIT_FAILURE, EXIT_SUCCESS
#include <string.h>     //memset
#include <sys/socket.h> //socket, bind, recvfrom, sendto
#include <arpa/inet.h>  //struct sockaddr_in
#include <unistd.h>     //close

#define DEV_FN "/dev/gpio_stream"

#define IP_ADDRESS "127.0.0.1"
#define SERVER_PORT 32501
#define DEFAULT_BUFLEN 1024

#define ERROR_EXIT(...) fprintf(stderr, __VA_ARGS__); exit(EXIT_FAILURE)
#define ERROR_RETURN(R, ...) fprintf(stderr, __VA_ARGS__); return R

#define NUM_OF_ANGLES 100

int gpio_write(int fd, uint8_t pin, uint8_t value) {
	uint8_t pkg[3];
	pkg[0] = 'w';
	pkg[1] = pin;
	pkg[2] = value;

	if(write(fd, &pkg, 3) != 3){
		perror("Failed to write to GPIO");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

int main()
{
    int client_socket_fd;
    struct sockaddr_in servaddr;

    // Creating socket file descriptor
    if ((client_socket_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        ERROR_EXIT("Socket creation failed");
    }
    printf("Socket created sccessfully, socket_fd = %d\n", client_socket_fd);
    memset(&servaddr, 0, sizeof(servaddr));

    // Filling server information
    servaddr.sin_family = AF_INET; // IPv4
    servaddr.sin_addr.s_addr = inet_addr(IP_ADDRESS);
    servaddr.sin_port = htons(SERVER_PORT);
    
    // Connect the socket to the server
    if (connect(client_socket_fd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        close(client_socket_fd);
        ERROR_EXIT("Connecting failed");
    }
    printf("Socket connected sccessfully, IP address : %s, port : %hu\n", inet_ntoa(servaddr.sin_addr), ntohs(servaddr.sin_port));

    //  Opening GPIO driver
	int gpio_fd = open(DEV_FN, O_RDWR);
	if(gpio_fd < 0){
        close(client_socket_fd);
        ERROR_EXIT("Failed to open GPIO driver");
	}
    printf("GPIO driver opened successfully\n");

    uint32_t distance;
    char message[] = "REQ";
    uint8_t step = 0;
    uint16_t angle = 0;
    uint32_t measurements[NUM_OF_ANGLES];

    while (1) {
        //  Sending REQ to sensor to signal that the motor is in place for the next measurement
        ssize_t sent_size;
        if ((sent_size = sendto(client_socket_fd, message, strlen(message), 0, (const struct sockaddr *)NULL, sizeof(servaddr))) != strlen(message)) {
            close(client_socket_fd);
            close(gpio_fd);
            ERROR_EXIT("Sending failed");
        }

        //  Recieving the measurement
        ssize_t received_size;
        if ((received_size = recvfrom(client_socket_fd, &distance, sizeof(distance), 0, (struct sockaddr *)NULL, NULL)) < 0) {
            close(client_socket_fd);
            close(gpio_fd);
            ERROR_EXIT("Recieving of data failed");
        }
        printf("Received message : %u mm\n", distance);

        //  Saving the measurement into an array that will be forwarded over to plot.py, indexed by the angle (subject to A/B testing)
        measurements[angle] = distance;

        //  Moving the stepper motor for 20 steps (subject to A/B testing)
        for (int i = 0; i < 20; i++) {
            if (step > 3)   step = 0;
            gpio_write(gpio_fd, 17, step == 0);
            gpio_write(gpio_fd, 18, step == 1);
            gpio_write(gpio_fd, 22, step == 2);
            gpio_write(gpio_fd, 23, step == 3);
            step++;
        }

        // Incrementing the angle index, number of angles is subject to A/B testing
        angle++;
        if (angle > NUM_OF_ANGLES)  angle = 0;
    }

    //  Closing file descriptors
    close(client_socket_fd);
	close(gpio_fd);
    return EXIT_SUCCESS;
}
