# Zone weather database

`zone_weather_database.sqlite` inventories all 300 retail zone IDs. It combines
the server weather cycle with the weather resources authored in each installed
zone model DAT.

The two sources answer different questions:

- `zone_scheduled_weather` lists weather IDs that occur in a zone's repeating
  2,160 Vana'diel-day cycle. Use this table to decide which weather a zone
  naturally allows.
- `zone_authored_weather` lists the client `/weat` resource branches found in
  the zone model. Use this table to select the visual environment that DATura
  can render and to diagnose missing or shared assets.

The authored table also records PS2-relevant structure for every branch:
environment (`0x2F`) records and names, particle-generator counts, initially
active generators, weather-batched generators, rain-activatable generators,
and sound-resource counts. This mirrors the decompiled client's resource scan,
weather interpolation, and `ActivateRain`/`DeactivateRain` flow without
claiming that the client contains the server's natural-weather schedule.

`zones.schedule_status` and `zones.asset_status` preserve incomplete cases.
Five zone IDs have no known model DAT mapping, while every zone ID has a
schedule record in the inspected server dataset. A scheduled type need not have
a same-named local resource: some arenas inherit their surroundings, some
strength levels share one visual branch, and a few internal zones lack a known
model mapping.

Useful queries:

```sql
-- Zones in which rain or squalls can naturally occur.
SELECT z.zone_id, z.zone_name, w.weather_type
FROM zones AS z
JOIN zone_scheduled_weather AS zw USING (zone_id)
JOIN weather_types AS w USING (weather_id)
WHERE w.weather_type IN ('rain', 'squalls')
ORDER BY z.zone_id, w.weather_id;

-- Natural weather types for one zone.
SELECT w.weather_id, w.weather_type
FROM zone_scheduled_weather AS zw
JOIN weather_types AS w USING (weather_id)
WHERE zw.zone_id = 108
ORDER BY w.weather_id;
```

The CSV and JSON exports contain the same inventory in portable formats. Run
`python tools/generate_zone_weather_database.py` to rebuild all three files.
Pass `--ffxi-root`, `--zone-table`, or `--schedule-sql` when those sources are
stored elsewhere.
