// Read from serial port in non-canonical mode
//
// Modified by: Eduardo Nuno Almeida [enalmeida@fe.up.pt]
// Receiver: Implements Logic Connection Establishment by receiving SET and sending UA

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>

// Baudrate settings are defined in <asm/termbits.h>, which is 
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

#define FRAME_SIZE 5
#define MAX_RETRIES 3

volatile int STOP = FALSE;

typedef enum {

    WAIT_FOR_FLAG,
    WAIT_FOR_A,
    WAIT_FOR_C,
    WAIT_FOR_BCC,
    WAIT_FOR_FLAG_END

} State;

        // Define UA frame
unsigned char UA_FRAME[FRAME_SIZE];
UA_FRAME[0] = 0x7E;        // FLAG
UA_FRAME[1] = 0x01;        // A (Receiver)
UA_FRAME[2] = 0x07;        // C (UA)
UA_FRAME[3] = UA_FRAME[1] ^ UA_FRAME[2]; // BCC1
UA_FRAME[4] = 0x7E;        // FLAG


int main(int argc, char *argv[])
{
    // Program usage: Specify the serial port
    if (argc < 2)
    {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS2\n",
               argv[0],
               argv[0]);
        exit(1);
    }

    const char *serialPortName = argv[1];

    // Open serial port device for reading and writing, and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    int fd = open(serialPortName, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror(serialPortName);
        exit(EXIT_FAILURE);
    }

    struct termios oldtio;
    struct termios newtio;

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    // Configure port settings
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0; // Inter-character timer unused
    newtio.c_cc[VMIN] = 1;  // Blocking read until FRAME_SIZE chars received

    // Flush the serial port
    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        close(fd);
        exit(EXIT_FAILURE);
    }

    printf("Receiver: New termios structure set. Waiting for SET frame...\n");

    // Read SET frame
    State state = WAIT_FOR_FLAG;
    unsigned char received_frame[FRAME_SIZE];
    int frame_index = 0;
    
    while (!STOP)
    {
        unsigned char byte;
        int bytes_read = read(fd, &byte, 1);

        if (bytes_read < 0)
        {
            perror("read SET_FRAME");
            // Restore the old port settings before exiting
            tcsetattr(fd, TCSANOW, &oldtio);
            close(fd);
            exit(EXIT_FAILURE);
        }
        else if (bytes_read == 0)
        {
            continue; // no data 
        }

        switch (state)
        {
            case WAIT_FOR_FLAG:
                if (byte == 0x7E)
                {
                    received_frame[0] = byte;
                    state = WAIT_FOR_A;
                }
                break;
            case WAIT_FOR_A:
                if (byte == 0x03)
                {
                    received_frame[1] = byte;
                    state = WAIT_FOR_C;
                }
                else if (byte != 0x7E)
                {
                    state = WAIT_FOR_FLAG;
                }
                break;
            case WAIT_FOR_C:
                if (byte == 0x03)
                {
                    received_frame[2] = byte;
                    state = WAIT_FOR_BCC;
                }
                else 
                {
                    state = WAIT_FOR_FLAG;
                }
                break;
            case WAIT_FOR_BCC:
                if (byte == (received_frame[1] ^ received_frame[2]))
                {
                    received_frame[3] = byte;
                    state = WAIT_FOR_FLAG_END;
                }
                else
                {
                    state = WAIT_FOR_FLAG;
                }
                break;
            case WAIT_FOR_FLAG_END:
                if (byte == 0x7E)
                {
                    received_frame[4] = byte;
                    printf("Receiver: SET frame received (%d bytes):\n", bytes_read);
                    for (int i = 0; i < bytes_read; i++)
                    {
                        printf("0x%02X ", received_frame[i]);
                    }
                    printf("\n");

                    int bytes_written = write(fd, UA_FRAME, sizeof(UA_FRAME));
                    if (bytes_written < 0)
                    {
                        perror("write UA_FRAME");
                        // Restore the old port settings before exiting
                        tcsetattr(fd, TCSANOW, &oldtio);
                        close(fd);
                        exit(EXIT_FAILURE);
                    }
                    printf("Receiver: UA frame sent (%d bytes):\n", bytes_written);
                    for (int i = 0; i < sizeof(UA_FRAME); i++)
                    {
                        printf("0x%02X ", UA_FRAME[i]);
                    }
                    printf("\n");

                    STOP = TRUE;
                }
                else
                {
                    state = WAIT_FOR_FLAG;
                }
                break;

            default:
                state = WAIT_FOR_FLAG;
                break;
        }
    
    }

    if(tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        close(fd);
        exit(EXIT_FAILURE);
    }

    close(fd);
    return 0;
}
