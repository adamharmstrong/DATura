# FFXI Character Schedule Catalog

Generated from `DATura/character_dat_table.h` by `tools/scan_ffxi_schedules.ps1`.

Schedule chunks are type `0x07` records. `MotionRefs` are embedded tokens that match known motion display names from the animation catalog; `OtherRefs` are still-undecoded helper/action/effect tokens.

## Elvaan Female

### Skeleton + Base Tex - `ROM/42/4.dat`

| Schedule | MotionRefs | OtherRefs | Size | Offset |
| --- | --- | --- | ---: | ---: |
| `cafa` | `ms0` | `hwmg,nefa` | 192 | 576 |
| `ls11` | `gc2` | `lc11,stgc,eigc` | 272 | 768 |
| `ls10` | `gh2` | `lc10,stgh,eigh` | 272 | 1040 |
| `lc11` | `gc0,gc1` | `stgc,negc` | 288 | 1312 |
| `lc10` | `gh0,gh1` | `stgh,negh` | 288 | 1600 |
| `shge` | `` | `waso` | 144 | 1888 |
| `cage` | `` | `` | 128 | 2032 |
| `@tr0` | `wlk,idl` | `` | 192 | 2160 |
| `lc00` | `sk1` | `vsng` | 224 | 2352 |
| `init` | `` | `hwpc` | 128 | 2576 |
| `@tl0` | `wlk,idl` | `` | 192 | 2704 |
| `gurd` | `` | `` | 144 | 2896 |
| `damg` | `` | `chit,sdam,vdam` | 208 | 3040 |
| `chit` | `` | `se h,ef h` | 144 | 3248 |
| `sway` | `` | `vswy` | 128 | 3392 |
| `dead` | `ded,cor` | `vded` | 240 | 3520 |
| `pop0` | `` | `init` | 176 | 3760 |
| `corp` | `cor` | `` | 160 | 3936 |
| `res1` | `rx1` | `` | 160 | 4096 |
| `res0` | `rx0,rx1` | `` | 224 | 4256 |
| `shwh` | `` | `hwmg,stwh,eis4,sswh,wash` | 192 | 4480 |
| `shbk` | `` | `hwmg,stbk,eis3,ssbk,wash` | 192 | 4672 |
| `ssbk` | `mb1,mb2` | `` | 192 | 4864 |
| `ssit` | `mi2,mi3` | `` | 192 | 5056 |
| `sswh` | `mw1,mw2` | `` | 192 | 5248 |
| `ssso` | `mw1,mw2` | `` | 192 | 5440 |
| `lhit` | `` | `eflg,selg` | 144 | 5632 |
| `ldam` | `` | `lhit,sdam,vdam` | 208 | 5776 |
| `ls06` | `yu2` | `lc06,kalg,ldad,lgot` | 240 | 5984 |
| `lc06` | `yu0,yu1` | `lgin` | 224 | 6224 |
| `ls04` | `na2` | `lc04,hwso,kalg,ldad,chlg,lgot` | 272 | 6448 |
| `lc04` | `na0,na1` | `lgin` | 224 | 6720 |
| `ls03` | `gu2` | `lc03,kalg,ldad,lgot` | 240 | 6944 |
| `lc03` | `gu0,gu1` | `lgin` | 224 | 7184 |
| `shso` | `` | `stso,eis6,waso` | 176 | 7408 |
| `caso` | `` | `ner5` | 144 | 7584 |
| `ls00` | `sk2` | `lc00` | 224 | 7728 |
| `lc05` | `na0,na1` | `lgin` | 224 | 7952 |
| `ls05` | `na3` | `lc05,hwso,kalg,ldad` | 240 | 8176 |
| `pary` | `` | `` | 144 | 8416 |
| `kf&h` | `` | `sefl,sehp` | 144 | 8560 |
| `ls01` | `sf2` | `lc01,hwso,sefl` | 224 | 8704 |
| `ls02` | `sh2` | `lc02,hwso,sehp` | 224 | 8928 |
| `lc01` | `sf0,sf1` | `hwso,sefl` | 240 | 9152 |
| `lc02` | `sh0,sh1` | `hwso,sehp` | 240 | 9392 |
| `cabk` | `mb0` | `hwmg,ner1` | 192 | 9632 |
| `cawh` | `mw0` | `hwmg,ner2` | 192 | 9824 |
| `cast` | `mb0` | `` | 160 | 10016 |
| `res2` | `rx2` | `` | 192 | 10176 |
| `shit` | `` | `chit,hwmg,ssit,wash` | 176 | 10368 |
| `shlg` | `` | `hwso,wash` | 160 | 10544 |
| `calg` | `` | `hwso` | 144 | 10704 |
| `stnd` | `std` | `` | 160 | 10848 |
| `sssm` | `ms1,ms2` | `` | 192 | 11008 |
| `shsm` | `` | `hwmg,eis2,stsm,sssm,wash` | 192 | 11200 |
| `shnj` | `` | `hwmg,ner3,stnj,ssnj,wash` | 192 | 11392 |
| `ssnj` | `mn1,mn2` | `` | 192 | 11584 |
| `casm` | `ms0` | `hwmg,ner4` | 192 | 11776 |
| `canj` | `mn0` | `hwmg,sei5` | 192 | 11968 |
| `cait` | `mi0,mi1` | `hwmg` | 224 | 12160 |
| `sit0` | `si0,si1` | `` | 224 | 12384 |
| `sit1` | `si1` | `` | 160 | 12608 |
| `sit2` | `si2` | `` | 192 | 12768 |
| `gur1` | `` | `chit,sdam,vdam` | 192 | 12960 |
| `cabl` | `ma0` | `hwmg,neao` | 192 | 13152 |
| `ssbl` | `ma1,ma2` | `` | 192 | 13344 |
| `shbl` | `` | `hwmg,stbl,shao,ssbl,wash` | 192 | 13536 |
| `shfa` | `` | `hwmg,eifa,stfa,sssm,wash` | 192 | 13728 |

### Animation set 0 - `ROM/46/57.dat`

| Schedule | MotionRefs | OtherRefs | Size | Offset |
| --- | --- | --- | ---: | ---: |
| `ati2` | `at2` | `skaz,dada` | 224 | 32 |
| `atk0` | `` | `hwat,vatk` | 160 | 256 |
| `cnt0` | `` | `hwat,vswy` | 160 | 416 |
| `cnb0` | `amf` | `skaz,dcnt` | 208 | 576 |
| `cni0` | `at1` | `skaz,dcnt` | 224 | 784 |
| `cnr0` | `amr` | `skaz,dcnt` | 208 | 1008 |
| `cnl0` | `aml` | `skaz,dcnt` | 208 | 1216 |
| `atb0` | `amf` | `skaz,dada` | 208 | 1424 |
| `ati0` | `at0` | `skaz,dada` | 224 | 1632 |
| `atr0` | `amr` | `skaz,dada` | 208 | 1856 |
| `atl0` | `aml` | `skaz,dada` | 208 | 2064 |
| `out1` | `otf` | `sotr` | 208 | 2272 |
| `in 1` | `inf` | `sinr` | 208 | 2480 |
| `out0` | `ota` | `sotr` | 208 | 2688 |
| `in 0` | `ina` | `sinr` | 208 | 2896 |
| `atf0` | `amf` | `skaz,dada` | 208 | 3104 |
| `ati1` | `at1` | `skaz,dada` | 224 | 3312 |
| `cnf0` | `amf` | `skaz,dcnt` | 208 | 3536 |

### Animation set 1 - `ROM/46/75.dat`

| Schedule | MotionRefs | OtherRefs | Size | Offset |
| --- | --- | --- | ---: | ---: |
| `em07` | `wee` | `` | 160 | 32 |
| `em03` | `sl2` | `` | 160 | 192 |
| `em04` | `sl3` | `` | 160 | 352 |
| `em00` | `bow` | `` | 160 | 512 |
| `em06` | `lau` | `` | 160 | 672 |
| `em01` | `poi` | `` | 160 | 832 |
| `em02` | `sl1` | `` | 160 | 992 |
| `em05` | `kne` | `` | 160 | 1152 |

