#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <argp.h>

const char *argp_program_version = "PmodBT2 Uart Communication 0.1";

const char *argp_program_bug_address = "claudiu.ghenea15@yahoo.ro";

static char doc[] = "Your program description.";
static char args_doc[] = "";

static struct argp_option options[] = { 
    { "uart", 'u', 0, 0, "Simple comunication with the bluetooth device."},
    { "connect", 'c', "[Bluetooth Address]", 0, "Connect to given bluetooth address. The address must be provided in the following format [11:22:33:44:55:66]\n"},
    { "disconnect", 'd', 0, 0, "Disconnect the device."},
    { 0 } 
};


/**
 * @see Page 33
 * C,<address>
 * This command causes the device to connect to a remote address, 
 * where <address> is specified in hex format. The address is also stored as the remote address.
 * Example:
 * "C,00A053112233"
 * Connect to the Bluetooth address // 00A053112233
 */

struct arguments {
    enum { 
        CONNECT,
        DISCONNECT,
        COMMUNICATE,
        UNSET
    } mode;
    char* ble_address;
    char formatted_mac[13];
};

int isValidMacAddress(char* mac, char* formatted_mac) {

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

static error_t parse_opt(int key, char *arg, struct argp_state *state) {

    struct arguments *arguments = state->input;
    switch (key) {
    case 'u': arguments->mode = COMMUNICATE; break;
    case 'd': arguments->mode = DISCONNECT; break;
    case 'c': arguments->ble_address = arg; arguments->mode = CONNECT; break;
    case ARGP_KEY_ARG: return 0;
    default: return ARGP_ERR_UNKNOWN;
    }   
    return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc, 0, 0, 0 };

int main(int argc, char *argv[])
{
    struct arguments arguments;

    arguments.mode = UNSET;
    arguments.ble_address = NULL;

    argp_parse(&argp, argc, argv, 0, 0, &arguments);

    


    if(arguments.ble_address) {
        printf("The given mac addres is %s\n", isValidMacAddress(arguments.ble_address, arguments.formatted_mac) == 0 ? "Invalid or it does not respect the requested format [11:22:33:44:55:66]" : "Valid" );
        printf("Given bluetooth address is %s\n", ++arguments.ble_address);
        printf("Formatted bluetooth address is %s\n", arguments.formatted_mac);

    }

    return 0;
}