#include <netinet/in.h>
#include <stdint.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <zmq.h>
#include <pthread.h>
#include <termios.h>

#define DEV_FN "/dev/gpio_stream"

#define IP_ADDRESS "127.0.0.1"
#define SERVER_PORT 32501

#define NUM_OF_ANGLES 64 
#define DELTA 50

#define ERROR_EXIT(...) do { fprintf(stderr, __VA_ARGS__); exit(EXIT_FAILURE); } while(0)

// GPIO write function
int gpio_write(int fd, uint8_t pin, uint8_t value) {
    uint8_t pkg[3] = {'w', pin, value};
    if(write(fd, &pkg, 3) != 3){
        perror("Failed to write to GPIO");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

// Step sequence for ULN2003 + 28BYJ-48
const uint8_t step_seq[4][4] = {
    {1,0,0,0},
    {0,1,0,0},
    {0,0,1,0},
    {0,0,0,1}
};

// Globals for thread
volatile int quit_flag = 0;
int angle;
int step_index;
int dir;
int gpio_fd;
pthread_mutex_t quit_mutex;

// Thread function to detect 'q' press
void* key_thread(void* arg){
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    char c;
    while(!quit_flag){
        c = getchar();
        if(c == 'q'){
	    pthread_mutex_lock(&quit_mutex);
            quit_flag = 1;
	    pthread_mutex_unlock(&quit_mutex);
        }
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return NULL;
}

// Rotate stepper N steps in given direction
void step_motor(int steps, int direction){
    for(int i=0; i<steps; i++){
        step_index += direction;
        if(step_index > 3) step_index = 0;
        if(step_index < 0) step_index = 3;

        gpio_write(gpio_fd, 17, step_seq[step_index][0]);
        gpio_write(gpio_fd, 18, step_seq[step_index][1]);
        gpio_write(gpio_fd, 22, step_seq[step_index][2]);
        gpio_write(gpio_fd, 23, step_seq[step_index][3]);

        usleep(2000);
    }
}

int main()
{
    int client_socket_fd;
    struct sockaddr_in servaddr;

    // UDP socket
    if ((client_socket_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        ERROR_EXIT("Socket creation failed\n");
    }
    printf("Socket created, fd = %d\n", client_socket_fd);

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(IP_ADDRESS);
    servaddr.sin_port = htons(SERVER_PORT);

    if (connect(client_socket_fd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        close(client_socket_fd);
        ERROR_EXIT("Connecting failed\n");
    }
    printf("UDP socket connected, IP: %s, port: %hu\n", inet_ntoa(servaddr.sin_addr), ntohs(servaddr.sin_port));

    // Open GPIO
    gpio_fd = open(DEV_FN, O_RDWR);
    if(gpio_fd < 0){
        close(client_socket_fd);
        ERROR_EXIT("Failed to open GPIO driver\n");
    }
    printf("GPIO opened successfully\n");

    // ZeroMQ PUB
    void *context = zmq_ctx_new();
    void *publisher = zmq_socket(context, ZMQ_PUB);
    if(zmq_bind(publisher, "tcp://*:5555") != 0)
        ERROR_EXIT("Failed to bind PUB socket\n");

    sleep(1); // wait for subscribers
    printf("[DEBUG] Ready\n");

    // Thread for 'q' press
    pthread_t tid;
    if(pthread_create(&tid, NULL, key_thread, NULL) != 0){
        ERROR_EXIT("Failed to create key thread\n");
    }

    uint32_t distance;
    char message[] = "REQ";

    angle = NUM_OF_ANGLES - 1;
    dir = -1;
    step_index = 3;

    uint32_t measurements[NUM_OF_ANGLES] = {0};
    uint32_t prev_measurements[NUM_OF_ANGLES] = {0};

    while(1) {
	pthread_mutex_lock(&quit_mutex);
	if (quit_flag) {
	    pthread_mutex_unlock(&quit_mutex);
	    break;
	}
	pthread_mutex_unlock(&quit_mutex);
        // Send request
        if (send(client_socket_fd, message, strlen(message), 0) < 0) {
            perror("Send failed");
        }
        
        if (recv(client_socket_fd, &distance, sizeof(distance), 0) < 0) {
            perror("Recv failed");
        }

        measurements[angle] = distance;

        // Rotate stepper 32 steps
        step_motor(32, dir);

        // Check if change > DELTA
        if(abs((int)measurements[angle] - (int)prev_measurements[angle]) >= DELTA){
            prev_measurements[angle] = measurements[angle];

            zmq_msg_t meas_msg;
            zmq_msg_init_size(&meas_msg, sizeof(prev_measurements));
            memcpy(zmq_msg_data(&meas_msg), prev_measurements, sizeof(prev_measurements));
            zmq_msg_send(&meas_msg, publisher, 0);
            zmq_msg_close(&meas_msg);
        }

        // Update angle
        angle += dir;
        if(angle >= NUM_OF_ANGLES){
            angle = NUM_OF_ANGLES -1;
            dir = -1;
        } else if(angle < 0){
            angle = 0;
            dir = 1;
        }
    }

    // Return stepper to initial position
    printf("Returning sensor to initial position\n");
    if (dir == 1) angle--;
    step_motor((NUM_OF_ANGLES - 1 - angle) * 32, 1);


    pthread_join(tid, NULL);
    close(client_socket_fd);
    close(gpio_fd);
    zmq_close(publisher);
    zmq_ctx_destroy(context);

    printf("Exiting.\n");
    return EXIT_SUCCESS;
}

