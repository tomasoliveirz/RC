// Write to serial port in non-canonical mode
//
// Modified by: Eduardo Nuno Almeida [enalmeida@fe.up.pt]
// edited by: Tomás Oliveira
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

#define FRAME_SIZE 5

volatile int STOP = FALSE;

#define MAX_RETRIES 3

#define TIMEOUT_SEC 3

volatile int STOP = FALSE;

volatile int alarmCounter = 0;

volatile int retryCounter = 0;

unsigned char EXPECTED_UA[5] = {0x7E, 0x01, 0x07, 0x06, 0x7E}; 
unsigned char SET_FRAME[5] = {0x7E, 0x03, 0x03, 0x00, 0x7E};

void alarmHandler(int signal)
{
    alarmCounter++;
    printf("Alarm %d\n", alarmCounter);
    STOP = TRUE;
}


int main(int argc, char *argv[])
{
    // Program usage: Uses either COM1 or COM2
    const char *serialPortName = argv[1];

    if (argc < 2)
    {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n",
               argv[0],
               argv[0]);
        exit(1);
    }

    // Open serial port device for reading and writing, and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    int fd = open(serialPortName, O_RDWR | O_NOCTTY);
    // O_RDWR - abre o arquivo para leitura e escrita
    // O_NOCTTY - nao torna o terminal o controlador do processo
    // serialPortName - nome da porta serial a ser aberta


    if (fd < 0)
    {
        perror(serialPortName);
        exit(-1);
    }

    struct termios oldtio;
    struct termios newtio;

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0; /* desativa o modo canonico e outras funcoes (echo por exemplo)
    configurando o terminal para trabalhar em modo nao canonico onde a 
    entrada de dados e feita byte a byte, sem a necessidade de um 
    caractere de terminacao /n
    */
    newtio.c_cc[VTIME] = 0; /* define que a leitura
    nao tera timeout, ou seja, apos a leitura de um byte o programa 
    continuara a execucao, bloqueando ate que um byte seja lido
    */
    newtio.c_cc[VMIN] = 5;  /* a funcao read() so retornara quando
    receber 5 bytes, ou seja, o programa ficara bloqueado ate que 5 bytes 
    sejam lidos (garante que o programa espere por 5 bytes pacotes completos)
    */


    // VTIME e VMIN should be changed in order to protect with a
    // timeout the reception of the following character(s)

    // Now clean the line and activate the settings for the port
    // tcflush() discards data written to the object referred to
    // by fd but not transmitted, or data received but not read,
    // depending on the value of queue_selector:
    //   TCIFLUSH - flushes data received but not read.


    tcflush(fd, TCIOFLUSH); /*
    tcflush é
    usada para descartar (flush) qualquer
    dado pendente que esteja no buffer de entrada ou saida da porta serial
    é util para garantir que nenhuma informacao seja perdida ou misturada

    fd - descritor do arquivo, que ja foi aberto anteriormente
    usando a funcao open(). ele identifica a porta serial que
    estamos a configurar

    TCIOFLUSH - descarta ambos os buffers de entrada e saida
    TCIFLUSH - descarta o buffer de entrada
    TCOFLUSH - descarta o buffer de saida

    */

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &alarmHandler;

    // instala o handler para o sinal SIGALRM
    if (sigaction(SIGALRM, &sa, NULL) == -1)
    {
        perror("sigaction");
        tcsetattr(fd, TCSANOW, &oldtio);
        close(fd);
        exit(-1);
    }

    SET_FRAME[3] = SET_FRAME[1] ^ SET_FRAME[2];

    int ua_received = FALSE;
    
    while (retryCount < MAX_RETRIES && !ua_received)
    {
        // send the set frame
        int bytes_written = write(fd, SET_FRAME, sizeof(SET_FRAME));
        if (bytes_written < 0)
        {
            perror("write SET_FRAME");
            tcsetattr(fd, TCSANOW, &oldtio);
            close(fd);
            exit(EXIT_FAILURE);
                    
        }

        printf("SET_FRAME sent (%d bytes)\n", bytes_written);
        
        for (int i = 0; i < sizeof(SET_FRAME); i++)
        {
            printf("%02X ", SET_FRAME[i]);
        }
        printf("\n");

        STOP = FALSE;
        alarmCounter = 0;

        alarm(TIMEOUT_SEC);

        // In non-canonical mode, '\n' does not end the writing.
        // Test this condition by placing a '\n' in the middle of the buffer.
        // The whole buffer must be sent even with the '\n'.

        unsigned char received_frame[FRAME_SIZE];

        memset(received_frame, 0, sizeof(received_frame));

        //fiquei aqui

        int bytes_read = read(fd, received_frame, sizeof(received_frame));
        
        if (bytes_read < 0)
        {

            if (errno == EINTR)
            {
                printf("Sender: Timeout - waiting for UA\n");
                retryCounter++;
                continue; // tentar novamente
            }

            else
            {
                perror("read UA failed");
                tcsetattr(fd, TCSANOW, &oldtio);
                close(fd);
                exit(-1);
            }
        }
        else if (bytes_read == 0)
        {
            printf("Sender: No bytes received\n");
            retryCounter++;
            continue;
        }

        alarm(0);

        printf("Sender: UA received (%d bytes)\n", bytes_read);
        for (int i = 0; i < bytes_read; i++)
        {
            printf("%02X ", received_frame[i]);
        }
        printf("\n");

        if (bytes_read == FRAME_SIZE && memcmp(received_frame, EXPECTED_UA, FRAME_SIZE) == 0)
        {
            printf("Sender: UA is correct - connection established\n");
            ua_received = TRUE;
        }
        else
        {
            printf("Sender: UA is incorrect\n");
            retryCounter++;
        }

    }
    
    if (!ua_received)
    {
        printf("Sender: Maximum retries reached. %d retries\n", retryCounter);
    }
    
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        close(fd);
        exit(-1);
    }

    close(fd);
    return 0;
}

