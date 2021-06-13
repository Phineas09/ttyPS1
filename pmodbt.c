#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <argp.h>
#include <signal.h>
#include <pthread.h>

/**
 * Defines for developing
 */
#define DEBUG 
#define RELEASE 


/** Variable for detecting CTRL-C */

static volatile int keep_running = 1;


/**
 * Declarations specific argp (program arguments and specs)
 */
int COMMAND_MODE = 0;


static error_t parse_opt(int key, char *arg, struct argp_state *state);

const char *argp_program_version = "PmodBT2 Uart Communication 0.1";
const char *argp_program_bug_address = "claudiu.ghenea15@yahoo.ro";

static char doc[] = "Your program description.";
static char args_doc[] = "";

static struct argp_option options[] = { 
    { "uart", 'u', 0, 0, "Simple comunication with the connected bluetooth device or with the PMOD."},
    { "connect", 'c', "[Bluetooth Address]", 0, "Connect to given bluetooth address. The address must be provided in the following format [11:22:33:44:55:66]\n"},
    { "disconnect", 'd', 0, 0, "Disconnect the device."},
    { "exitCMDmode", 'e', 0, 0, "Exit CMD mode."},
    { 0 } 
};

struct arguments {
    enum { 
        CONNECT,
        DISCONNECT,
        COMMUNICATE,
        EXITCMDMODE,
        UNSET
    } mode;
    char* ble_address;
    char formatted_mac[13];
};

static struct argp argp = { options, parse_opt, args_doc, doc, 0, 0, 0 };


/**
 * Function interrupt_handler
 * ----------------------------
 *  Handler for SIGINT 
 * 
 *      @param[in] dummy interrupt source
 */

void interrupt_handler(int dummy) {
    keep_running = 0;
    printf("Keyboard Interrupt!\n");
    //raise (SIGALRM);
    ualarm(100, 0);
}

/**
 * Function is_valid_mac_address
 * ----------------------------
 *  Returns if a given mac addres is valid and converts it to PmodBT2 format.
 * 
 *      @see page 33
 *      @param[in] mac target mac addres to verify in [11:22:33:44:55:66] string format
 *      @param[out] formatted_mac mac formatted for sending to the Pmod 112233445566 string format \
 *          (must be allocated before calling)
 */

int is_valid_mac_address(char* mac, char* formatted_mac) {

    mac++; //Get rid of the first "["
    mac[strlen(mac) - 1] = 0; //get rig of the last "]"

    int i = 0;
    int s = 0;

    while (*mac) {
       if (isxdigit(*mac)) {
          formatted_mac[i] = *mac;
          i++;
       }
       else if (*mac == ':' || *mac == '-') {
          if (i == 0 || i / 2 - 1 != s)
            break;
          ++s;
       }
       else
           s = -1;
       ++mac;
    }
    formatted_mac[12] = 0;
    return (i == 12 && s == 5); // 12 hex digits, 5 separators  as requested in the documentation
}

/**
 * Function parse_opt
 * ----------------------------
 *  Function for parseing the input arguments used by the argp_parse and argp structure
 */

static error_t parse_opt(int key, char *arg, struct argp_state *state) {

    struct arguments *arguments = state->input;
    switch (key) {
    case 'u': arguments->mode = COMMUNICATE; break;
    case 'e': arguments->mode = EXITCMDMODE; break;
    case 'd': arguments->mode = DISCONNECT; break;
    case 'c': arguments->ble_address = arg; arguments->mode = CONNECT; break;
    case ARGP_KEY_ARG: return 0;
    default: return ARGP_ERR_UNKNOWN;
    }   
    return 0;
}

/**
 * Function initialize_serial
 * ----------------------------
 *  Initialize the device descriptor.
 * 
 *      @param[in] fd File descriptor of serial device
 *      @param[in] speed Baud rate for the selected serial device : 115200 used in this case \
 *          pecified by the PmodBT2 documentation
 *      @param[in] parity Parity bits, no parity in our case
 * 
 *      @return Returns 0 if success or -1 otherwise
 */