## Elvaan Male

### Skeleton + Base Tex - `ROM/37/31.dat`

| Schedule | MotionRefs | OtherRefs | Size | Offset |
| --- | --- | --- | ---: | ---: |
| `cafa` | `ms0` | `hwmg,nefa` | 192 | 576 |
| `gur1` | `` | `chit,sdam,vdam` | 192 | 768 |
| `ls05` | `na3` | `lc05,hwso,kalg,ldad` | 240 | 960 |
| `lc05` | `na0,na1` | `lgin` | 224 | 1200 |
| `ls06` | `yu2` | `lc06,kalg,ldad,lgot` | 240 | 1424 |
| `lc06` | `yu0,yu1` | `lgin` | 224 | 1664 |
| `ls04` | `na2` | `lc04,hwso,kalg,ldad,chlg,lgot` | 272 | 1888 |
| `lc04` | `na0,na1` | `lgin` | 224 | 2160 |
| `ls03` | `gu2` | `lc03,kalg,ldad,lgot` | 240 | 2384 |
| `lc03` | `gu0,gu1` | `lgin` | 224 | 2624 |
| `lc01` | `sf0,sf1` | `hwso,sefl` | 240 | 2848 |
| `ls01` | `sf2` | `lc01,hwso,sefl` | 224 | 3088 |
| `ls02` | `sh2` | `lc02,hwso,sehp` | 224 | 3312 |
| `lc02` | `sh0,sh1` | `hwso,sehp` | 240 | 3536 |
| `sit0` | `si0,si1` | `` | 224 | 3776 |
| `sit1` | `si1` | `` | 160 | 4000 |
| `sit2` | `si2` | `` | 192 | 4160 |
| `cait` | `mi0,mi1` | `hwmg` | 224 | 4352 |
| `canj` | `mn0` | `hwmg,sei5` | 192 | 4576 |
| `casm` | `ms0` | `hwmg,ner4` | 192 | 4768 |
| `ssnj` | `mn1,mn2` | `` | 192 | 4960 |
| `shnj` | `` | `hwmg,ner3,stnj,ssnj,wash` | 192 | 5152 |
| `shsm` | `` | `hwmg,eis2,stsm,sssm,wash` | 192 | 5344 |
| `sssm` | `ms1,ms2` | `` | 192 | 5536 |
| `stnd` | `std` | `` | 160 | 5728 |
| `calg` | `` | `hwso` | 144 | 5888 |
| `shlg` | `` | `hwso,wash` | 160 | 6032 |
| `shit` | `` | `chit,hwmg,ssit,wash` | 176 | 6192 |
| `res2` | `rx2` | `` | 192 | 6368 |
| `cast` | `mb0` | `` | 160 | 6560 |
| `cawh` | `mw0` | `hwmg,ner2` | 192 | 6720 |
| `cabk` | `mb0` | `hwmg,ner1` | 192 | 6912 |
| `kf&h` | `` | `sefl,sehp` | 144 | 7104 |
| `lc00` | `sk1` | `vsng` | 224 | 7248 |
| `ls00` | `sk2` | `lc00,ksng` | 240 | 7472 |
| `init` | `` | `hwpc` | 128 | 7712 |
| `@tr0` | `wlk,idl` | `` | 192 | 7840 |
| `@tl0` | `wlk,idl` | `` | 192 | 8032 |
| `gurd` | `` | `` | 144 | 8224 |
| `damg` | `` | `chit,sdam,vdam` | 208 | 8368 |
| `chit` | `` | `se h,ef h` | 144 | 8576 |
| `sway` | `` | `vswy` | 128 | 8720 |
| `dead` | `ded,cor` | `vded` | 240 | 8848 |
| `pop0` | `` | `init` | 176 | 9088 |
| `corp` | `cor` | `` | 160 | 9264 |
| `res1` | `rx1` | `` | 160 | 9424 |
| `res0` | `rx0,rx1` | `` | 224 | 9584 |
| `shwh` | `` | `hwmg,stwh,eis4,sswh,wash` | 192 | 9808 |
| `shbk` | `` | `hwmg,stbk,eis3,ssbk,wash` | 192 | 10000 |
| `ssbk` | `mb1,mb2` | `` | 192 | 10192 |
| `ssit` | `mi2,mi3` | `` | 192 | 10384 |
| `sswh` | `mw1,mw2` | `` | 192 | 10576 |
| `ssso` | `mw1,mw2` | `` | 192 | 10768 |
| `lhit` | `` | `eflg,selg` | 144 | 10960 |
| `ldam` | `` | `lhit,sdam,vdam` | 208 | 11104 |
| `shso` | `` | `stso,eis6,waso` | 176 | 11312 |
| `caso` | `` | `ner5` | 144 | 11488 |
| `pary` | `` | `` | 144 | 11632 |
| `cabl` | `ma0` | `neao,hwmg` | 192 | 11776 |
| `ssbl` | `ma1,ma2` | `` | 192 | 11968 |
| `shbl` | `` | `hwmg,stbl,shao,ssbl,wash` | 192 | 12160 |
| `cage` | `` | `` | 128 | 12352 |
| `shge` | `` | `waso` | 144 | 12480 |
| `ls10` | `gh2` | `lc10,stgh,eigh` | 272 | 12624 |
| `ls11` | `gc2` | `lc11,stgc,eigc` | 272 | 12896 |
| `lc10` | `gh0,gh1` | `stgh,negh` | 288 | 13168 |
| `lc11` | `gc0,gc1` | `stgc,negc` | 288 | 13456 |
| `shfa` | `` | `hwmg,eifa,stfa,sssm,wash` | 192 | 13744 |

### Animation set 1 - `ROM/41/114.dat`

| Schedule | MotionRefs | OtherRefs | Size | Offset |
| --- | --- | --- | ---: | ---: |
| `em02` | `sl1` | `` | 160 | 32 |
| `em01` | `poi` | `` | 160 | 192 |
| `em06` | `lau` | `` | 160 | 352 |
| `em00` | `bow` | `` | 160 | 512 |
| `em04` | `sl3` | `` | 160 | 672 |
| `em03` | `sl2` | `` | 160 | 832 |
| `em07` | `wee` | `` | 160 | 992 |
| `em05` | `kne` | `` | 160 | 1152 |

### Animation set 0 - `ROM/41/84.dat`

| Schedule | MotionRefs | OtherRefs | Size | Offset |
| --- | --- | --- | ---: | ---: |
| `bti0` | `at3` | `skaz,dada` | 224 | 32 |
| `cnb0` | `amb` | `skaz,dcnt` | 208 | 256 |
| `cnf0` | `amf` | `skaz,dcnt` | 208 | 464 |
| `cni0` | `at0` | `skaz,dcnt` | 224 | 672 |
| `cnr0` | `amr` | `skaz,dcnt` | 208 | 896 |
| `cnl0` | `aml` | `skaz,dcnt` | 208 | 1104 |
| `atb0` | `amb` | `skaz,dada` | 208 | 1312 |
| `atf0` | `amf` | `skaz,dada` | 208 | 1520 |
| `ati0` | `at0` | `skaz,dada` | 224 | 1728 |
| `ati1` | `at1` | `skaz,dada` | 224 | 1952 |
| `ati2` | `at2` | `skaz,dada` | 224 | 2176 |
| `atr0` | `amr` | `skaz,dada` | 208 | 2400 |
| `atl0` | `aml` | `skaz,dada` | 208 | 2608 |
| `out0` | `ota` | `sotr` | 208 | 2816 |
| `in 0` | `ina` | `sinr` | 208 | 3024 |
| `cnt0` | `` | `hwat,vswy` | 160 | 3232 |
| `atk0` | `` | `hwat,vatk` | 160 | 3392 |
| `in 1` | `ink` | `sinr` | 208 | 3552 |
| `out1` | `otk` | `sotr` | 208 | 3760 |
| `bti1` | `at4` | `skaz,dada` | 224 | 3968 |
| `dti0` | `wa5` | `skaz,dada` | 224 | 4192 |
| `cti0` | `wa4` | `skaz,dada` | 224 | 4416 |

