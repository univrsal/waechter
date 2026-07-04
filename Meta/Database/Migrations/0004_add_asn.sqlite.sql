CREATE TABLE IF NOT EXISTS "Asn" (
	"ID"	INTEGER,
	"Number"	INTEGER NOT NULL,
	"Organization"	TEXT NOT NULL,
	"Country"	TEXT NOT NULL,
	PRIMARY KEY("ID")
);
PRAGMA foreign_keys = OFF;
CREATE TABLE "Host_new" (
	"ID"	INTEGER,
	"Family"	INTEGER NOT NULL CHECK("Family" IN (4, 6)),
	"IPAddress"	BLOB NOT NULL,
	"AsnID"	INTEGER,
	PRIMARY KEY("ID"),
	FOREIGN KEY("AsnID") REFERENCES "Asn"("ID")
);
INSERT INTO "Host_new" ("ID", "Family", "IPAddress", "AsnID")
SELECT "ID", "Family", "IPAddress", NULL
FROM "Host";
DROP TABLE "Host";
ALTER TABLE "Host_new" RENAME TO "Host";
PRAGMA foreign_keys = ON;