int initialize_serial(int fd, int speed, int parity) {

    struct termios tty;

    if (tcgetattr(fd, &tty) != 0) {
        printf("Error %d from \"initialize_serial::tcgetattr\"\n", errno);
        return -1;
    }

    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8; // 8-bit chars disable IGNBRK for mismatched speed tests; otherwise receive break
    tty.c_iflag &= ~IGNBRK; // Disable break processing
    tty.c_lflag = 0;        // No signaling chars & no echo
                            // No canonical processing
    tty.c_oflag = 0;        // No remapping, no delays
    tty.c_cc[VMIN] = 0;     // Read doesn't block
    tty.c_cc[VTIME] = 10;   // 1 second read timeout

    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl
    tty.c_cflag |= (CLOCAL | CREAD);   // Turn on READ & ignore ctrl lines (CLOCAL = 1)
    tty.c_cflag &= ~(PARENB | PARODD); // No parity
    tty.c_cflag |= parity;
    tty.c_cflag &= ~CSTOPB; // Only one stop bit
    tty.c_cflag &= ~CRTSCTS; // Disable RTS/CTS hardware
    tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
    tty.c_oflag &= ~ONLCR; // Prevent conversion of newline to \r\n

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        printf("Error %d from \"initialize_serial::tcsetattr\"\n", errno);
        return -1;
    }
    return 0;
}

/*
 * Function: set_blocking
 * ----------------------------
 *   Sets blocking communication, 0 for non-blocking and 1 for blocking
 *
 */

void set_blocking(int fd) {

    struct termios tty;
    memset(&tty, 0, sizeof tty);
    if (tcgetattr(fd, &tty) != 0) {
        printf("Error %d from \"set_blocking::tggetattr\"\n", errno);
        return;
    }

    /**
     * @see https://man7.org/linux/man-pages/man3/termios.3.html
     */

    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 10; // 1 second read timeout

    if (tcsetattr(fd, TCSANOW, &tty) != 0)
        printf("Error %d while setting device attributes! (\"set_blocking::tcsetattr\")\n", errno);
}


/**
 * Function: send_message_to_device
 * ----------------------------
 *  Send message to device
 *      @param[in] device_descriptor File descriptor of serial device
 *      @param[in] buffer Buffed containing the message to send
 *      @param[in] buffer_size Size of the buffer to send
 * 
 *      @return Returns number of bytes sent to device
 */

int send_message_to_device(int device_descriptor, char* buffer, ssize_t buffer_size) {

    int sent_bytes = write(device_descriptor, buffer, buffer_size);

    #ifdef DEBUG
        printf("DEBUG: Sent %d bytes to device: %s\n", sent_bytes, buffer);
    #endif

    return sent_bytes;
}

/**
 * Function: get_response_from_device
 * ----------------------------
 *  Send message to device and returns the message response generated by the device
 *      @param[in] device_descriptor File descriptor of serial device
 *      @param[in] buffer Buffed containing the message to send
 *      @param[in] buffer_size Size of the buffer to send
 *      @param[in] recv_buffer Empty buffer of size 256 for the message generated by the device
 * 
 *      @return Returns number of bytes read from the device
 */

int get_response_from_device(int device_descriptor, char* buffer, size_t buffer_size, char* recv_buffer) {

    int sent_bytes = send_message_to_device(device_descriptor, buffer, buffer_size);

    if(sent_bytes == buffer_size) {
        
        usleep((buffer_size + 25) * 100); // sleep enough to transmit the 7 plus receive 25:  approx 100 uS per char transmit

        if(recv_buffer != NULL) { //Check the buffer to be allocated
            int n = read(device_descriptor, recv_buffer, 256);  // Read up to 256 characters if ready to read 
            if(n > 0) {
                recv_buffer[n] = 0;
                #ifdef DEBUG
                    printf("DEBUG: Device has responded with %d bytes: %s\n", n, recv_buffer);
                #endif
            }
            else 
                printf("Error %d while reading from device! (\"get_response_from_device::read\")\n", errno);
            return n;   
        } 
    }
    printf("Error %d while writing to the device! (\"get_response_from_device::write\")\n", errno);
    return sent_bytes;
}

/**
 * Function: enter_device_cmd_mode
 * ----------------------------
 *  Set the device to CMD mode, prepare for sending commands.
 *      @param[in] device_descriptor File descriptor of serial device
 * 
 *      @return 0 if the device entered the CMD mode succesfullt and 1 otherwise
 */

