-- Removal data of the bots module (design decision 0034): drops the price table of the bots' auction house that
-- data/sql/db-world/2026_11_22_00_playerbots_item_price.sql created. The module reads the table only while
-- AiPlayerbot.Auction.Enable = 1; with the table gone and the switch on, it logs one line and trades nothing.
-- Use it when the module is taken out for good. It is safe to run again.

DROP TABLE IF EXISTS `playerbots_item_price`;
