/*
  Warnings:

  - Added the required column `pwm` to the `Data` table without a default value. This is not possible if the table is not empty.

*/
-- RedefineTables
PRAGMA defer_foreign_keys=ON;
PRAGMA foreign_keys=OFF;
CREATE TABLE "new_Data" (
    "id" INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
    "adc" INTEGER NOT NULL,
    "pwm" INTEGER NOT NULL,
    "voltage" REAL NOT NULL,
    "resistance" REAL NOT NULL,
    "createdAt" DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
);
INSERT INTO "new_Data" ("adc", "createdAt", "id", "resistance", "voltage") SELECT "adc", "createdAt", "id", "resistance", "voltage" FROM "Data";
DROP TABLE "Data";
ALTER TABLE "new_Data" RENAME TO "Data";
PRAGMA foreign_keys=ON;
PRAGMA defer_foreign_keys=OFF;