## Galka

### Skeleton + Base Tex - `ROM/56/59.dat`

| Schedule | MotionRefs | OtherRefs | Size | Offset |
| --- | --- | --- | ---: | ---: |
| `cafa` | `ms0` | `hwmg,nefa` | 192 | 576 |
| `ls11` | `gc2` | `lc11,stgc,eigc` | 272 | 768 |
| `ls10` | `gh2` | `lc10,stgh,eigh` | 272 | 1040 |
| `lc11` | `gc0,gc1` | `stgc,negc` | 288 | 1312 |
| `lc10` | `gh0,gh1` | `stgh,negh` | 288 | 1600 |
| `shge` | `` | `waso` | 144 | 1888 |
| `cage` | `` | `` | 128 | 2032 |
| `ssbl` | `ma1,ma2` | `` | 192 | 2160 |
| `shbl` | `` | `hwmg,stbl,shao,ssbl,wash` | 192 | 2352 |
| `cabl` | `ma0` | `hwmg,neao` | 192 | 2544 |
| `pary` | `` | `` | 144 | 2736 |
| `caso` | `` | `ner5` | 144 | 2880 |
| `shso` | `` | `stso,eis6,waso` | 176 | 3024 |
| `ldam` | `` | `lhit,sdam,vdam` | 208 | 3200 |
| `lhit` | `` | `eflg,selg` | 144 | 3408 |
| `ssso` | `mw1,mw2` | `` | 192 | 3552 |
| `sswh` | `mw1,mw2` | `` | 192 | 3744 |
| `ssit` | `mi2,mi3` | `` | 192 | 3936 |
| `ssbk` | `mb1,mb2` | `` | 192 | 4128 |
| `shbk` | `` | `hwmg,stbk,eis3,ssbk,wash` | 192 | 4320 |
| `shwh` | `` | `hwmg,stwh,eis4,sswh,wash` | 192 | 4512 |
| `init` | `` | `hwpc` | 128 | 4704 |
| `@tr0` | `wlk,idl` | `` | 192 | 4832 |
| `@tl0` | `wlk,idl` | `` | 192 | 5024 |
| `gurd` | `` | `` | 144 | 5216 |
| `damg` | `` | `chit,sdam,vdam` | 208 | 5360 |
| `chit` | `` | `se h,ef h` | 144 | 5568 |
| `sway` | `` | `vswy` | 128 | 5712 |
| `dead` | `ded,cor` | `vded` | 240 | 5840 |
| `pop0` | `` | `init` | 176 | 6080 |
| `corp` | `cor` | `` | 160 | 6256 |
| `res1` | `rx1` | `` | 160 | 6416 |
| `ls00` | `sk2` | `lc00,ksng` | 240 | 6576 |
| `lc00` | `sk1` | `vsng` | 224 | 6816 |
| `kf&h` | `` | `sefl,sehp` | 144 | 7040 |
| `cabk` | `mb0` | `hwmg,ner1` | 192 | 7184 |
| `cawh` | `mw0` | `hwmg,ner2` | 192 | 7376 |
| `cast` | `mb0` | `` | 160 | 7568 |
| `res2` | `rx2` | `` | 192 | 7728 |
| `shit` | `` | `chit,hwmg,ssit,wash` | 176 | 7920 |
| `res0` | `rx0,rx1` | `` | 224 | 8096 |
| `shlg` | `` | `hwso,wash` | 160 | 8320 |
| `calg` | `` | `hwso` | 144 | 8480 |
| `stnd` | `std` | `` | 160 | 8624 |
| `sssm` | `ms1,ms2` | `` | 192 | 8784 |
| `shsm` | `` | `hwmg,eis2,stsm,sssm,wash` | 192 | 8976 |
| `shnj` | `` | `hwmg,ner3,stnj,ssnj,wash` | 192 | 9168 |
| `ssnj` | `mn1,mn2` | `` | 192 | 9360 |
| `casm` | `ms0` | `hwmg,ner4` | 192 | 9552 |
| `canj` | `mn0` | `hwmg,sei5` | 192 | 9744 |
| `cait` | `mi0,mi1` | `hwmg` | 224 | 9936 |
| `sit0` | `si0,si1` | `` | 224 | 10160 |
| `sit1` | `si1` | `` | 160 | 10384 |
| `sit2` | `si2` | `` | 192 | 10544 |
| `lc01` | `sf0,sf1` | `hwso,sefl` | 240 | 10736 |
| `ls01` | `sf2` | `lc01,hwso,sefl` | 224 | 10976 |
| `ls02` | `sh2` | `lc02,hwso,sehp` | 224 | 11200 |
| `lc02` | `sh0,sh1` | `hwso,sehp` | 240 | 11424 |
| `lc05` | `na0,na1` | `lgin` | 224 | 11664 |
| `ls05` | `na3` | `lc05,hwso,kalg,ldad` | 240 | 11888 |
| `lc03` | `gu0,gu1` | `lgin` | 224 | 12128 |
| `ls03` | `gu2` | `lc03,kalg,ldad,lgot` | 240 | 12352 |
| `lc04` | `na0,na1` | `lgin` | 224 | 12592 |
| `ls04` | `na2` | `lc04,hwso,kalg,ldad,chlg,lgot` | 272 | 12816 |
| `lc06` | `yu0,yu1` | `lgin` | 224 | 13088 |
| `ls06` | `yu2` | `lc06,kalg,ldad,lgot` | 240 | 13312 |
| `gur1` | `` | `chit,sdam,vdam` | 192 | 13552 |
| `shfa` | `` | `hwmg,stfa,eifa,sssm,wash` | 192 | 13744 |

### Animation set 0 - `ROM/60/112.dat`

| Schedule | MotionRefs | OtherRefs | Size | Offset |
| --- | --- | --- | ---: | ---: |
| `ati2` | `at4` | `skaz,dada` | 224 | 32 |
| `bti1` | `at3` | `skaz,dada` | 224 | 256 |
| `atk0` | `` | `hwat,vatk` | 160 | 480 |
| `cnt0` | `` | `hwat,vswy` | 160 | 640 |
| `cnf0` | `amf` | `skaz,dcnt` | 208 | 800 |
| `cnb0` | `amf` | `skaz,dcnt` | 208 | 1008 |
| `cni0` | `at2` | `skaz,dcnt` | 224 | 1216 |
| `cnr0` | `amr` | `skaz,dcnt` | 208 | 1440 |
| `cnl0` | `aml` | `skaz,dcnt` | 208 | 1648 |
| `atf0` | `amf` | `skaz,dada` | 208 | 1856 |
| `out1` | `ot0` | `sotr` | 208 | 2064 |
| `in 1` | `in0` | `sinr` | 208 | 2272 |
| `atb0` | `amf` | `skaz,dada` | 208 | 2480 |
| `ati0` | `at0` | `skaz,dada` | 224 | 2688 |
| `ati1` | `at1` | `skaz,dada` | 224 | 2912 |
| `bti0` | `at2` | `skaz,dada` | 224 | 3136 |
| `atr0` | `amr` | `skaz,dada` | 208 | 3360 |
| `atl0` | `aml` | `skaz,dada` | 208 | 3568 |
| `in 0` | `ina` | `sinr` | 208 | 3776 |
| `out0` | `ota` | `sotr` | 208 | 3984 |
| `dti0` | `wa5` | `skaz,dada` | 224 | 4192 |
| `cti0` | `wa4` | `skaz,dada` | 224 | 4416 |

### Animation set 1 - `ROM/61/8.dat`

