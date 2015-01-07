
CREATE TABLE IF NOT EXISTS Version (
    Version     TEXT NOT NULL,
    Salt        BLOB NOT NULL,
    KeyCheck    BLOB NOT NULL
);

CREATE TABLE IF NOT EXISTS Entry (
    ID          BLOB PRIMARY KEY,
    ParentID    BLOB,
    Row         INTEGER NOT NULL,
    FileID      BLOB,
    Favorite    INTEGER NOT NULL,
    Data        BLOB NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_Entry_ParentID_Row ON Entry (ParentID, Row ASC);
CREATE INDEX IF NOT EXISTS idx_Entry_Favorite ON Entry (Favorite ASC);

CREATE TABLE IF NOT EXISTS File (
    ID      BLOB PRIMARY KEY,
    Length  INTEGER NOT NULL,
    Data    BLOB NOT NULL
);
