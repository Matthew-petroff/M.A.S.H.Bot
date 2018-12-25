These segments are combinable as-is into a full movie file without any extra input "glue"
(barring sync issues). Use your favorite file concatenator or just run them one at a time.

_HeaderForTAS is required only if trying to run this in the DeSmuME emulator, otherwise it
can be omitted. It contains the TAS header information needed for playback.

_StartGame navigates the initial menus from power-on.

_ViewCredits backs out and views the in-game credits.

Level-specific content is in the Levelxxx folders. The first number in the segment file
name (01, 02, etc.) says what its order in the sequence is. If it's 02a, 02b, etc. it's
one of the four options we'll be presenting to Twitch, and exactly one of these should
be selected. A complete run through the level will consist of exactly one of each number.

Files marked 'extra' are completely new solutions that can replace one of the basic four
if one of them is having pervasive sync issues.

The final movie can mix and match levels or reorder them as needed; there aren't any
dependencies between levels.

Example movie sequence that completes levels 1-8 and 5-9 with the SHURIKEN and TIMELESS
MAGIC CARPET solutions and then goes to view the credits:
  _HeaderForTAS.txt
  _StartGame.txt
  Level1-8/01_EnterLevel1-8.txt
  Level1-8/02b_SHURIKEN.txt
  Level1-8/03_ExitLevel.txt
  Level5-9/01_EnterLevel5-9.txt
  Level5-9/02a_TIMELESS-MAGIC-CARPET.txt
  Level5-9/03_ExitLevel.txt
  _ViewCredits.txt