| Schedule | MotionRefs | OtherRefs | Size | Offset |
| --- | --- | --- | ---: | ---: |
| `em02` | `sl1` | `` | 160 | 32 |
| `em01` | `poi` | `` | 160 | 192 |
| `em06` | `lau` | `` | 160 | 352 |
| `em00` | `bow` | `` | 160 | 512 |
| `em04` | `sl3` | `` | 160 | 672 |
| `em03` | `sl2` | `` | 160 | 832 |
| `em07` | `wee` | `` | 160 | 992 |
| `em05` | `kne` | `` | 160 | 1152 |

## Hume Female

### Skeleton + Base Tex - `ROM/32/58.dat`

| Schedule | MotionRefs | OtherRefs | Size | Offset |
| --- | --- | --- | ---: | ---: |
| `cafa` | `ms0` | `hwmg,nefa` | 192 | 576 |
| `ls10` | `gh2` | `lc10,stgh,eigh` | 272 | 768 |
| `lc11` | `gc0,gc1` | `stgc,negc` | 288 | 1040 |
| `gur1` | `` | `chit,sdam,vdam` | 192 | 1328 |
| `ls06` | `yu2` | `lc06,kalg,ldad,lgot` | 240 | 1520 |
| `lc06` | `yu0,yu1` | `lgin` | 224 | 1760 |
| `ls04` | `na2` | `lc04,hwso,kalg,ldad,chlg,lgot` | 272 | 1984 |
| `lc04` | `na0,na1` | `lgin` | 224 | 2256 |
| `ls03` | `gu2` | `lc03,kalg,ldad,lgot` | 240 | 2480 |
| `lc03` | `gu0,gu1` | `lgin` | 224 | 2720 |
| `lc05` | `na0,na1` | `lgin` | 224 | 2944 |
| `ls05` | `na3` | `lc05,hwso,kalg,ldad` | 240 | 3168 |
| `ls01` | `sf2` | `lc01,hwso,sefl` | 224 | 3408 |
| `ls02` | `sh2` | `lc02,hwso,sehp` | 224 | 3632 |
| `lc01` | `sf0,sf1` | `hwso,sefl` | 240 | 3856 |
| `lc02` | `sh0,sh1` | `hwso,sehp` | 240 | 4096 |
| `sit0` | `si0,si1` | `` | 224 | 4336 |
| `sit1` | `si1` | `` | 160 | 4560 |
| `sit2` | `si2` | `` | 192 | 4720 |
| `cait` | `mi0,mi1` | `hwmg` | 224 | 4912 |
| `canj` | `mn0` | `hwmg,sei5` | 192 | 5136 |
| `casm` | `ms0` | `hwmg,ner4` | 192 | 5328 |
| `ssnj` | `mn1,mn2` | `` | 192 | 5520 |
| `shnj` | `` | `hwmg,ner3,stnj,ssnj,wash` | 192 | 5712 |
| `shsm` | `` | `hwmg,eis2,stsm,sssm,wash` | 192 | 5904 |
| `sssm` | `ms1,ms2` | `` | 192 | 6096 |
| `stnd` | `std` | `` | 160 | 6288 |
| `calg` | `` | `hwso` | 144 | 6448 |
| `shlg` | `` | `hwso,wash` | 160 | 6592 |
| `shit` | `` | `chit,hwmg,ssit,wash` | 176 | 6752 |
| `res2` | `rx2` | `` | 192 | 6928 |
| `cast` | `mb0` | `` | 160 | 7120 |
| `cawh` | `mw0` | `hwmg,ner2` | 192 | 7280 |
| `cabk` | `mb0` | `hwmg,ner1` | 192 | 7472 |
| `kf&h` | `` | `sefl,sehp` | 144 | 7664 |
| `lc00` | `sk1` | `vsng` | 224 | 7808 |
| `ls00` | `sk2` | `lc00,ksng` | 240 | 8032 |
| `init` | `` | `hwpc` | 128 | 8272 |
| `@tr0` | `wlk,idl` | `` | 192 | 8400 |
| `@tl0` | `wlk,idl` | `` | 192 | 8592 |
| `gurd` | `` | `` | 144 | 8784 |
| `damg` | `` | `chit,sdam,vdam` | 208 | 8928 |
| `chit` | `` | `se h,ef h` | 144 | 9136 |
| `sway` | `` | `vswy` | 128 | 9280 |
| `dead` | `ded,cor` | `vded` | 240 | 9408 |
| `pop0` | `` | `init` | 176 | 9648 |
| `corp` | `cor` | `` | 160 | 9824 |
| `res1` | `rx1` | `` | 160 | 9984 |
| `res0` | `rx0,rx1` | `` | 224 | 10144 |
| `shwh` | `` | `hwmg,stwh,eis4,sswh,wash` | 192 | 10368 |
| `shbk` | `` | `hwmg,stbk,eis3,ssbk,wash` | 192 | 10560 |
| `ssbk` | `mb1,mb2` | `` | 192 | 10752 |
| `ssit` | `mi2,mi3` | `` | 192 | 10944 |
| `sswh` | `mw1,mw2` | `` | 192 | 11136 |
| `ssso` | `mw1,mw2` | `` | 192 | 11328 |
| `lhit` | `` | `eflg,selg` | 144 | 11520 |
| `ldam` | `` | `lhit,sdam,vdam` | 208 | 11664 |
| `shso` | `` | `stso,eis6,waso` | 176 | 11872 |
| `caso` | `` | `ner5` | 144 | 12048 |
| `pary` | `` | `` | 144 | 12192 |
| `cabl` | `ma0` | `hwmg,neao` | 192 | 12336 |
| `ssbl` | `ma1,ma2` | `` | 192 | 12528 |
| `shbl` | `` | `hwmg,stbl,shao,ssbl,wash` | 192 | 12720 |
| `cage` | `` | `` | 128 | 12912 |
| `ls11` | `gc2` | `lc11,eigc,stgc` | 272 | 13040 |
| `lc10` | `gh0,gh1` | `stgh,negh` | 288 | 13312 |
| `shge` | `` | `waso` | 144 | 13600 |
| `shfa` | `` | `hwmg,stfa,eifa,sssm,wash` | 192 | 13744 |

### Animation set 0 - `ROM/36/117.dat`

| Schedule | MotionRefs | OtherRefs | Size | Offset |
| --- | --- | --- | ---: | ---: |
| `cnl0` | `aml` | `skaz,dcnt` | 208 | 32 |
| `cnr0` | `amr` | `skaz,dcnt` | 208 | 240 |
| `cni0` | `at0` | `skaz,dcnt` | 224 | 448 |
| `cnf0` | `amf` | `skaz,dcnt` | 208 | 672 |
| `cnb0` | `amf` | `skaz,dcnt` | 208 | 880 |
| `in 0` | `ina` | `sinr` | 208 | 1088 |
| `out0` | `ota` | `sotr` | 208 | 1296 |
| `atl0` | `aml` | `skaz,dada` | 208 | 1504 |
| `atr0` | `amr` | `skaz,dada` | 208 | 1712 |
| `ati2` | `at2` | `skaz,dada` | 224 | 1920 |
| `ati1` | `at1` | `skaz,dada` | 224 | 2144 |
| `ati0` | `at0` | `skaz,dada` | 224 | 2368 |
| `atf0` | `amf` | `skaz,dada` | 208 | 2592 |
| `atb0` | `amb` | `skaz,dada` | 208 | 2800 |
| `cnt0` | `` | `hwat,vswy` | 160 | 3008 |
| `atk0` | `` | `hwat,vatk` | 160 | 3168 |

### Animation set 1 - `ROM/37/13.dat`

| Schedule | MotionRefs | OtherRefs | Size | Offset |
| --- | --- | --- | ---: | ---: |
| `em02` | `sl1` | `` | 160 | 32 |
| `em01` | `poi` | `` | 160 | 192 |
| `em06` | `lau` | `` | 160 | 352 |
| `em00` | `bow` | `` | 160 | 512 |
| `em04` | `sl3` | `` | 160 | 672 |
| `em03` | `sl2` | `` | 160 | 832 |
| `em07` | `wee` | `` | 160 | 992 |
| `em05` | `kne` | `` | 160 | 1152 |

