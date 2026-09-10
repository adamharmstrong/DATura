# Player nameplate icons

Config > Appearance provides a temporary manual Player icon selector. Each option
uses a stable string ID from `FFXIPlayerIcons::Catalog`; gameplay status can choose
the same IDs later without changing texture composition. None clears the badge.
Job Master stars are an independent checkbox and may accompany any selection.
Changes apply immediately and save the nameplate controls in the loaded game UI config.
Linkshell RGB remains configurable in that file.

The Subtitle selector offers None, Linkshell name, and Jobs + levels. The detail
row edits the Linkshell name or separate main/subjob and level values. For example,
`WAR 99 / NIN 37` preserves the entered subjob level rather than deriving half of
the main level. No subjob omits the slash and second pair. An empty Linkshell name
produces no subtitle. Switching modes preserves the inactive values. Text edits
commit when the field loses focus. These local presentation fields can later be
supplied from each player's gameplay profile through the subtitle formatter.

## Retail artwork

All artwork is read locally from `ROM/119/51.DAT`, `menu ustatshd`. No extracted
image needs to ship. The fontshp table provides 32x32 source rectangles rendered
on 16x16 logical quads. The decoder returns BGRA with half-range FFXI alpha.
Downsampling averages premultiplied samples before source-over composition.

The catalog includes 35 choices plus None: the original categories, auto-party,
SGM/LGM, Developer/Producer, official PlayOnline staff, historical Tribune
correspondent, Campaign and its two party-seeking combinations, six Belligerency
rank states, and three Assist-chat Mentor flag previews. Only Linkshell is tinted.

- The former `busy` spelling aliases `auto-party`; the sprite is a red exclamation mark.
- Retail fontshp material offsets 7367, 7429, and 7491 all reference (96,96,32,32)
  with identical vertex colors. Consequently GM, SGM, and LGM remain separate
  selections but share the current installation's HD artwork.
- Tribune uses the quill at (64,128), not the nearby scales-of-justice sprite.
- Campaign/Gladiator use (128,96). Monipulators additionally use (160,96).
  CG/NM add one small star; HCG/HNM add two. Stars use (192,64).
- Campaign combinations keep both participation and recruitment visible in the
  same nameplate texture. The composition uses separate padded columns.
- Job Master uses three stars in a separate row above the name.
- Bronze (192,128), Silver (224,128), Gold (0,160) are explicitly labeled Assist-chat
  previews in the manual selector. They are not automatic overhead mentor grades.
  Chat Mastery Rank numbers belong to the future chat/profile presentation.

The selector does not infer icon priority or simulate online account permissions.
Unknown IDs and unavailable atlas data leave the text visible without a badge.

## References checked

- [FFXIclopedia: Player Icon Priority](https://ffxiclopedia.fandom.com/wiki/Player_Icon_Priority)
  documents the conventional categories and Campaign/recruitment exception.
- [Square Enix: historical staff icons](https://www.playonline.com/comnews/200211261346.html)
  identifies Trial, Tribune, PlayOnline staff, GM grades, and Producer artwork.
- [Square Enix: Monstrosity](https://www.playonline.com/ff11us/guide/monstrosity/index.html)
  includes illustrations of the sword/star badge and additional Monipulator symbol.
- [Square Enix: Assist Channel](https://www.playonline.com/ff11/guide/assistchannel/)
  identifies bronze/silver/gold as chat-name flags.
- [BG Wiki: Job Points](https://www.bg-wiki.com/ffxi/Gifts)
  describes three Job Master stars above the name.

## Verification

`NameplateRenderingTests` exercises every Config dropdown option, legacy ID
selection, checkbox synchronization, reopening, file persistence, unrelated-key
preservation, and save failure. Its D3D9 pass checks distinct composite/rank
variants, intrinsic red/blue channels, Linkshell-only tint, Job Master cache and
position isolation, inherited scene state, and foreground-depth occlusion.
It also produces an expanded gallery through the production nameplate renderer.
