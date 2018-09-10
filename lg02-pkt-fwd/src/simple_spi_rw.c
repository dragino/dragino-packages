#include "radio.h"

int main () {
    FILE *fp;

    radiodev *loradev;
    loradev = (radiodev *) malloc(sizeof(radiodev));
    loradev->nss = 15;
    loradev->rst = 8;
    loradev->dio[0] = 7;
    loradev->dio[1] = 6;
    loradev->dio[2] = 0;	
    strcpy(loradev->desc, "LG02 detect");	
    loradev->spiport = lgw_spi_open(SPI_DEV_RX);

    if ((fp = fopen("/var/iot/board", "w")) < 0) {
        perror("open board");
        close(loradev->spiport);
        free(loradev);
        return 1;
    }

    if (get_radio_version(loradev))  
        fprintf(fp, "LG02\n");
    else {
        close(loradev->spiport);
        loradev->nss = 21;
        loradev->rst = 12;
        strcpy(loradev->desc, "LG08P detect");	
        loradev->spiport = lgw_spi_open(SPI_DEV_TX);
        if (get_radio_version(loradev))  
            fprintf(fp, "LG08P\n");
        else
            fprintf(fp, "LG08\n");
    }

    fflush(fp);
    fclose(fp);

    close(loradev->spiport);

    free(loradev);

    return 0;

}