## Hume Male

### Skeleton + Base Tex - `ROM/27/82.dat`

| Schedule | MotionRefs | OtherRefs | Size | Offset |
| --- | --- | --- | ---: | ---: |
| `shge` | `` | `waso` | 144 | 576 |
| `gur1` | `` | `chit,sdam,vdam` | 192 | 720 |
| `stnd` | `std` | `` | 160 | 912 |
| `calg` | `` | `hwso` | 144 | 1072 |
| `shlg` | `` | `hwso,wash` | 160 | 1216 |
| `shit` | `` | `chit,hwmg,ssit,wash` | 176 | 1376 |
| `res2` | `rx2` | `` | 192 | 1552 |
| `cast` | `mb0` | `` | 160 | 1744 |
| `cawh` | `mw0` | `hwmg,ner2` | 192 | 1904 |
| `cabk` | `mb0` | `hwmg,ner1` | 192 | 2096 |
| `kf&h` | `` | `sefl,sehp` | 144 | 2288 |
| `pary` | `` | `` | 144 | 2432 |
| `lhit` | `` | `eflg,selg` | 144 | 2576 |
| `ldam` | `` | `lhit,sdam,vdam` | 208 | 2720 |
| `ls06` | `yu2` | `lc06,hwso,kalg,ldad,lgot` | 256 | 2928 |
| `lc06` | `yu0,yu1` | `lgin` | 224 | 3184 |
| `ls04` | `na2` | `lc04,hwso,kalg,ldad,chlg,lgot` | 272 | 3408 |
| `lc04` | `na0,na1` | `lgin` | 224 | 3680 |
| `ls03` | `gu2` | `lc03,hwso,kalg,ldad,lgot` | 256 | 3904 |
| `lc03` | `gu0,gu1` | `lgin` | 224 | 4160 |
| `@tr0` | `wlk,idl` | `` | 192 | 4384 |
| `@tl0` | `wlk,idl` | `` | 192 | 4576 |
| `shso` | `` | `stso,eis6,waso` | 176 | 4768 |
| `gurd` | `` | `` | 144 | 4944 |
| `damg` | `` | `chit,sdam,vdam` | 208 | 5088 |
| `res1` | `rx1` | `` | 160 | 5296 |
| `res0` | `rx0,rx1` | `` | 224 | 5456 |
| `corp` | `cor` | `` | 160 | 5680 |
| `pop0` | `` | `init` | 176 | 5840 |
| `init` | `` | `hwpc` | 128 | 6016 |
| `casm` | `ms0` | `hwmg,ner4` | 192 | 6144 |
| `canj` | `mn0` | `hwmg,sei5` | 192 | 6336 |
| `shwh` | `` | `hwmg,stwh,eis4,sswh,wash` | 192 | 6528 |
| `shbk` | `` | `hwmg,stbk,eis3,ssbk,wash` | 192 | 6720 |
| `shsm` | `` | `hwmg,eis2,stsm,sssm,wash` | 192 | 6912 |
| `shnj` | `` | `hwmg,ner3,stnj,ssnj,wash` | 192 | 7104 |
| `caso` | `` | `ner5` | 144 | 7296 |
| `ssbk` | `mb1,mb2` | `` | 192 | 7440 |
| `ssit` | `mi2,mi3` | `` | 192 | 7632 |
| `ssnj` | `mn1,mn2` | `` | 192 | 7824 |
| `sssm` | `ms1,ms2` | `` | 192 | 8016 |
| `sswh` | `mw1,mw2` | `` | 192 | 8208 |
| `dead` | `ded,cor` | `vded` | 240 | 8400 |
| `sway` | `` | `vswy` | 128 | 8640 |
| `chit` | `` | `se h,ef h` | 144 | 8768 |
| `ls00` | `sk2` | `lc00,ksng` | 240 | 8912 |
| `ls01` | `sf2` | `lc01,hwso,sefl` | 224 | 9152 |
| `ls02` | `sh2` | `lc02,hwso,sehp` | 224 | 9376 |
| `lc05` | `na0,na1` | `lgin` | 224 | 9600 |
| `ls05` | `na3` | `lc05,hwso,kalg,ldad` | 240 | 9824 |
| `lc01` | `sf0,sf1` | `hwso,sefl` | 240 | 10064 |
| `lc02` | `sh0,sh1` | `hwso,sehp` | 240 | 10304 |
| `lc00` | `sk1` | `vsng` | 224 | 10544 |
| `cait` | `mi0,mi1` | `hwmg` | 224 | 10768 |
| `sit2` | `si2` | `` | 192 | 10992 |
| `sit1` | `si1` | `` | 160 | 11184 |
| `sit0` | `si0,si1` | `` | 224 | 11344 |
| `cabl` | `ma0` | `hwmg,neao` | 192 | 11568 |
| `shbl` | `` | `stbl,hwmg,shao,ssb1,wash` | 192 | 11760 |
| `ssb1` | `ma1,ma2` | `` | 192 | 11952 |
| `cage` | `` | `` | 128 | 12144 |
| `lc10` | `gh0,gh1` | `stgh,negh` | 288 | 12272 |
| `ls10` | `gh2` | `lc10,eigh,stgh` | 272 | 12560 |
| `lc11` | `gc0,gc1` | `stgc,negc` | 288 | 12832 |
| `ls11` | `gc2` | `lc11,stgc,eigc` | 272 | 13120 |
| `cafa` | `ms0` | `hwmg,nefa` | 192 | 13392 |
| `shfa` | `` | `eifa,hwmg,stfa,sssm,wash` | 192 | 13584 |

### Animation set 0 - `ROM/32/13.dat`

| Schedule | MotionRefs | OtherRefs | Size | Offset |
| --- | --- | --- | ---: | ---: |
| `out1` | `otb` | `sotr` | 208 | 32 |
| `in 1` | `inb` | `sinr` | 208 | 240 |
| `ati2` | `at2` | `skaz,dada` | 224 | 448 |
| `ati1` | `at1` | `skaz,dada` | 224 | 672 |
| `atb0` | `amb` | `skaz,dada` | 208 | 896 |
| `atf0` | `amb` | `skaz,dada` | 208 | 1104 |
| `ati0` | `at0` | `skaz,dada` | 224 | 1312 |
| `atr0` | `amr` | `skaz,dada` | 208 | 1536 |
| `atl0` | `aml` | `skaz,dada` | 208 | 1744 |
| `out0` | `otd` | `sotr` | 208 | 1952 |
| `in 0` | `ind` | `sinr` | 208 | 2160 |
| `cnb0` | `amb` | `skaz,dcnt` | 208 | 2368 |
| `cnf0` | `amb` | `skaz,dcnt` | 208 | 2576 |
| `cni0` | `at1` | `skaz,dcnt` | 224 | 2784 |
| `cnr0` | `amr` | `skaz,dcnt` | 208 | 3008 |
| `cnl0` | `aml` | `skaz,dcnt` | 208 | 3216 |
| `cnt0` | `` | `hwat,vswy` | 160 | 3424 |
| `atk0` | `` | `hwat,vatk` | 160 | 3584 |

### Animation set 1 - `ROM/32/40.dat`

| Schedule | MotionRefs | OtherRefs | Size | Offset |
| --- | --- | --- | ---: | ---: |
| `em02` | `sl1` | `` | 160 | 32 |
| `em01` | `poi` | `` | 160 | 192 |
| `em06` | `lau` | `` | 160 | 352 |
| `em00` | `bow` | `` | 160 | 512 |
| `em04` | `sl3` | `` | 160 | 672 |
| `em03` | `sl2` | `` | 160 | 832 |
| `em07` | `wee` | `` | 160 | 992 |
| `em05` | `kne` | `` | 160 | 1152 |

## Mithra

