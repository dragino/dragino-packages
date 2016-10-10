comgt -s /etc/gcom/reset_uc20.gcom  -v -d /dev/ttyUSB2
sleep 20
cp /etc/gcom/setpin.gcom.bak /etc/gcom/setpin.gcom
sed "s/OLDPIN/$1/" /etc/gcom/setpin.gcom -i
sed "s/NEWPIN/$2/" /etc/gcom/setpin.gcom -i
comgt -s /etc/gcom/setpin.gcom  -v -d /dev/ttyUSB2



