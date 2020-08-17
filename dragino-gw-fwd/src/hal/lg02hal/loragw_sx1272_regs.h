/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
    (C)2013 Semtech

Description: SX1272 FSK modem registers

License: Revised BSD License, see LICENSE.TXT file include in the project

Maintainer: Michael Coracin
*/
#ifndef _LORAGW_SX1272_REGS_H
#define _LORAGW_SX1272_REGS_H

/*!
 * ============================================================================
 * SX1272 Internal FSK registers Address
 * ============================================================================
 */
#define SX1272_REG_FIFO                                    0x00
// Common settings
#define SX1272_REG_OPMODE                                  0x01
#define SX1272_REG_BITRATEMSB                              0x02
#define SX1272_REG_BITRATELSB                              0x03
#define SX1272_REG_FDEVMSB                                 0x04
#define SX1272_REG_FDEVLSB                                 0x05
#define SX1272_REG_FRFMSB                                  0x06
#define SX1272_REG_FRFMID                                  0x07
#define SX1272_REG_FRFLSB                                  0x08
// Tx settings
#define SX1272_REG_PACONFIG                                0x09
#define SX1272_REG_PARAMP                                  0x0A
#define SX1272_REG_OCP                                     0x0B
// Rx settings
#define SX1272_REG_LNA                                     0x0C
#define SX1272_REG_RXCONFIG                                0x0D
#define SX1272_REG_RSSICONFIG                              0x0E
#define SX1272_REG_RSSICOLLISION                           0x0F
#define SX1272_REG_RSSITHRESH                              0x10
#define SX1272_REG_RSSIVALUE                               0x11
#define SX1272_REG_RXBW                                    0x12
#define SX1272_REG_AFCBW                                   0x13
#define SX1272_REG_OOKPEAK                                 0x14
#define SX1272_REG_OOKFIX                                  0x15
#define SX1272_REG_OOKAVG                                  0x16
#define SX1272_REG_RES17                                   0x17
#define SX1272_REG_RES18                                   0x18
#define SX1272_REG_RES19                                   0x19
#define SX1272_REG_AFCFEI                                  0x1A
#define SX1272_REG_AFCMSB                                  0x1B
#define SX1272_REG_AFCLSB                                  0x1C
#define SX1272_REG_FEIMSB                                  0x1D
#define SX1272_REG_FEILSB                                  0x1E
#define SX1272_REG_PREAMBLEDETECT                          0x1F
#define SX1272_REG_RXTIMEOUT1                              0x20
#define SX1272_REG_RXTIMEOUT2                              0x21
#define SX1272_REG_RXTIMEOUT3                              0x22
#define SX1272_REG_RXDELAY                                 0x23
// Oscillator settings
#define SX1272_REG_OSC                                     0x24
// Packet handler settings
#define SX1272_REG_PREAMBLEMSB                             0x25
#define SX1272_REG_PREAMBLELSB                             0x26
#define SX1272_REG_SYNCCONFIG                              0x27
#define SX1272_REG_SYNCVALUE1                              0x28
#define SX1272_REG_SYNCVALUE2                              0x29
#define SX1272_REG_SYNCVALUE3                              0x2A
#define SX1272_REG_SYNCVALUE4                              0x2B
#define SX1272_REG_SYNCVALUE5                              0x2C
#define SX1272_REG_SYNCVALUE6                              0x2D
#define SX1272_REG_SYNCVALUE7                              0x2E
#define SX1272_REG_SYNCVALUE8                              0x2F
#define SX1272_REG_PACKETCONFIG1                           0x30
#define SX1272_REG_PACKETCONFIG2                           0x31
#define SX1272_REG_PAYLOADLENGTH                           0x32
#define SX1272_REG_NODEADRS                                0x33
#define SX1272_REG_BROADCASTADRS                           0x34
#define SX1272_REG_FIFOTHRESH                              0x35
// SM settings
#define SX1272_REG_SEQCONFIG1                              0x36
#define SX1272_REG_SEQCONFIG2                              0x37
#define SX1272_REG_TIMERRESOL                              0x38
#define SX1272_REG_TIMER1COEF                              0x39
#define SX1272_REG_TIMER2COEF                              0x3A
// Service settings
#define SX1272_REG_IMAGECAL                                0x3B
#define SX1272_REG_TEMP                                    0x3C
#define SX1272_REG_LOWBAT                                  0x3D
// Status
#define SX1272_REG_IRQFLAGS1                               0x3E
#define SX1272_REG_IRQFLAGS2                               0x3F
// I/O settings
#define SX1272_REG_DIOMAPPING1                             0x40
#define SX1272_REG_DIOMAPPING2                             0x41
// Version
#define SX1272_REG_VERSION                                 0x42
// Additional settings
#define SX1272_REG_AGCREF                                  0x43
#define SX1272_REG_AGCTHRESH1                              0x44
#define SX1272_REG_AGCTHRESH2                              0x45
#define SX1272_REG_AGCTHRESH3                              0x46
#define SX1272_REG_PLLHOP                                  0x4B
#define SX1272_REG_TCXO                                    0x58
#define SX1272_REG_PADAC                                   0x5A
#define SX1272_REG_PLL                                     0x5C
#define SX1272_REG_PLLLOWPN                                0x5E
#define SX1272_REG_FORMERTEMP                              0x6C
#define SX1272_REG_BITRATEFRAC                             0x70

