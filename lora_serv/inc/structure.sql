--
-- Database: `lora_app`
--

ATTACH DATABASE 'loraserv' As 'loraserv';
-- --------------------------------------------------------
--
-- Table structure for table `loradevs`
--

CREATE TABLE IF NOT EXISTS `devs` (
  `deveui` INTEGER PRIMARY KEY DEFAULT NULL,
  `appid` INTEGER,
  `appskey` TEXT DEFAULT NULL,
  `devaddr` TEXT DEFAULT NULL,
  `nwkskey` TEXT DEFAULT NULL,
  `fcntdown` INTEGER DEFAULT '0',
  `fcntup` INTEGER DEFAULT '0',
  `joinnonce` INTEGER DEFAULT '0'
);

-- --------------------------------------------------------

--
-- Table structure for table `configuration`
--

CREATE TABLE IF NOT EXISTS `configuration` (
  `name` text PRIMARY KEY NOT NULL,
  `value` text NOT NULL
); 

-- --------------------------------------------------------

--
-- Table structure for table `upframes`
--

CREATE TABLE IF NOT EXISTS `upframes` (
  `ID` integer NOT NULL,
  `DataRate` real DEFAULT NULL,
  `ULFreq` real DEFAULT NULL,
  `RSSI` real DEFAULT NULL,
  `SNR` real DEFAULT NULL,
  `GWCnt` integer DEFAULT NULL,
  `FCntUp` integer NOT NULL,
  `RecvTime` datetime NOT NULL,
  `Confirmed` integer NOT NULL,
  `FRMPayload` blob DEFAULT NULL,
  `FPort` integer NOT NULL
  `GWEUI` text DEFAULT NULL,
  `DevEUI` text DEFAULT NULL,
); 


-- --------------------------------------------------------

--
-- Table structure for table `joinmotes`
--

CREATE TABLE IF NOT EXISTS `joindevs` (
  `ID` INTEGER PRIMARY KEY AUTOINCREMENT,
  `DevEUI` text NOT NULL,
  `NwkKey` text DEFAULT NULL,
  `AppKey` text DEFAULT NULL,
  `RJcount1_last` integer DEFAULT '0',
  `DevNonce_last` integer DEFAULT '0',
  `Lifetime` integer NOT NULL,
  `joinNonce` text DEFAULT '0'
);


--
-- Table structure for table `DeviceProfiles`
--

CREATE TABLE IF NOT EXISTS `gwprofile` (
  `ID` integer NOT NULL,
  `SupportsClassC` integer NOT NULL,
  `ClassCTimeout` integer NOT NULL,
  `MACVersion` text NOT NULL,
  `RegParamsRevision` text NOT NULL,
  `SupportsJoin` integer NOT NULL,
  `RX1Delay` integer NOT NULL,
  `RX1DROffset` integer NOT NULL,
  `RX2DataRate` integer NOT NULL,
  `RX2Freq` real NOT NULL,
  `MaxEIRP` integer NOT NULL,
  `MaxDutyCycle` real NOT NULL,
  `RFRegion` text NOT NULL,
  `32bitFCnt` integer NOT NULL,
);

--
-- Table structure for table `gateways`
--

CREATE TABLE IF NOT EXISTS `gateways` (
  `eui` integer PRIMARY KEY NOT NULL,
  `description` text not null,
  `created_at` timestamp not null,
  `updated_at` timestamp not null,
  `first_seen_at` timestamp,
  `last_seen_at` timestamp,
  `RFRegion` text,
  `maxTxPower_dBm` integer DEFAULT NULL,
  `allowGpsToSetPosition` integer NOT NULL DEFAULT '1',
  `time` timestamp NOT NULL DEFAULT NULL,
  `latitude` real DEFAULT NULL,
  `longitude` real DEFAULT NULL,
  `altitude` ellouble DEFAULT NULL,
  `ntwkMaxDelayUp_ms` integer  DEFAULT NULL, --'Max expected delay in ms from GW to NS',
  `ntwkMaxDelayDn_ms` smallint(5)  DEFAULT NULL, --'Max expected delay in ms from NS to Gw',
  `UpRecv` int(10)  DEFAULT '0',
  `UpRecvOK` int(10)  NOT NULL DEFAULT '0',
  `UpFwd` int(10)  NOT NULL DEFAULT '0',
  `UpACK` float NOT NULL DEFAULT '0',
  `DownRecv` int(10)  NOT NULL DEFAULT '0',
  `DownTrans` int(10)  NOT NULL DEFAULT '0',
  `lastuppacketid` bigint(20)  DEFAULT NULL,
);

--
-- Table structure for table `sessions`
--

CREATE TABLE IF NOT EXISTS `sessions` (
  `ID` int(10)  NOT NULL,
  `Until` datetime DEFAULT NULL COMMENT 'NULL for ABP',
  `DevAddr` int(10)  NOT NULL,
  `NFCntDown` int(10)  DEFAULT NULL,
  `FCntUp` int(10)  DEFAULT NULL,
  `FNwkSIntKey` binary(16) DEFAULT NULL,
  `SNwkSIntKey` binary(16) DEFAULT NULL,
  `NwkSEncKey` binary(16) DEFAULT NULL,
  `AS_KEK_label` varchar(256) DEFAULT NULL,
  `AS_KEK_key` varbinary(256) DEFAULT NULL,
  `createdAt` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP
);

create table if not exits `apps` (
        `app_id` integer primary key not null,
        `des` text,
        `appeui` text not null,
        `appkey` text not null
);
        