### Skeleton + Base Tex - `ROM/51/89.dat`

| Schedule | MotionRefs | OtherRefs | Size | Offset |
| --- | --- | --- | ---: | ---: |
| `cafa` | `ms0` | `hwmg,nefa` | 192 | 576 |
| `ls11` | `gc2` | `lc11,stgc,eigc` | 272 | 768 |
| `ls10` | `gh2` | `lc10,stgh,eigh` | 272 | 1040 |
| `lc11` | `gc0,gc1` | `stgc,negc` | 288 | 1312 |
| `lc10` | `gh0,gh1` | `stgh,negh` | 288 | 1600 |
| `shge` | `` | `waso` | 144 | 1888 |
| `cage` | `` | `` | 128 | 2032 |
| `shbl` | `` | `hwmg,stbl,shao,ssbl,wash` | 192 | 2160 |
| `ssbl` | `ma1,ma2` | `` | 192 | 2352 |
| `cabl` | `ma0` | `hwmg,neao` | 192 | 2544 |
| `pary` | `` | `` | 144 | 2736 |
| `caso` | `` | `ner5` | 144 | 2880 |
| `shso` | `` | `stso,eis6,waso` | 176 | 3024 |
| `ldam` | `` | `lhit,sdam,vdam` | 208 | 3200 |
| `lhit` | `` | `eflg,selg` | 144 | 3408 |
| `ssso` | `mw1,mw2` | `` | 192 | 3552 |
| `sswh` | `mw1,mw2` | `` | 192 | 3744 |
| `ssit` | `mi2,mi3` | `` | 192 | 3936 |
| `ssbk` | `mb1,mb2` | `` | 192 | 4128 |
| `shbk` | `` | `hwmg,stbk,eis3,ssbk,wash` | 192 | 4320 |
| `shwh` | `` | `hwmg,stwh,eis4,sswh,wash` | 192 | 4512 |
| `res0` | `rx0,rx1` | `` | 224 | 4704 |
| `init` | `` | `hwpc` | 128 | 4928 |
| `@tr0` | `wlk,idl` | `` | 192 | 5056 |
| `@tl0` | `wlk,idl` | `` | 192 | 5248 |
| `gurd` | `` | `` | 144 | 5440 |
| `damg` | `` | `chit,sdam,vdam` | 208 | 5584 |
| `chit` | `` | `se h,ef h` | 144 | 5792 |
| `sway` | `` | `vswy` | 128 | 5936 |
| `dead` | `ded,cor` | `vded` | 240 | 6064 |
| `pop0` | `` | `init` | 176 | 6304 |
| `corp` | `cor` | `` | 160 | 6480 |
| `res1` | `rx1` | `` | 160 | 6640 |
| `ls00` | `sk2` | `lc00,ksng` | 240 | 6800 |
| `lc00` | `sk1` | `vsng` | 224 | 7040 |
| `kf&h` | `` | `sefl,sehp` | 144 | 7264 |
| `cabk` | `mb0` | `hwmg,ner1` | 192 | 7408 |
| `cawh` | `mw0` | `hwmg,ner2` | 192 | 7600 |
| `cast` | `mb0` | `` | 160 | 7792 |
| `res2` | `rx2` | `` | 192 | 7952 |
| `shit` | `` | `chit,hwmg,ssit,wash` | 176 | 8144 |
| `shlg` | `` | `hwso,wash` | 160 | 8320 |
| `calg` | `` | `hwso` | 144 | 8480 |
| `stnd` | `std` | `` | 160 | 8624 |
| `sssm` | `ms1,ms2` | `` | 192 | 8784 |
| `shsm` | `` | `hwmg,eis2,stsm,sssm,wash` | 192 | 8976 |
| `shnj` | `` | `hwmg,ner3,stnj,ssnj,wash` | 192 | 9168 |
| `ssnj` | `mn1,mn2` | `` | 192 | 9360 |
| `casm` | `ms0` | `hwmg,ner4` | 192 | 9552 |
| `canj` | `mn0` | `hwmg,sei5` | 192 | 9744 |
| `cait` | `mi0,mi1` | `hwmg` | 224 | 9936 |
| `sit0` | `si0,si1` | `` | 224 | 10160 |
| `sit1` | `si1` | `` | 160 | 10384 |
| `sit2` | `si2` | `` | 192 | 10544 |
| `ls06` | `yu2` | `lc06,kalg,ldad,lgot` | 240 | 10736 |
| `lc06` | `yu0,yu1` | `lgin` | 224 | 10976 |
| `ls04` | `na2` | `lc04,hwso,kalg,ldad,chlg,lgot` | 272 | 11200 |
| `lc04` | `na0,na1` | `lgin` | 224 | 11472 |
| `ls03` | `gu2` | `lc03,kalg,ldad,lgot` | 240 | 11696 |
| `lc03` | `gu0,gu1` | `lgin` | 224 | 11936 |
| `lc05` | `na0,na1` | `lgin` | 224 | 12160 |
| `ls05` | `na3` | `lc05,hwso,kalg,ldad` | 240 | 12384 |
| `ls01` | `sf2` | `lc01,hwso,sefl` | 224 | 12624 |
| `ls02` | `sh2` | `lc02,hwso,sehp` | 224 | 12848 |
| `lc01` | `sf0,sf1` | `hwso,sefl` | 240 | 13072 |
| `lc02` | `sh0,sh1` | `hwso,sehp` | 240 | 13312 |
| `gur1` | `` | `chit,sdam,vdam` | 192 | 13552 |
| `shfa` | `` | `hwmg,eifa,stfa,sssm,wash` | 192 | 13744 |

### Animation set 0 - `ROM/56/14.dat`

| Schedule | MotionRefs | OtherRefs | Size | Offset |
| --- | --- | --- | ---: | ---: |
| `ati2` | `at2` | `skaz,dada` | 224 | 32 |
| `ati1` | `at1` | `skaz,dada` | 224 | 256 |
| `out0` | `ota` | `sotr` | 208 | 480 |
| `in 0` | `ina` | `sinr` | 208 | 688 |
| `atl0` | `aml` | `skaz,dada` | 208 | 896 |
| `atr0` | `amr` | `skaz,dada` | 208 | 1104 |
| `ati0` | `at0` | `skaz,dada` | 224 | 1312 |
| `atb0` | `amb` | `skaz,dada` | 208 | 1536 |
| `atf0` | `amf` | `skaz,dada` | 208 | 1744 |
| `cnl0` | `aml` | `skaz,dcnt` | 208 | 1952 |
| `cnr0` | `amr` | `skaz,dcnt` | 208 | 2160 |
| `cni0` | `at1` | `skaz,dcnt` | 224 | 2368 |
| `cnb0` | `amb` | `skaz,dcnt` | 208 | 2592 |
| `cnf0` | `amf` | `skaz,dcnt` | 208 | 2800 |
| `cnt0` | `` | `hwat,vswy` | 160 | 3008 |
| `atk0` | `` | `hwat,vatk` | 160 | 3168 |

### Animation set 1 - `ROM/56/41.dat`

| Schedule | MotionRefs | OtherRefs | Size | Offset |
| --- | --- | --- | ---: | ---: |
| `em05` | `kne` | `` | 160 | 32 |
| `em07` | `wee` | `` | 160 | 192 |
| `em03` | `sl2` | `` | 160 | 352 |
| `em04` | `sl3` | `` | 160 | 512 |
| `em00` | `bow` | `` | 160 | 672 |
| `em06` | `lau` | `` | 160 | 832 |
| `em01` | `poi` | `` | 160 | 992 |
| `em02` | `sl1` | `` | 160 | 1152 |

## Tarutaru Female

### Skeleton + Base Tex - `ROM/46/93.dat`

