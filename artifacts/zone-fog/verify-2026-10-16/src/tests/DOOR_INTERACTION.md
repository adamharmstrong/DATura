# Door interaction regression

Config > Mode / Debug > Door Interaction offers **Classic** (the default) and
**Physics**. Switching modes takes effect immediately without reloading the zone.

Physics uses independent hinged panels with angular velocity, damping, a gentle
return spring, and stops at +/-100 degrees. The player's collision cylinder
applies torque at the contact point before normal movement collision is resolved.
The simulation uses bounded 120 Hz substeps, and updates the same render transforms
and collision panels as Classic. Clicking can select a physics door but does not
open it. The Classic 15-second timer is replaced by the spring in Physics mode.

In Classic Game mode, click a visible door to select it, then click either leaf again
to open it. A selected door displays a label. Opening requires the player to
be within six horizontal world units. Escape or clicking elsewhere clears
selection. Doors automatically swing closed after about 15 seconds fully open.
Interacting with an open door restarts the timer; interacting during closing
reopens it along the same swing.

The implementation recognizes placed `door_` resources, keeps all their LOD
variants together, and rotates them about their hinges over half a second.
Paired leaves swing away from the player. Gates with a shared center origin
use their outer edges as hinges. This is a local hinge animation; no retail
event scripts, locks, or quest conditions are interpreted.

Visual collision ranges and tightly matching native collision panels rotate
with the door. Their spatial-index entries cover the swing, so the static
zone collision index does not need rebuilding each frame. Surrounding walls
and floors remain static. Picking tests visible triangles and rejects doors
behind opaque geometry.

Build `DoorInteractionTests.vcxproj` for Debug/x64, then run:

```powershell
& tests/bin/door-interaction/Debug/DoorInteractionTests.exe 'C:/Program Files (x86)/PlayOnline/SquareEnix/FINAL FANTASY XI/ROM/1/35.DAT'
```

Omit the DAT argument for synthetic tests only. Retail coverage checks the
30 Bastok Markets leaves, 15 pairs, full-zone picking, mirrored coordinates,
moving collision, clearance above the doorway sills, and actual movement
through all 15 open doorways from both sides using the production player
collision dimensions. Closed-door movement is also checked synthetically. The test does not
automate mouse input in the running application. Physics coverage includes player
traversal from both sides, independent leaves, angular momentum after contact,
vertical separation, hinge limits, settling, and mode transitions.
