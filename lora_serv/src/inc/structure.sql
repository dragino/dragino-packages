--
-- Database: `lora_app`
--

ATTACH DATABASE 'loraserv' As 'loraserv';
-- --------------------------------------------------------
--
-- Table structure for table `devs`
--

CREATE TABLE IF NOT EXISTS `devs` (
  `deveui` text PRIMARY KEY DEFAULT NULL,
  `appeui` text default NULL,
  `appskey` blob DEFAULT NULL,
  `nwkskey` blob DEFAULT NULL,
  `devaddr` blob DEFAULT NULL,
  `fcntdown` INTEGER DEFAULT '0',
  `fcntup` INTEGER DEFAULT '0',
  `devnonce` blob DEFAULT '0'
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

CREATE TABLE IF NOT EXISTS `upmsg` (
  `id` integer PRIMARY key AUTOINCREMENT,
  `recvtime` timestamp NOT NULL default CURRENT_TIMESTAMP,
  `tmst` integer NOT NULL default 0,
  `datarate` integer DEFAULT 5,
  `freq` real DEFAULT NULL,
  `rssi` real DEFAULT NULL,
  `snr` real DEFAULT NULL,
  `fcntup` integer NOT NULL default 0,
  `confirmed` integer NOT NULL default 0,
  `frmpayload` blob DEFAULT NULL,
  `fport` integer NOT NULL default 7,
  `gweui` text DEFAULT NULL,
  `appeui` text DEFAULT NULL,
  `deveui` text DEFAULT NULL
); 


-- --------------------------------------------------------

--
-- Table structure for table `joinmotes`
--

CREATE TABLE IF NOT EXISTS `joindevs` (
  `id` INTEGER PRIMARY KEY AUTOINCREMENT,
  `deveui` text NOT NULL,
  `nwkskey` blob DEFAULT NULL,
  `appskey` blob DEFAULT NULL,
  `rjcount1_last` integer DEFAULT '0',
  `devnonce_last` integer DEFAULT '0',
  `lifetime` integer NOT NULL,
  `devnonce` blob DEFAULT '0'
);


--
-- Table structure for table `gwprofile`
--

CREATE TABLE IF NOT EXISTS `gwprofile` (
  `id` integer PRIMARY KEY AUTOINCREMENT,
  `supportsclassc` integer NOT NULL default 1,
  `classctimeout` integer NOT NULL default 0,
  `macversion` text NOT NULL default '1.0.3',
  `regparamsrevision` text NOT NULL default '1',
  `supportsjoin` integer NOT NULL default 1,
  `rx1delay` integer NOT NULL default 1,
  `rx1droffset` integer NOT NULL default 0,
  `rx2datarate` integer NOT NULL default 5,
  `rx2freq` real NOT NULL default 868.925,
  `maxeirp` integer NOT NULL default 1,
  `maxdutycycle` real NOT NULL default 1,
  `rfregion` text NOT NULL default 'EU868',
  `32bitfcnt` integer NOT NULL default 0
);

--
-- Table structure for table `gateways`
--

CREATE TABLE IF NOT EXISTS `gws` (
  `gweui` text PRIMARY KEY NOT NULL,
  `profileid` integer not null default 1,
  `description` text not null default 'dragino gw',
  `created_at` timestamp not null DEFAULT CURRENT_TIMESTAMP,
  `updated_at` timestamp DEFAULT CURRENT_TIMESTAMP,
  `first_seen_at` timestamp,
  `last_seen_at` timestamp,
  `maxtxpower_dbm` integer DEFAULT NULL default 26,
  `allowgpstosetposition` integer NOT NULL DEFAULT '1',
  `latitude` real DEFAULT NULL,
  `longitude` real DEFAULT NULL,
  `altitude` real DEFAULT NULL,
  `uprecv` integer  DEFAULT '0',
  `uprecvok` integer  NOT NULL DEFAULT '0',
  `upfwd` integer  NOT NULL DEFAULT '0',
  `upack` integer NOT NULL DEFAULT '0',
  `downrecv` integer  NOT NULL DEFAULT '0',
  `downtrans` integer  NOT NULL DEFAULT '0',
  `lastuppacketid` integer  DEFAULT NULL
);

--
-- Table structure for table `apps`
--

CREATE TABLE IF NOT EXISTS `apps` (
        `appeui` TEXT PRIMARY KEY not null,
        `description` TEXT,
        `appkey` BLOB NOT null
);
        

INSERT OR IGNORE INTO gws (gweui) VALUES ('a840411a92d44150');
INSERT OR IGNORE INTO apps (appeui, appkey) VALUES ('000C29FFFF189889', '3FF71C74EE5C4F18DFF3705455910AF6');
INSERT OR IGNORE INTO devs (deveui, appeui) VALUES ('1234590834221467', '000C29FFFF189889');
INSERT OR IGNORE INTO gwprofile (id) VALUES (1);