| Schedule | MotionRefs | OtherRefs | Size | Offset |
| --- | --- | --- | ---: | ---: |
| `cafa` | `ms0` | `hwmg,nefa` | 192 | 576 |
| `ls11` | `gc2` | `lc11,stgc,eigc` | 272 | 768 |
| `ls10` | `gh2` | `lc10,stgh,eigh` | 272 | 1040 |
| `lc10` | `gh0,gh1` | `stgh,negh` | 288 | 1312 |
| `lc11` | `gc0,gc1` | `stgc,negc` | 288 | 1600 |
| `shge` | `` | `waso` | 144 | 1888 |
| `cage` | `` | `` | 128 | 2032 |
| `ssbl` | `ma1,ma2` | `` | 192 | 2160 |
| `shbl` | `` | `hwmg,stbl,shao,ssbl,wash` | 192 | 2352 |
| `cabl` | `ma0` | `hwmg,neao` | 192 | 2544 |
| `pary` | `` | `` | 144 | 2736 |
| `caso` | `` | `ner5` | 144 | 2880 |
| `shso` | `` | `stso,eis6,waso` | 176 | 3024 |
| `ldam` | `` | `lhit,sdam,vdam` | 208 | 3200 |
| `lhit` | `` | `eflg,selg` | 144 | 3408 |
| `ssso` | `mw1,mw2` | `` | 192 | 3552 |
| `sswh` | `mw1,mw2` | `` | 192 | 3744 |
| `ssit` | `mi2,mi3` | `` | 192 | 3936 |
| `ssbk` | `mb1,mb2` | `` | 192 | 4128 |
| `shbk` | `` | `hwmg,stbk,eis3,ssbk,wash` | 192 | 4320 |
| `shwh` | `` | `hwmg,stwh,eis4,sswh,wash` | 192 | 4512 |
| `res0` | `rx0,rx1` | `` | 224 | 4704 |
| `res1` | `rx1` | `` | 160 | 4928 |
| `corp` | `cor` | `` | 160 | 5088 |
| `pop0` | `` | `init` | 176 | 5248 |
| `dead` | `ded,cor` | `vded` | 240 | 5424 |
| `sway` | `` | `vswy` | 128 | 5664 |
| `chit` | `` | `se h,ef h` | 144 | 5792 |
| `damg` | `` | `chit,sdam,vdam` | 208 | 5936 |
| `gurd` | `` | `` | 144 | 6144 |
| `@tl0` | `wlk,idl` | `` | 192 | 6288 |
| `@tr0` | `wlk,idl` | `` | 192 | 6480 |
| `init` | `` | `hwpc` | 128 | 6672 |
| `ls00` | `sk2` | `lc00,ksng` | 240 | 6800 |
| `lc00` | `sk1` | `vsng` | 224 | 7040 |
| `kf&h` | `` | `sefl,sehp` | 144 | 7264 |
| `cabk` | `mb0` | `hwmg,ner1` | 192 | 7408 |
| `cawh` | `mw0` | `hwmg,ner2` | 192 | 7600 |
| `cast` | `mb0` | `` | 160 | 7792 |
| `res2` | `rx2` | `` | 192 | 7952 |
| `shit` | `` | `chit,hwmg,ssit,wash` | 176 | 8144 |
| `shlg` | `` | `hwso,wash` | 160 | 8320 |
| `calg` | `` | `hwso` | 144 | 8480 |
| `stnd` | `std` | `` | 160 | 8624 |
| `sssm` | `ms1,ms2` | `` | 192 | 8784 |
| `shsm` | `` | `hwmg,eis2,stsm,sssm,wash` | 192 | 8976 |
| `shnj` | `` | `hwmg,ner3,stnj,ssnj,wash` | 192 | 9168 |
| `ssnj` | `mn1,mn2` | `` | 192 | 9360 |
| `casm` | `ms0` | `hwmg,ner4` | 192 | 9552 |
| `canj` | `mn0` | `hwmg,sei5` | 192 | 9744 |
| `cait` | `mi0,mi1` | `hwmg` | 224 | 9936 |
| `sit0` | `si0,si1` | `` | 224 | 10160 |
| `sit1` | `si1` | `` | 160 | 10384 |
| `sit2` | `si2` | `` | 192 | 10544 |
| `ls06` | `yu2` | `lc06,kalg,ldad,lgot` | 240 | 10736 |
| `lc06` | `yu0,yu1` | `lgin` | 224 | 10976 |
| `ls04` | `na2` | `lc04,hwso,kalg,ldad,chlg,lgot` | 272 | 11200 |
| `lc04` | `na0,na1` | `lgin` | 224 | 11472 |
| `ls03` | `gu2` | `lc03,kalg,ldad,lgot` | 240 | 11696 |
| `lc03` | `gu0,gu1` | `lgin` | 224 | 11936 |
| `lc05` | `na0,na1` | `lgin` | 224 | 12160 |
| `ls05` | `na3` | `lc05,hwso,kalg,ldad` | 240 | 12384 |
| `ls01` | `sf2` | `lc01,hwso,sefl` | 224 | 12624 |
| `ls02` | `sh2` | `lc02,hwso,sehp` | 224 | 12848 |
| `lc01` | `sf0,sf1` | `hwso,sefl` | 240 | 13072 |
| `lc02` | `sh0,sh1` | `hwso,sehp` | 240 | 13312 |
| `gur1` | `` | `chit,sdam,vdam` | 192 | 13552 |
| `shfa` | `` | `hwmg,eifa,stfa,sssm,wash` | 192 | 13744 |

### Animation set 0 - `ROM/51/19.dat`

| Schedule | MotionRefs | OtherRefs | Size | Offset |
| --- | --- | --- | ---: | ---: |
| `bti0` | `at3` | `skaz,dada` | 224 | 32 |
| `cnb0` | `amb` | `skaz,dcnt` | 208 | 256 |
| `cnf0` | `amf` | `skaz,dcnt` | 208 | 464 |
| `cni0` | `at0` | `skaz,dcnt` | 224 | 672 |
| `cnr0` | `amr` | `skaz,dcnt` | 208 | 896 |
| `cnl0` | `aml` | `skaz,dcnt` | 208 | 1104 |
| `out1` | `ot0` | `sotr` | 208 | 1312 |
| `in 1` | `in0` | `sinr` | 208 | 1520 |
| `atb0` | `amb` | `skaz,dada` | 208 | 1728 |
| `atf0` | `amf` | `skaz,dada` | 208 | 1936 |
| `ati0` | `at0` | `skaz,dada` | 224 | 2144 |
| `ati1` | `at1` | `skaz,dada` | 224 | 2368 |
| `ati2` | `at2` | `skaz,dada` | 224 | 2592 |
| `atr0` | `amr` | `skaz,dada` | 208 | 2816 |
| `atl0` | `aml` | `skaz,dada` | 208 | 3024 |
| `in 0` | `ina` | `sinr` | 208 | 3232 |
| `out0` | `ota` | `sotr` | 208 | 3440 |
| `cnt0` | `` | `hwat,vswy` | 160 | 3648 |
| `atk0` | `` | `hwat,vatk` | 160 | 3808 |
| `bti1` | `at4` | `skaz,dada` | 224 | 3968 |
| `dti0` | `wa5` | `skaz,dada` | 224 | 4192 |
| `cti0` | `wa4` | `skaz,dada` | 224 | 4416 |

### Animation set 1 - `ROM/51/71.dat`

| Schedule | MotionRefs | OtherRefs | Size | Offset |
| --- | --- | --- | ---: | ---: |
| `em07` | `wee` | `` | 160 | 32 |
| `em03` | `sl2` | `` | 160 | 192 |
| `em04` | `sl3` | `` | 160 | 352 |
| `em00` | `bow` | `` | 160 | 512 |
| `em06` | `lau` | `` | 160 | 672 |
| `em01` | `poi` | `` | 160 | 832 |
| `em02` | `sl1` | `` | 160 | 992 |
| `em05` | `kne` | `` | 160 | 1152 |

## Tarutaru Male

### Skeleton + Base Tex - `ROM/46/93.dat`

