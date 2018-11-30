#include "radio.h"
int DEBUG_INFO = 0;

int main (int argc, char *argv[]) {

    int rst_pin = 0, spidev = 0;
    int ret = 1;

    if (argc < 3) 
        return ret;

    rst_pin = atoi(argv[1]);

    spidev = atoi(argv[2]);

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

    if (spidev)
        loradev->spiport = lgw_spi_open(SPI_DEV_TX);
    else
        loradev->spiport = lgw_spi_open(SPI_DEV_RX);

    if (get_radio_version(loradev)) {  
        ret = 0;
    }

    close(loradev->spiport);

    free(loradev);

    return ret;

}
