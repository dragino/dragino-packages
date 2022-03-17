#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <signal.h>     /* sigaction */
#include <mpsse.h>

#define  VID          0x0403
#define  PID          0x6010

/* signal handling variables */
volatile bool exit_sig = false; /* 1 -> application terminates cleanly (shut down hardware, close open files, etc) */
volatile bool quit_sig = false; /* 1 -> application terminates without shutting down the hardware */

static void sig_handler(int sigio);

/*
int main() {
    int i;
    pthread_t thrid_lbt;

    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigact.sa_handler = sig_handler;
    sigaction(SIGQUIT, &sigact, NULL); 
    sigaction(SIGINT, &sigact, NULL); 
    sigaction(SIGTERM, &sigact, NULL); 

    sigemptyset(&sigusr.sa_mask);
    sigusr.sa_flags = 0;
    sigusr.sa_handler = sigusr_handler;

}

*/
static void sig_handler(int sigio) {
    if (sigio == SIGQUIT) {
        quit_sig = true;
    } else if ((sigio == SIGINT) || (sigio == SIGTERM)) {
        exit_sig = true;
    }
    return;
}

int main(void)
{
	int a, b;
	int retval = EXIT_FAILURE;
	struct mpsse_context *mpsse = NULL;
	uint8_t out_buf[3];
    uint8_t command_size;
	uint8_t *in_buf = NULL;
	//struct test_s *test = NULL;
    //test_mal(&test);
    //printf("RUN1: size=%u, test=%p, open=%d, vid=%d, pid=%d\n", sizeof(struct test_s), test, test->open, test->vid, test->pid);
    mpsse = OpenIndex(VID, PID, SPI0, SIX_MHZ, MSB, IFACE_A, NULL, NULL, 0);
    if (mpsse == NULL) {
            printf("ERROR: MPSSE OPEN FUNCTION RETURNED NULL\n");
            return retval;
    }
    printf("RUN2: mpsse=%p, open=%d, vid=%d, pid=%d\n", mpsse, mpsse->open, mpsse->vid, mpsse->pid);
    if (mpsse->open != 1) {
            printf("ERROR: MPSSE OPEN FUNCTION FAILED\n");
            Close(mpsse);
            return retval;
    }

    b = PinLow(mpsse, GPIOL0);
    usleep(10000);
    a = PinHigh(mpsse, GPIOL0);

    out_buf[0] = 0x00 | (0x42 & 0x7F);
    out_buf[1] = 0x00;
    command_size = 2;

    a = Start(mpsse);
    in_buf = (uint8_t *)Transfer(mpsse, (char *)out_buf, command_size);
    b = Stop(mpsse);

    if ((a != MPSSE_OK) || (b != MPSSE_OK)) {
            printf("ERROR: IMPOSSIBLE TO TOGGLE GPIOL1/ADBUS5\n");
            return retval;
    }

    printf("Read:in_buf=0x%02X\n", in_buf[1]);

    Close(mpsse);

	return retval;
}