int enter_device_cmd_mode(int device_descriptor) {

    char cmd_buffer[] = "$$$";
    char* recv_buffer = malloc(sizeof(char) * 256);

    get_response_from_device(device_descriptor, cmd_buffer, sizeof(cmd_buffer) - 1, recv_buffer);
    if(!strncmp(recv_buffer, "CMD", 3)) {
        free(recv_buffer);
        COMMAND_MODE = 1;
        return 0;
    }
    free(recv_buffer);

    printf("Device coudl be stuck in the CMD mode, try -e option first!\n");

    return 1;
}

/**
 * Function: exit_device_cmd_mode
 * ----------------------------
 *  Exit device CMD mode. ( ---<cr> )
 *      @param[in] device_descriptor File descriptor of serial device
 * 
 *      @return 0 if the device entered the CMD mode succesfullt and 1 otherwise
 */

int exit_device_cmd_mode(int device_descriptor) {

    char cmd_buffer[] = {'-', '-','-', 0x0D};
    char* recv_buffer = malloc(sizeof(char) * 256);

    get_response_from_device(device_descriptor, cmd_buffer, 4, recv_buffer);
    if(!strncmp(recv_buffer, "END", 3)) {
        free(recv_buffer);
        COMMAND_MODE = 0;
        return 0;
    }
    free(recv_buffer);
    return 1;
}

/**
 * Function: do_cleanup
 * ----------------------------
 *  Resets the deivce to normal mode and | or resets it to apply the changes
 *      @param[in] device_descriptor File descriptor of serial device
 * 
 */

int do_cleanup(int device_descriptor) {

    if(COMMAND_MODE) {
        if(!exit_device_cmd_mode(device_descriptor))
            return 0;
    }
    return 1;
}


/**
 * Function: connect_to_ble_address
 * ----------------------------
 *  Tell the module to connect to a given Bluetooth device by mac address.
 *      @param[in] device_descriptor File descriptor of serial device
 *      @param[in] address Target device mac address
 * 
 *      @return 0 If AOK
 *      @return 1 If ERR
 * 
 *      @example C,00A053112233<cr>
 */

int connect_to_ble_address(int device_descriptor, char* address) {

    char* command_buffer = calloc(256, sizeof(char));
    sprintf(command_buffer, "C,%s", address);

    #ifdef DEBUG
        printf("DEBUG: Sending connect command %s of length %lu\n", command_buffer, strlen(command_buffer));
    #endif

    command_buffer[strlen(command_buffer)-1] = 0x0D;
    
    //send_message_to_device(device_descriptor, command_buffer, strlen(command_buffer));

    get_response_from_device(device_descriptor, command_buffer, strlen(command_buffer), command_buffer);

    /* Interpret the response AOK success ERR error ? fatal error */

    if(!strncmp(command_buffer, "AOK", 3)) {
        free(command_buffer);
        return 0;
    }

    #ifdef DEBUG
        if(!strncmp(command_buffer, "?", 1)) {
            printf("DEBUG: Fatal error in connect_to_ble_address::get_response_from_device()\n");
        }
    #endif

    
    free(command_buffer);
    return 1;
}

/**
 * Function: disconnect_from_ble
 * ----------------------------
 *  Tell the module to diconnect from the connected Bluetooth device.
 *      @param[in] device_descriptor File descriptor of serial device
 * 
 *      @return 0 If Success
 *      @return 1 If Error
 * 
 *      @example K,<cr> -> should echo KILL<cl><lf>
 */

int disconnect_from_ble(int device_descriptor) {

    char cmd_buffer[] = {'K', ',', 0x0D};
    char* recv_buffer = malloc(sizeof(char) * 256);

    get_response_from_device(device_descriptor, cmd_buffer, 3, recv_buffer);
    if(!strncmp(recv_buffer, "KILL", 4)) {
        free(recv_buffer);
        return 0;
    }
    free(recv_buffer);
    return 1;
}


/**
 * Function: check_device_connected
 * ----------------------------
 *  Check if the device is connected.
 *      @param[in] device_descriptor File descriptor of serial device
 * 
 *      @return 0 If is connected
 *      @return 1 If is not connected
 * 
 *      @example GK<cr> -> should echo 1,0,0 if connected
 */

