#!/bin/sh
fwd=`pidof lora_pkt_fwd`
[ -z ${fwd} ] || /etc/init.d/lora_gw stop
sleep 1
echo "GPIO23 input/output testing..."
test=`reset_lgw.sh start`
[ -z ${fwd} ] || {
    echo ">>FIAL: GPIO23 read/write ERROR!"
    exit 1
}
echo ">>PASS: GPIO23 input/output"
echo "SPI tesing..."
/usr/lora/test_loragw_spi