/*!
 * ============================================================================
 * SX1272 Internal LoRa registers Address
 * ============================================================================
 */
#define SX1272_REG_LR_FIFO                                 0x00
// Common settings
#define SX1272_REG_LR_OPMODE                               0x01
#define SX1272_REG_LR_FRFMSB                               0x06
#define SX1272_REG_LR_FRFMID                               0x07
#define SX1272_REG_LR_FRFLSB                               0x08
// Tx settings
#define SX1272_REG_LR_PACONFIG                             0x09
#define SX1272_REG_LR_PARAMP                               0x0A
#define SX1272_REG_LR_OCP                                  0x0B
// Rx settings
#define SX1272_REG_LR_LNA                                  0x0C
// LoRa registers
#define SX1272_REG_LR_FIFOADDRPTR                          0x0D
#define SX1272_REG_LR_FIFOTXBASEADDR                       0x0E
#define SX1272_REG_LR_FIFORXBASEADDR                       0x0F
#define SX1272_REG_LR_FIFORXCURRENTADDR                    0x10
#define SX1272_REG_LR_IRQFLAGSMASK                         0x11
#define SX1272_REG_LR_IRQFLAGS                             0x12
#define SX1272_REG_LR_RXNBBYTES                            0x13
#define SX1272_REG_LR_RXHEADERCNTVALUEMSB                  0x14
#define SX1272_REG_LR_RXHEADERCNTVALUELSB                  0x15
#define SX1272_REG_LR_RXPACKETCNTVALUEMSB                  0x16
#define SX1272_REG_LR_RXPACKETCNTVALUELSB                  0x17
#define SX1272_REG_LR_MODEMSTAT                            0x18
#define SX1272_REG_LR_PKTSNRVALUE                          0x19
#define SX1272_REG_LR_PKTRSSIVALUE                         0x1A
#define SX1272_REG_LR_RSSIVALUE                            0x1B
#define SX1272_REG_LR_HOPCHANNEL                           0x1C
#define SX1272_REG_LR_MODEMCONFIG1                         0x1D
#define SX1272_REG_LR_MODEMCONFIG2                         0x1E
#define SX1272_REG_LR_SYMBTIMEOUTLSB                       0x1F
#define SX1272_REG_LR_PREAMBLEMSB                          0x20
#define SX1272_REG_LR_PREAMBLELSB                          0x21
#define SX1272_REG_LR_PAYLOADLENGTH                        0x22
#define SX1272_REG_LR_PAYLOADMAXLENGTH                     0x23
#define SX1272_REG_LR_HOPPERIOD                            0x24
#define SX1272_REG_LR_FIFORXBYTEADDR                       0x25
#define SX1272_REG_LR_FEIMSB                               0x28
#define SX1272_REG_LR_FEIMID                               0x29
#define SX1272_REG_LR_FEILSB                               0x2A
#define SX1272_REG_LR_RSSIWIDEBAND                         0x2C
#define SX1272_REG_LR_DETECTOPTIMIZE                       0x31
#define SX1272_REG_LR_INVERTIQ                             0x33
#define SX1272_REG_LR_DETECTIONTHRESHOLD                   0x37
#define SX1272_REG_LR_SYNCWORD                             0x39
#define SX1272_REG_LR_INVERTIQ2                            0x3B

// end of documented register in datasheet
// I/O settings
#define SX1272_REG_LR_DIOMAPPING1                          0x40
#define SX1272_REG_LR_DIOMAPPING2                          0x41
// Version
#define SX1272_REG_LR_VERSION                              0x42
// Additional settings
#define SX1272_REG_LR_AGCREF                               0x43
#define SX1272_REG_LR_AGCTHRESH1                           0x44
#define SX1272_REG_LR_AGCTHRESH2                           0x45
#define SX1272_REG_LR_AGCTHRESH3                           0x46
#define SX1272_REG_LR_PLLHOP                               0x4B
#define SX1272_REG_LR_TCXO                                 0x58
#define SX1272_REG_LR_PADAC                                0x5A
#define SX1272_REG_LR_PLL                                  0x5C
#define SX1272_REG_LR_PLLLOWPN                             0x5E
#define SX1272_REG_LR_FORMERTEMP                           0x6C

#endif // _LORAGW_SX1272_REGS_H
