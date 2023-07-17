SET @ENTRY:=45000;
DELETE FROM `acore_string` WHERE `entry`=@ENTRY+0;
INSERT INTO `acore_string` (`entry`, `content_default`, `locale_koKR`, `locale_frFR`, `locale_deDE`, `locale_zhCN`, `locale_zhTW`, `locale_esES`, `locale_esMX`, `locale_ruRU`) VALUES
(@ENTRY+0, 'This server is running the |cff4CFF00mounts on account|r module.', '', '', '', '', '', 'Este servidor está ejecutando el módulo |cff4CFF00mounts on account|r.', 'Este servidor está ejecutando el módulo |cff4CFF00mounts on account|r.', '');
