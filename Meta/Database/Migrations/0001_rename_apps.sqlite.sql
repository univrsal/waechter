ALTER TABLE "Application" RENAME TO "TrafficItem";
ALTER TABLE "TrafficItem" RENAME COLUMN "BinaryPath" TO "Name";
ALTER TABLE "TrafficEvent" RENAME COLUMN "AppID" TO "ItemID";
PRAGMA foreign_keys = OFF;
CREATE TABLE TrafficEvent_new (
    ID INTEGER,
    SnapshotID INTEGER NOT NULL,
    ItemID INTEGER NOT NULL,
    HostID INTEGER,
    BytesIn INTEGER NOT NULL,
    BytesOut INTEGER NOT NULL,
    PRIMARY KEY(ID),
    FOREIGN KEY(HostID) REFERENCES Host(ID),
    FOREIGN KEY(ItemID) REFERENCES TrafficItem(ID),
    FOREIGN KEY(SnapshotID) REFERENCES TrafficSnapshot(ID)
);

INSERT INTO TrafficEvent_new
SELECT * FROM TrafficEvent;

DROP TABLE TrafficEvent;
ALTER TABLE TrafficEvent_new RENAME TO TrafficEvent;

PRAGMA foreign_keys = ON;