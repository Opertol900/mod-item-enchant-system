-- Default scrolls. Change entries in both this file and the module config if needed.
DELETE FROM `item_template_locale` WHERE `ID` BETWEEN 900100 AND 900105;
DELETE FROM `item_template` WHERE `entry` BETWEEN 900100 AND 900105;

INSERT INTO `item_template`
(`entry`,`class`,`subclass`,`name`,`displayid`,`Quality`,`BuyCount`,`InventoryType`,`AllowableClass`,`AllowableRace`,
 `ItemLevel`,`RequiredLevel`,`maxcount`,`stackable`,`spellid_1`,`spelltrigger_1`,`bonding`,`description`,`Material`,
 `ScriptName`,`VerifiedBuild`)
VALUES
(900100,0,8,'Scroll: Enchant Item',13493,2,1,0,-1,-1,1,1,0,20,483,0,1,'Failure destroys the target item.',3,'item_mod_enchant_scroll',12340),
(900101,0,8,'Blessed Scroll: Enchant Item',13493,3,1,0,-1,-1,1,1,0,20,483,0,1,'Failure preserves the item and resets its enchant level.',3,'item_mod_enchant_scroll',12340),
(900102,0,8,'Safe Scroll: Enchant Item',13493,3,1,0,-1,-1,1,1,0,20,483,0,1,'Failure preserves both the item and its enchant level.',3,'item_mod_enchant_scroll',12340),
(900103,0,8,'Crystal Scroll: Enchant Item',13493,4,1,0,-1,-1,1,1,0,20,483,0,1,'Guaranteed enchanting up to the configured crystal limit.',3,'item_mod_enchant_scroll',12340),
(900104,0,8,'Event Scroll: Enchant Item',13493,4,1,0,-1,-1,1,1,0,20,483,0,1,'Behavior is controlled by the server configuration.',3,'item_mod_enchant_scroll',12340),
(900105,0,8,'GM Scroll: Enchant Item',13493,6,1,0,-1,-1,1,1,0,20,483,0,1,'Administrator-only guaranteed enchant scroll.',3,'item_mod_enchant_scroll',12340);

INSERT INTO `item_template_locale` (`ID`,`locale`,`Name`,`Description`,`VerifiedBuild`) VALUES
(900100,'ruRU','Свиток: заточка предмета','При неудаче предмет уничтожается.',12340),
(900101,'ruRU','Благословенный свиток: заточка предмета','При неудаче предмет сохраняется, а уровень заточки сбрасывается.',12340),
(900102,'ruRU','Безопасный свиток: заточка предмета','При неудаче предмет и его уровень заточки сохраняются.',12340),
(900103,'ruRU','Кристальный свиток: заточка предмета','Гарантированная заточка до настроенного предела.',12340),
(900104,'ruRU','Ивентовый свиток: заточка предмета','Поведение задаётся конфигурацией сервера.',12340),
(900105,'ruRU','GM-свиток: заточка предмета','Гарантированный свиток только для администрации.',12340);
