--
-- Database: `loraserv`
--

ATTACH DATABASE '/tmp/loraserv' As 'loraserv';
-- --------------------------------------------------------
--
-- Table structure for table `nsdevinfo`
--

CREATE TABLE IF NOT EXISTS `nsdevinfo` (
  `deveui` varchar(16) PRIMARY KEY NOT NULL,
  `appkey` bigint(20) DEFAULT NULL,
  `devaddr` int(10) DEFAULT NULL,
  `joineui` bigint(20) DEFAULT NULL,
  `nwkskey` bigint(20) DEFAULT NULL,
  `downcnt` int(10) DEFAULT '0',
  `class` integer DEFAULT '0',
  `downlinkStatus` text
);

-- --------------------------------------------------------

--
-- Table structure for table `lastjoininfo`
--

CREATE TABLE IF NOT EXISTS `lastjoininfo` (
  `deveui` varchar(16) PRIMARY KEY NOT NULL,
  `tmst` bigint(16) DEFAULT NULL,
  `devnoce` int(10) DEFAULT NULL,
  `upcnt` int(10) DEFAULT NULL
); 


--
-- Table structure for table `appsdevinfo`
--

CREATE TABLE IF NOT EXISTS `appsdevinfo` (
  `appid` integer PRIMARY KEY AUTOINCREMENT,
  `deveui` varchar(16) DEFAULT NULL,
  `appeui` varchar(16) DEFAULT NULL,
  `appkey` bigint(20) DEFAULT NULL,
  `appskey` bigint(20) DEFAULT '0'
);


--
-- Table structure for table `appsdevinfo`
--

CREATE TABLE IF NOT EXISTS `lastappinfo` (
  `deveui` varchar(16) PRIMARY KEY NOT NULL,
  `appkey` bigint(20) DEFAULT NULL,
  `appskey` bigint(20) DEFAULT '0',
  `tmst` bigint(16) DEFAULT NULL,
  `devnoce` int(10) DEFAULT NULL,
  `upcnt` int(10) DEFAULT NULL
);

--
-- Table structure for table `transarg`
--

CREATE TABLE IF NOT EXISTS `transarg` (
  `deveui` varchar(16) PRIMARY KEY NOT NULL,
  `gwaddr` varchar(32) NOT NULL,
  `delay` int(10) NOT NULL,
  `rx1datarate` int(10) NOT NULL,
  `rx2datarate` int(10) NOT NULL,
  `rx2freq` float NOT NULL
);

INSERT OR IGNORE INTO 'appsdevinfo' (deveui, appeui, appkey) VALUES ('aabbccdd', '12341234', '1234567812345678');
INSERT OR IGNORE INTO 'nsdevinfo' (deveui, appkey) VALUES ('aabbccdd', '1234567812345678');
