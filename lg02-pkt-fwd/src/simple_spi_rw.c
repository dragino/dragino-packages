#include "radio.h"
int DEBUG_INFO = 0;

int main (int argc, char *argv[]) {
    FILE *fp;

    int rst_pin = 0;
    int ret = 1;

    if (argc < 3) 
        return ret;

    rst_pin = atoi(argv[1]);

    if (rst_pin < 0 || rst_pin > 40) 
        return ret;

    radiodev *loradev;
    loradev = (radiodev *) malloc(sizeof(radiodev));
    loradev->nss = 15;
    loradev->rst = rst_pin;
    loradev->dio[0] = 7;
    loradev->dio[1] = 6;
    loradev->dio[2] = 0;	
    strcpy(loradev->desc, "LG02 detect");	
    loradev->spiport = lgw_spi_open(SPI_DEV_TX);

    if ((fp = fopen("/var/iot/board", "w")) < 0) {
        perror("open board");
        close(loradev->spiport);
        free(loradev);
        return ret;
    }

    if (get_radio_version(loradev)) {  
        fprintf(fp, "%s\n", argv[2]);
        ret = 0;
    }

    fflush(fp);
    fclose(fp);

    close(loradev->spiport);

    free(loradev);

    return ret;

}
