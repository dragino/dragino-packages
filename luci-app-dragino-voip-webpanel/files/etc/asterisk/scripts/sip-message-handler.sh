#!/bin/ash
# AGI script to handle the SIP message from Server. 


while read ARG && [ "$ARG" ] ; do
	ARG1=$ARG2
	ARG2=$ARG3
	ARG3=`echo $ARG | sed -e 's/://' | cut -d' ' -f2`
done
COM1=$ARG3
COM2=$ARG2
COM3=$ARG1

#update-config() {
#;
#}

#update-pkg() {
#
#}

case $COM1 in
	reboot) reboot;;
	update-config) echo "reboot" > /var/test;;
	update-pkg) echo "reboot" > /var/test;;	
	*) echo "no action" > /var/test;;
esac

exit 0