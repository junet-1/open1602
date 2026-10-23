# 0012 – Goods statistics: a ledger per city, the window "Warenbilanz", the early warning

Status: accepted · 2026-10-23

The original shows what a warehouse holds, and a building's productivity, but not whether a city
makes more of a good than it uses. A player only learns of a shortage when the store is empty
("Es fehlt an …"). Open1602 adds statistics in the spirit of later Anno titles: for every good, what
was produced, bought and unloaded against what the inhabitants, the workshops, the soldiers and the
builders used and what was sold, for all of the player's cities or one island's, and a warning
before a good the people need runs out.

## Decision

1. **A ledger per city (`sim::GoodsLedger`, `City::ledger`).** Every movement of a good is booked
   where it happens, in 1/32 t like the stock, as one of twelve flows (`sim::GoodsFlow`):
   - in: `Produced`, `Bought`, `ShippedIn`, `Returned`;
   - out: `Residents`, `Workshops`, `Military`, `Construction`, `Sold`, `ShippedOut`, `Lost`;
   - and `Demand`, which is not a movement: what the inhabitants asked for. Together with
     `Residents` it gives how well they were supplied.

   Production and the raw material of a cycle are booked at the building when the cycle ends
   (`production_cycle.cpp`), not when a carrier brings the goods to the warehouse. The statistics
   show the economy, and intermediate goods that never reach the warehouse still count. Carriers
   moving goods between the warehouse and buildings are not booked, since they only move goods inside
   the city. Ships book `ShippedIn`/`ShippedOut` at a Kontor of their owner and `Bought`/`Sold` at
   another player's. The free trader and the natives book `Bought`/`Sold`. A city that changes hands
   starts a new ledger.

   The goods a computer player is given (`ai_world.cpp`, `add_goods`) are deliberately not booked.
   Computer players' statistics are never shown.
2. **Minutes and quarters.** Every game minute (`kGoodsMinuteMs`, the last step of the tick,
   `statistics`) the running counts become a sample, together with the stock at the minute's end.
   - The last 60 minutes are kept one by one.
   - The last 48 quarters of an hour (12 hours) are kept as average minutes.
   - A sample holds 16 bits per flow (up to 2047 t a minute) and 32 bits of stock.

   The memory is fixed per good, about 3 KB per good and city. Only goods ever booked have a ledger,
   so a long game costs no more than a short one.
3. **Deterministic and saved.** Integers and game time only ([ADR 0004](0004-deterministic-simulation.md)).
   - The ledgers are saved in the optional section `GLDG` ([o1602-save.md](../formats/o1602-save.md)),
     only for cities that have statistics.
   - They are part of the state hash, so a multiplayer game compares them like everything else. A
     computer that joins or resyncs gets them with the savegame
     ([ADR 0011](0011-multiplayer-lockstep.md)).
   - The gameplay does not depend on them. The early warning only puts a message on the screen.
   - Adding the section changed the hash of the AI's golden test (`test_ai.cpp`). Without `GLDG`, the
     hash is the old one.
4. **Only one's own cities.** `sim::GoodsScope` sums a player's cities, all of them or one island's.
   The window shows the local player's only. Allies and trading partners never see each other's
   figures, although every computer of a multiplayer game computes all of them.
5. **The early warning (`CityEvent::Kind::RunningLow`).** Once a minute, for the cities of human
   players and the goods the inhabitants consume (`consumer_goods`):
   - The rule: the inhabitants used the good in the last 5 minutes, there is still some in store,
     and at the balance of those minutes it lasts less than 5 minutes.
   - The warning comes once, in the voice of the original's "Es fehlt an …" (MELDUNG 0x34 +
     good − food) with its own text. The original's message for the empty store is then not repeated.
   - It is given again only after the stock has lasted 10 minutes once more (or grows).
6. **The window "Warenbilanz" (`app/goods_window.hpp`).** It opens over the map with F2 (key action
   `goods_statistics`, listed on the F1 page) or with the line "Warenbilanz" Open1602 adds to the
   player status (INFO.GAD). The game goes on behind it.
   - The table: the island (all, or one, by its city's name), the time averaged over (1, 5, 15 or
     60 game minutes; 5 at first) and the goods grouped as consumer goods, building material, raw
     materials and intermediates, and weapons. The columns are stock, produced, imported, used,
     exported, balance (green or red) and how long the stock lasts, plus the stock over the last
     46 minutes as a small chart.
   - A click on a good opens its page: the stock and the room for it, how well the inhabitants were
     supplied, the buildings making it, each flow with a bar, and a chart of the last hour or 12 hours
     (green in, red out, the stock as a gold line).
   - It is drawn in the colours of Open1602's other windows (dark ground, gold frame), not as
     parchment.

## Consequences

- Every new place that moves goods must book them; the data test `test_goods_data.cpp` runs games
  of the original for 20 minutes and checks that the bookings add up to what the cities hold, with
  goods in buildings counted and only those on carts left over (about 1 % of what moved).
- Old savegames load with empty statistics; the original's files have none.
- The section can grow by a version later (more flows, longer history) without breaking older
  savegames.
