/**
 * Author: Dragino 
 * Date: 16/01/2018
 * 
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.dragino.com
 *
*/

#ifndef _GPIO_H
#define _GPIO_H

/*******************************************************************************
 * GPIO/SPI configure
********************************************************************************/

#define LOW 		0
#define HIGH 		1

#define GPIO_OUT        0
#define GPIO_IN         1

#define SX127x_RESET_IO   12

/*
 * Read the state of the port. The port can be input
 * or output.
 * @gpio the GPIO pin to read from.
 * @return GPIO_HIGH if the pin is HIGH, GPIO_LOW if the pin is low. Output
 * is -1 when an error occurred.
 */
int digital_read(int);

void digital_write(int, int);

#endif   //defined _GPIO_H
