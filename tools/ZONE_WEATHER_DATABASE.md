# Zone weather database

`zone_weather_database.sqlite` inventories all 300 retail zone IDs. It combines
the server weather cycle with the weather resources authored in each installed
zone model DAT.

The two sources answer different questions:

- `zone_scheduled_weather` lists weather IDs that occur in a zone's repeating
  2,160 Vana'diel-day cycle. Use this table to decide which weather a zone
  naturally allows.
- `zone_weather_schedule` is that cycle itself: one row per non-zero
  LandSandBoat day entry with its normal/common/rare weather IDs. As in LSB's
  `WeatherContainer::entryForDay`, a day uses the most recent entry at or
  before it, and days before a zone's first entry have no weather.
- `zone_authored_weather` lists the client `/weat` resource branches found in
  the zone model. Use this table to select the visual environment that DATura
  can render and to diagnose missing or shared assets.
- `zone_authored_weather_sounds` lists every `0x3D` weather sound record: the
  HHMM record name, whether it sits in the branch's `indo` (indoor) set, and
  the SeWave ID with its `se/seNNN/seNNNNNN.spw` path.
- `zone_rain_branch_audit` classifies every authored rain and squall branch
  (see below).

The authored table also records PS2-relevant structure for every branch:
environment (`0x2F`) records and names, particle-generator counts, initially
active generators, weather-batched generators, rain-activatable generators,
and sound-resource counts. This mirrors the decompiled client's resource scan,
weather interpolation, and `ActivateRain`/`DeactivateRain` flow without
claiming that the client contains the server's natural-weather schedule.

`zones.schedule_status` and `zones.asset_status` preserve incomplete cases.
Five zone IDs have no known model DAT mapping, while every zone ID has a
schedule record in the inspected server dataset. A scheduled type need not have
a same-named local resource: some arenas inherit their surroundings and a few
internal zones lack a known model mapping. Of the 753 scheduled zone/weather
pairs in zones that author branches, 739 have a branch of the same family. The
other 14 fall back to `suny`, as the client's `YmCombineWeather` constructor
does: wind 5, thunderstorms 2, and one each of clouds, snow, blizzard, rain
(Phomiuna Aqueducts), gloom, thunder, and gales.

## Runtime schedule

The generator also writes `DATura/zone_weather_schedule.bin`, which the
post-build step copies next to `DATura.exe`. Layout (little-endian): `DWSC`,
version `1`, zone count, day count (2,160), then per zone a `u32` zone ID and
2,160 packed `u16` LSB day values (bits 10-14 normal, 5-9 common, 0-4 rare;
zero means "no new entry"). `ZoneWeatherSchedule::Table` reads it.

## Rain and squall branch audit

`zone_rain_branch_audit` (also exported as `zone_rain_branch_audit.csv`)
classifies each of the 73 rain/squall branches from the generator records,
flags, and `StandardParticleSetup` link types:

| Classification | Rain | Squalls | Meaning |
| --- | ---: | ---: | --- |
| `precipitation` | 35 | 7 | Autorun (`0x69 & 0x10`), weather-batched (`0x6B & 0x20`) emitters; DATura renders them. |
| `environment_only` | 19 | 4 | No `0x05` generators. Weather changes `0x2F` lighting/fog and plays its ambient loop only; these are indoor or cave zones. |
| `positional_sound_only` | 2 | 2 | Alzadaal Undersea Ruins (`squl`, `squ1`), Arrapago Remnants and Silver Sea Remnants (`rai1`): autorun generators link only `0x3D` positional sounds. |
| `weather_scenery_unsupported` | 2 | 2 | Sealion's Den (`rain`, `squl`), Cloister of Tides (`squl`), Hall of the Gods (`rain`): autorun generators link `0x0B` scenery meshes (sea, coral, cloud layers) and `0x39` lens records. The PS2 constructor activates them outdoors; DATura's weather pass draws only weather-batched emitters. |

None of the 31 zero-precipitation branches is a decoder or activation defect
for rain: none contains a weather-batched precipitation emitter. Every rain or
squall branch except Reisenjima Sanctorium's `d_hi/weat/rain` has a sound
record. Reisenjima has none, and Sealion's Den `rain`/`squl` author only an
indoor (`indo`) loop.

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

-- Weather that applies on cycle day 1000 in La Theine Plateau.
SELECT normal_weather_id, common_weather_id, rare_weather_id
FROM zone_weather_schedule
WHERE zone_id = 102 AND cycle_day <= 1000
ORDER BY cycle_day DESC LIMIT 1;

-- Why a rain/squall branch has no precipitation.
SELECT zone_name, resource_path, classification, cause
FROM zone_rain_branch_audit
WHERE classification <> 'precipitation';
```

The CSV and JSON exports contain the zone inventory in portable formats. Run
`python tools/generate_zone_weather_database.py` to rebuild the SQLite, CSV,
JSON, audit CSV, and runtime schedule in one step. Pass `--ffxi-root`,
`--zone-table`, `--schedule-sql`, or `--runtime-schedule` when those are stored
elsewhere. `metadata.schedule_sql_sha256` identifies the `zone_weather.sql`
input.
