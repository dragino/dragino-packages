#include <stdio.h>
#include <stdlib.h>
#include <mpsse.h>

#define  VID          0x0403
#define  PID          0x6010

int main(void)
{
	int a, b;
	int retval = EXIT_FAILURE;
	struct mpsse_context *mpsse = NULL;
	struct test_s *test = NULL;
        test_mal(&test);
        printf("RUN1: size=%u, test=%p, open=%d, vid=%d, pid=%d\n", sizeof(struct test_s), test, test->open, test->vid, test->pid);
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

        /*it resets the SX1276 */
        a = PinHigh(mpsse, GPIOL0);
        b = PinLow(mpsse, GPIOL0);

        if ((a != MPSSE_OK) || (b != MPSSE_OK)) {
                printf("ERROR: IMPOSSIBLE TO TOGGLE GPIOL1/ADBUS5\n");
                return retval;
        }

        Close(mpsse);

	return retval;
}