int check_device_connected(int device_descriptor) {

    char cmd_buffer[] = {'G', 'K', 0x0D};
    char* recv_buffer = malloc(sizeof(char) * 256);

    get_response_from_device(device_descriptor, cmd_buffer, 3, recv_buffer);
    if(!strncmp(recv_buffer, "1,0,0", 5)) {
        free(recv_buffer);
        return 0;
    }
    free(recv_buffer);
    return 1;
}

/**
 * Function: check_device_connected
 * ----------------------------
 *  Check if the device is connected.
 *      @param[in] device_descriptor File descriptor of serial device
 *      @param[in] recv_buffer Buffer to store the address, should be allocated before call buffer[256]
 * 
 *      @return te address of the connected device
 * 
 *      @example GR<cr> -> should echo the connected device address
 */

int get_connected_address(int device_descriptor, char* recv_buffer) {

    char cmd_buffer[] = {'G', 'R', 0x0D};

    int recv_size = get_response_from_device(device_descriptor, cmd_buffer, 3, recv_buffer);
    recv_buffer[recv_size] = 0;

    return recv_size;
}

/**
 * Function: thread_pooling_module
 * ----------------------------
 *  Thread function in pooling mode, read from the device descriptor every 0.1 seconds
 *  Thread function is cancellable and ASYNC cancellable
 *      @param[in] args device descriptor
 * 
 */

void* thread_pooling_module(void* args) {


    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,NULL);

    int device_descriptor = *(int*)args;
    char* pmod_buffer = calloc(256, sizeof(char));

    while(1) {

        //Sleep 
        usleep(100000);
        int recv_bytes = read(device_descriptor, pmod_buffer, 256);  // Read up to 256 characters if ready to read 
        if(recv_bytes > 0) {
            pmod_buffer[recv_bytes] = 0;
            printf("\nPmodBT2 Responded > %s", pmod_buffer);
        }
    }

}

/**
 * Function: handler
 * ----------------------------
 *  Dummy handler function for SIGALRM to stop the scanf function
 * 
 */

void handler(int signo) {
  return;
}

