

CREATE DATABASE httpserver;

USE httpserver;

CREATE TABLE scriptitem_t
(
	filename 			VARCHAR(256),
	code					VARCHAR(32),
	sub						INT,
	itemtype			INT,
	object1				INT,
	object2				INT,
	id						VARCHAR(32),
	scopy					VARCHAR(12),
	name					VARCHAR(32),
	property			VARCHAR(32),
	type					VARCHAR(12),
	value					CLOB
);

CREATE TABLE cspfileinfo_t
(
	filename 			VARCHAR(256),
	filesize			BIGINT UNSIGNED,
	lasttime			BIGINT UNSIGNED
);