| Schedule | MotionRefs | OtherRefs | Size | Offset |
| --- | --- | --- | ---: | ---: |
| `cafa` | `ms0` | `hwmg,nefa` | 192 | 576 |
| `ls11` | `gc2` | `lc11,stgc,eigc` | 272 | 768 |
| `ls10` | `gh2` | `lc10,stgh,eigh` | 272 | 1040 |
| `lc10` | `gh0,gh1` | `stgh,negh` | 288 | 1312 |
| `lc11` | `gc0,gc1` | `stgc,negc` | 288 | 1600 |
| `shge` | `` | `waso` | 144 | 1888 |
| `cage` | `` | `` | 128 | 2032 |
| `ssbl` | `ma1,ma2` | `` | 192 | 2160 |
| `shbl` | `` | `hwmg,stbl,shao,ssbl,wash` | 192 | 2352 |
| `cabl` | `ma0` | `hwmg,neao` | 192 | 2544 |
| `pary` | `` | `` | 144 | 2736 |
| `caso` | `` | `ner5` | 144 | 2880 |
| `shso` | `` | `stso,eis6,waso` | 176 | 3024 |
| `ldam` | `` | `lhit,sdam,vdam` | 208 | 3200 |
| `lhit` | `` | `eflg,selg` | 144 | 3408 |
| `ssso` | `mw1,mw2` | `` | 192 | 3552 |
| `sswh` | `mw1,mw2` | `` | 192 | 3744 |
| `ssit` | `mi2,mi3` | `` | 192 | 3936 |
| `ssbk` | `mb1,mb2` | `` | 192 | 4128 |
| `shbk` | `` | `hwmg,stbk,eis3,ssbk,wash` | 192 | 4320 |
| `shwh` | `` | `hwmg,stwh,eis4,sswh,wash` | 192 | 4512 |
| `res0` | `rx0,rx1` | `` | 224 | 4704 |
| `res1` | `rx1` | `` | 160 | 4928 |
| `corp` | `cor` | `` | 160 | 5088 |
| `pop0` | `` | `init` | 176 | 5248 |
| `dead` | `ded,cor` | `vded` | 240 | 5424 |
| `sway` | `` | `vswy` | 128 | 5664 |
| `chit` | `` | `se h,ef h` | 144 | 5792 |
| `damg` | `` | `chit,sdam,vdam` | 208 | 5936 |
| `gurd` | `` | `` | 144 | 6144 |
| `@tl0` | `wlk,idl` | `` | 192 | 6288 |
| `@tr0` | `wlk,idl` | `` | 192 | 6480 |
| `init` | `` | `hwpc` | 128 | 6672 |
| `ls00` | `sk2` | `lc00,ksng` | 240 | 6800 |
| `lc00` | `sk1` | `vsng` | 224 | 7040 |
| `kf&h` | `` | `sefl,sehp` | 144 | 7264 |
| `cabk` | `mb0` | `hwmg,ner1` | 192 | 7408 |
| `cawh` | `mw0` | `hwmg,ner2` | 192 | 7600 |
| `cast` | `mb0` | `` | 160 | 7792 |
| `res2` | `rx2` | `` | 192 | 7952 |
| `shit` | `` | `chit,hwmg,ssit,wash` | 176 | 8144 |
| `shlg` | `` | `hwso,wash` | 160 | 8320 |
| `calg` | `` | `hwso` | 144 | 8480 |
| `stnd` | `std` | `` | 160 | 8624 |
| `sssm` | `ms1,ms2` | `` | 192 | 8784 |
| `shsm` | `` | `hwmg,eis2,stsm,sssm,wash` | 192 | 8976 |
| `shnj` | `` | `hwmg,ner3,stnj,ssnj,wash` | 192 | 9168 |
| `ssnj` | `mn1,mn2` | `` | 192 | 9360 |
| `casm` | `ms0` | `hwmg,ner4` | 192 | 9552 |
| `canj` | `mn0` | `hwmg,sei5` | 192 | 9744 |
| `cait` | `mi0,mi1` | `hwmg` | 224 | 9936 |
| `sit0` | `si0,si1` | `` | 224 | 10160 |
| `sit1` | `si1` | `` | 160 | 10384 |
| `sit2` | `si2` | `` | 192 | 10544 |
| `ls06` | `yu2` | `lc06,kalg,ldad,lgot` | 240 | 10736 |
| `lc06` | `yu0,yu1` | `lgin` | 224 | 10976 |
| `ls04` | `na2` | `lc04,hwso,kalg,ldad,chlg,lgot` | 272 | 11200 |
| `lc04` | `na0,na1` | `lgin` | 224 | 11472 |
| `ls03` | `gu2` | `lc03,kalg,ldad,lgot` | 240 | 11696 |
| `lc03` | `gu0,gu1` | `lgin` | 224 | 11936 |
| `lc05` | `na0,na1` | `lgin` | 224 | 12160 |
| `ls05` | `na3` | `lc05,hwso,kalg,ldad` | 240 | 12384 |
| `ls01` | `sf2` | `lc01,hwso,sefl` | 224 | 12624 |
| `ls02` | `sh2` | `lc02,hwso,sehp` | 224 | 12848 |
| `lc01` | `sf0,sf1` | `hwso,sefl` | 240 | 13072 |
| `lc02` | `sh0,sh1` | `hwso,sehp` | 240 | 13312 |
| `gur1` | `` | `chit,sdam,vdam` | 192 | 13552 |
| `shfa` | `` | `hwmg,eifa,stfa,sssm,wash` | 192 | 13744 |

### Animation set 0 - `ROM/51/19.dat`

| Schedule | MotionRefs | OtherRefs | Size | Offset |
| --- | --- | --- | ---: | ---: |
| `bti0` | `at3` | `skaz,dada` | 224 | 32 |
| `cnb0` | `amb` | `skaz,dcnt` | 208 | 256 |
| `cnf0` | `amf` | `skaz,dcnt` | 208 | 464 |
| `cni0` | `at0` | `skaz,dcnt` | 224 | 672 |
| `cnr0` | `amr` | `skaz,dcnt` | 208 | 896 |
| `cnl0` | `aml` | `skaz,dcnt` | 208 | 1104 |
| `out1` | `ot0` | `sotr` | 208 | 1312 |
| `in 1` | `in0` | `sinr` | 208 | 1520 |
| `atb0` | `amb` | `skaz,dada` | 208 | 1728 |
| `atf0` | `amf` | `skaz,dada` | 208 | 1936 |
| `ati0` | `at0` | `skaz,dada` | 224 | 2144 |
| `ati1` | `at1` | `skaz,dada` | 224 | 2368 |
| `ati2` | `at2` | `skaz,dada` | 224 | 2592 |
| `atr0` | `amr` | `skaz,dada` | 208 | 2816 |
| `atl0` | `aml` | `skaz,dada` | 208 | 3024 |
| `in 0` | `ina` | `sinr` | 208 | 3232 |
| `out0` | `ota` | `sotr` | 208 | 3440 |
| `cnt0` | `` | `hwat,vswy` | 160 | 3648 |
| `atk0` | `` | `hwat,vatk` | 160 | 3808 |
| `bti1` | `at4` | `skaz,dada` | 224 | 3968 |
| `dti0` | `wa5` | `skaz,dada` | 224 | 4192 |
| `cti0` | `wa4` | `skaz,dada` | 224 | 4416 |

### Animation set 1 - `ROM/51/37.dat`

| Schedule | MotionRefs | OtherRefs | Size | Offset |
| --- | --- | --- | ---: | ---: |
| `em02` | `sl1` | `` | 160 | 32 |
| `em01` | `poi` | `` | 160 | 192 |
| `em06` | `lau` | `` | 160 | 352 |
| `em00` | `bow` | `` | 160 | 512 |
| `em04` | `sl3` | `` | 160 | 672 |
| `em03` | `sl2` | `` | 160 | 832 |
| `em07` | `wee` | `` | 160 | 992 |
| `em05` | `kne` | `` | 160 | 1152 |