int main(int argc, char* argv[]) {

    /** First step: Get the arguments */

    struct arguments arguments;

    arguments.mode = UNSET;
    arguments.ble_address = NULL;

    argp_parse(&argp, argc, argv, 0, 0, &arguments);

    signal(SIGINT, interrupt_handler);

    /** Initialize device */

    #ifdef RELEASE

        char * device_name = "/dev/ttyPS1";
        int device_descriptor = open(device_name, O_RDWR | O_NOCTTY | O_SYNC);

        if (device_descriptor < 0) {
            printf("Error %d  while opening %s: %s\n", errno, device_name, strerror(errno));
            return -1;
        }

        initialize_serial(device_descriptor, B115200, 0); // set speed to 115,200 bps, 8n1 (no parity)
        set_blocking(device_descriptor);                   

        //char* recv_buffer = malloc(sizeof(char) * 256);

    #endif


    if(arguments.mode == UNSET) {
        printf("See --help for more information!\n");
        return -1;
    }

    /** Connect to a given Ble Device address */

    if(arguments.mode == CONNECT && arguments.ble_address) {

        int valid_mac_address = is_valid_mac_address(arguments.ble_address, arguments.formatted_mac);

        arguments.ble_address++;

        #ifdef DEBUG

            printf("DEBUG: The given mac addres is %s\n", valid_mac_address\
                == 0 ? "Invalid or it does not respect the requested format [11:22:33:44:55:66]" : "Valid" );
            printf("DEBUG: Given bluetooth address is %s\n", arguments.ble_address);
            printf("DEBUG: Formatted bluetooth address is %s\n", arguments.formatted_mac);

        #endif

        #ifdef RELEASE

            printf("Entering comand mode...\n");
            if(!enter_device_cmd_mode(device_descriptor)) {

                #ifdef DEBUG
                    printf("DEBUG: Device has entereed comand mode succesfully!\n");
                #endif

                if(!connect_to_ble_address(device_descriptor, arguments.formatted_mac))
                    printf("The device has connected to %s succesfully!\n", arguments.ble_address);
                else
                    printf("Device could not connect to %s!\n", arguments.ble_address);
            }

        #endif

    }

    /** Disconnect from any ble device that is connected */
    
    if(arguments.mode == DISCONNECT) { 
        #ifdef RELEASE
            printf("Entering comand mode...\n");
            if(!enter_device_cmd_mode(device_descriptor)) {

                #ifdef DEBUG
                    printf("DEBUG: Device has entereed comand mode succesfully!\n");
                #endif

                if(!disconnect_from_ble(device_descriptor))
                    printf("The device has disconnected succesfully!\n");
                else
                    printf("Device could not disconnect!\n");
            }
        #endif
    }

    /** Exit CMD mode */

    if(arguments.mode == EXITCMDMODE) { 
        #ifdef RELEASE
            printf("Exiting comand mode...\n");
            if(!exit_device_cmd_mode(device_descriptor)) {
                printf("Device has exited CMD mode succesfully!\n");
            }
        #endif
    }


    /** Communicate with the other device, something like a pipe */

#pragma region COMMUNICATE

    if(arguments.mode == COMMUNICATE) {

        /** Verify if the device is connected ? */

        /** Start communication */

        #ifdef RELEASE
            printf("Entering comand mode...\n");

            /** Enter cmd mode and find whom we are talking to! */

            if(!enter_device_cmd_mode(device_descriptor)) {
                
                #ifdef DEBUG
                    printf("DEBUG: Device has entereed comand mode succesfully!\n");
                #endif

                if(!check_device_connected(device_descriptor)) {

                    #ifdef DEBUG
                        printf("The device is connected!\n");
                    #endif

                    char* connection_buffer = calloc(256, sizeof(char));

                    int cn_size = get_connected_address(device_descriptor, connection_buffer);

                    if(cn_size)
                        printf("You will talk with: %s\n", connection_buffer);
                    else {
                        printf("Something wen wrong!\n");
                        free(connection_buffer);
                        do_cleanup(device_descriptor);
                        return -1;
                    }
                } 
                else
                    printf("You will talk with the PmodBT2\n");

                if(!exit_device_cmd_mode(device_descriptor)) {

                    #ifdef DEBUG
                        printf("DEBUG: Device has exited CMD mode succesfully!\n");
                    #endif

                    printf("> ");

                    /** Start communication */

                    char* user_buffer = calloc(256, sizeof(char));

                    int user_buffer_size = 0;
                    int sent_bytes  = 0;

                    pthread_t thread_id;
                    pthread_create(&thread_id, NULL, thread_pooling_module, (void*)&device_descriptor);

                    struct sigaction sa;        

                    sa.sa_handler = handler;
                    sigemptyset(&sa.sa_mask);
                    sa.sa_flags = 0;
                    sigaction(SIGALRM, &sa, NULL);

                    while(keep_running) {

                        /** Read buffer from user */

                        if(scanf("%s", user_buffer) == 1) {
                            user_buffer_size = strlen(user_buffer);
                            if(user_buffer_size > 0) {

                                /** Maybe put <cr><lf> at the end ot buffer  */

                                if(strncmp(user_buffer, "$$$", 3)) {
                                    user_buffer[user_buffer_size++] = 0xD;
                                    user_buffer[user_buffer_size] = 0;
                                }

                                sent_bytes = send_message_to_device(device_descriptor, user_buffer, user_buffer_size);

                                if(sent_bytes == user_buffer_size) {
                                    usleep((user_buffer_size + 25) * 100); 
                                } else {
                                    printf("Err > Only %d bytes were sent!", sent_bytes);
                                }
                            }
                        }
                        printf("> ");
                    }            

                    pthread_cancel(thread_id);

                    #ifdef DEBUG
                        printf("DEBUG: Thread has been canceled, waiting for join!\n");
                    #endif

                    pthread_join(thread_id, NULL);


                    free(user_buffer);

                } else {
                    printf("Something wen wrong!\n");
                    do_cleanup(device_descriptor);
                    return -1;
                }


            } else {
                printf("Something wen wrong!\n");
                do_cleanup(device_descriptor);
                return -1;
            }

        #endif

    }

#pragma endregion COMMUNICATE

    /** Before exiting we must reset the device to exit from command mode */
    #ifdef RELEASE
        do_cleanup(device_descriptor);
        close(device_descriptor);
    #endif

    return 0;
}